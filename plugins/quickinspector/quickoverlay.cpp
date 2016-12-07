/*
  quickoverlay.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2010-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Filipe Azevedo <filipe.azevedo@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quickoverlay.h"

#include <QDebug>
#include <QEvent>
#include <QPainter>

using namespace GammaRay;

static QQuickItem *toplevelItem(QQuickItem *item)
{
    Q_ASSERT(item);
    return item->window()->contentItem();
}

static QPointF itemPos(QQuickItem *item)
{
    Q_ASSERT(item);
    return QPointF(item->x(), item->y());
}

static QSizeF itemSize(QQuickItem *item)
{
    Q_ASSERT(item);
    QSizeF size = QSizeF(item->width(), item->height());

    // Fallback to children rect if needed
    if (size.isNull()) {
        size = item->childrenRect().size();
    }

    return size;
}

static QRectF itemGeometry(QQuickItem *item)
{
    return QRectF(itemPos(item), itemSize(item));
}

static bool itemIsLayout(QQuickItem *item)
{
    Q_ASSERT(item);
    return item->inherits("QQuickLayout");
    //return qobject_cast<QQuickLayout *>(item);
}

QRectF ItemOrLayoutFacade::geometry() const
{
    return itemGeometry(isLayout() ? asLayout() : asItem());
}

bool ItemOrLayoutFacade::isVisible() const
{
    return item() ? item()->isVisible() /*&& !item()->isHidden()*/ : false;
}

QPointF ItemOrLayoutFacade::pos() const
{
    return isLayout() ? itemGeometry(asLayout()).topLeft() : QPoint(0, 0);
}

bool ItemOrLayoutFacade::isLayout() const
{
    return itemIsLayout(m_object);
}

OverlayItem::OverlayItem()
  : m_currentToplevelItem(0),
    m_drawLayoutOutlineOnly(true)
{
}

void OverlayItem::show()
{
    setVisible(true);
}

void OverlayItem::hide()
{
    setVisible(false);
}

void OverlayItem::move(qreal x, qreal y)
{
    setX(x);
    setY(y);
}

void OverlayItem::move(const QPointF &pos)
{
    move(pos.x(), pos.y());
}

void OverlayItem::resize(qreal width, qreal height)
{
    setWidth(width);
    setHeight(height);
}

void OverlayItem::resize(const QSizeF &size)
{
    resize(size.width(), size.height());
}

void OverlayItem::placeOn(ItemOrLayoutFacade item)
{
    if (item.isNull()) {
        if (!m_currentItem.isNull())
            disconnectItemChanges(m_currentItem.data());

        if (m_currentToplevelItem)
            disconnectTopItemChanges(m_currentToplevelItem);

        m_currentToplevelItem = nullptr;
        m_currentItem.clear();
        m_outerRect = QRect();
        m_layoutPath = QPainterPath();

        update();
        return;
    }

    if (!m_currentItem.isNull())
        disconnectItemChanges(m_currentItem.data());

    m_currentItem = item;

    QQuickItem *toplevel = toplevelItem(item.item());
    Q_ASSERT(toplevel);

    if (toplevel != m_currentToplevelItem) {
        if (m_currentToplevelItem)
            disconnectTopItemChanges(m_currentToplevelItem);

        m_currentToplevelItem = toplevel;

        setParentItem(toplevel);
        resizeOverlay();

        connectTopItemChanges(m_currentToplevelItem);

        show();
    }

    connectItemChanges(m_currentItem.data());

    updatePositions();
}

void OverlayItem::updatePositions()
{
    if (m_currentItem.isNull() || !m_currentToplevelItem)
        return;

    if (!m_currentItem.isVisible())
        m_outerRectColor = Qt::green;
    else
        m_outerRectColor = Qt::red;

    const QPointF parentPos = m_currentItem.item()->mapToItem(m_currentToplevelItem, m_currentItem.pos());
    m_outerRect = QRectF(parentPos.x(), parentPos.y(),
                        m_currentItem.geometry().width(),
                        m_currentItem.geometry().height()).adjusted(0, 0, -1, -1);

    m_layoutPath = QPainterPath();

    if (m_currentItem.layout()
        && qstrcmp(m_currentItem.layout()->metaObject()->className(),
                   "QMainWindowLayout") != 0) {
        const QRectF layoutGeometry = itemGeometry(m_currentItem.layout());

        const QRectF mappedOuterRect
            = QRectF(m_currentItem.item()->mapToItem(m_currentToplevelItem,
                                                  layoutGeometry.topLeft()), layoutGeometry.size());

        QPainterPath outerPath;
        outerPath.addRect(mappedOuterRect.adjusted(1, 1, -2, -2));

        QPainterPath innerPath;
        for (int i = 0; i < m_currentItem.layout()->itemCount(); ++i) {
            QQuickItem *item = m_currentItem.layout()->itemAt(i);
            if (!itemIsLayout(item) && item->isVisible())
                continue;
            const QRectF mappedInnerRect
                = QRectF(m_currentItem.item()->mapToItem(m_currentToplevelItem,
                                                      itemGeometry(item).topLeft()),
                        itemGeometry(item).size());
            innerPath.addRect(mappedInnerRect);
        }

        m_layoutPath.setFillRule(Qt::OddEvenFill);
        m_layoutPath = outerPath.subtracted(innerPath);

        if (m_layoutPath.isEmpty()) {
            m_layoutPath = outerPath;
            m_layoutPath.addPath(innerPath);
            m_drawLayoutOutlineOnly = true;
        } else {
            m_drawLayoutOutlineOnly = false;
        }
    }

    update();
}

void OverlayItem::resizeOverlay()
{
    if (m_currentToplevelItem) {
        setRotation(m_currentToplevelItem->rotation());
        setScale(m_currentToplevelItem->scale());
        setTransformOrigin(m_currentToplevelItem->transformOrigin());
        move(0, 0);
        resize(itemSize(m_currentToplevelItem));
    }
}

void OverlayItem::updateOverlay()
{
    resizeOverlay();
    updatePositions();
}

void OverlayItem::itemParentChanged(QQuickItem *parent)
{
    Q_UNUSED(parent);
    if (!m_currentItem.isNull())
        placeOn(m_currentItem);
}

void OverlayItem::itemWindowChanged(QQuickWindow *window)
{
    Q_UNUSED(window);
    if (!m_currentItem.isNull())
        placeOn(m_currentItem);
}

void OverlayItem::connectItemChanges(QQuickItem *item)
{
    connect(item, &QQuickItem::childrenRectChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::rotationChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::scaleChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::widthChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::heightChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::xChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::yChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::zChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::visibleChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::parentChanged, this, &OverlayItem::itemParentChanged);
    connect(item, &QQuickItem::windowChanged, this, &OverlayItem::itemWindowChanged);
}

void OverlayItem::disconnectItemChanges(QQuickItem *item)
{
    disconnect(item, &QQuickItem::childrenRectChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::rotationChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::scaleChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::widthChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::heightChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::xChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::yChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::zChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::visibleChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::parentChanged, this, &OverlayItem::itemParentChanged);
    disconnect(item, &QQuickItem::windowChanged, this, &OverlayItem::itemWindowChanged);
}

void OverlayItem::connectTopItemChanges(QQuickItem *item)
{
    connect(item, &QQuickItem::childrenRectChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::rotationChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::scaleChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::widthChanged, this, &OverlayItem::updateOverlay);
    connect(item, &QQuickItem::heightChanged, this, &OverlayItem::updateOverlay);
}

void OverlayItem::disconnectTopItemChanges(QQuickItem *item)
{
    disconnect(item, &QQuickItem::childrenRectChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::rotationChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::scaleChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::widthChanged, this, &OverlayItem::updateOverlay);
    disconnect(item, &QQuickItem::heightChanged, this, &OverlayItem::updateOverlay);
}

void OverlayItem::paint(QPainter *painter)
{
    QPainter &p(*painter);
    p.setPen(m_outerRectColor);
    p.drawRect(m_outerRect);

    QBrush brush(Qt::BDiagPattern);
    brush.setColor(Qt::blue);

    if (!m_drawLayoutOutlineOnly)
        p.fillPath(m_layoutPath, brush);

    p.setPen(Qt::blue);
    p.drawPath(m_layoutPath);
}
