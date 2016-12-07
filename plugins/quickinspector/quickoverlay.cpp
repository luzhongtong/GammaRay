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
    return QSizeF(item->width(), item->height());
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
    /*setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);*/
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
            m_currentItem->removeEventFilter(this);

        if (m_currentToplevelItem)
            m_currentToplevelItem->removeEventFilter(this);

        m_currentToplevelItem = nullptr;
        m_currentItem.clear();
        m_outerRect = QRect();
        m_layoutPath = QPainterPath();

        update();
        return;
    }

    if (!m_currentItem.isNull())
        m_currentItem->removeEventFilter(this);

    m_currentItem = item;

    QQuickItem *toplevel = toplevelItem(item.item());
    Q_ASSERT(toplevel);

    if (toplevel != m_currentToplevelItem) {
        if (m_currentToplevelItem)
            m_currentToplevelItem->removeEventFilter(this);

        m_currentToplevelItem = toplevel;

        setParentItem(toplevel);
        move(0, 0);
        resize(toplevel->width(), toplevel->height());

        m_currentToplevelItem->installEventFilter(this);

        show();
    }

    m_currentItem->installEventFilter(this);

    updatePositions();
}

bool OverlayItem::eventFilter(QObject *receiver, QEvent *event)
{
    if (!m_currentItem.isNull() && m_currentToplevelItem->window() != m_currentItem.item()->window()) { // detect (un)docking
        placeOn(m_currentItem);
        return false;
    }

    if (receiver == m_currentItem.data()) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Show || event->type() == QEvent::Hide) {
            resizeOverlay();
            updatePositions();
        }
    } else if (receiver == m_currentToplevelItem) {
        if (event->type() == QEvent::Resize) {
            resizeOverlay();
            updatePositions();
        }
    }

    return false;
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
        move(0, 0);
        resize(itemSize(m_currentToplevelItem));
    }
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
