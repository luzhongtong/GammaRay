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
  : m_currentToplevelItem(0)
  , m_isGrabbingMode(false)
{
}

bool OverlayItem::isGrabbingMode() const
{
    return m_isGrabbingMode;
}

void OverlayItem::setIsGrabbingMode(bool isGrabbingMode)
{
    if (m_isGrabbingMode == isGrabbingMode)
        return;
    m_isGrabbingMode = isGrabbingMode;
    update();
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
        m_effectiveGeometry = QuickItemGeometry();

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

    m_effectiveGeometry.initFrom(m_currentItem.data());

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
    if (m_isGrabbingMode || m_currentItem.isNull())
        return;
    drawDecoration(painter, m_effectiveGeometry, QRectF(QPointF(), m_currentItem.data()->window()->size()), 1.0);
}

void OverlayItem::drawDecoration(QPainter *painter, const QuickItemGeometry &itemGeometry, const QRectF &viewRect,
                                 qreal zoom)
{
    painter->save();

    // bounding box
    painter->setPen(QColor(232, 87, 82, 170));
    painter->setBrush(QBrush(QColor(232, 87, 82, 95)));
    painter->drawRect(itemGeometry.boundingRect);

    // original geometry
    if (itemGeometry.itemRect != itemGeometry.boundingRect) {
        painter->setPen(Qt::gray);
        painter->setBrush(QBrush(Qt::gray, Qt::BDiagPattern));
        painter->drawRect(itemGeometry.itemRect);
    }

    // children rect
    if (itemGeometry.itemRect != itemGeometry.boundingRect
        && itemGeometry.transform.isIdentity()) {
        // If this item is transformed the children rect will be painted wrongly,
        // so for now skip painting it.
        painter->setPen(QColor(0, 99, 193, 170));
        painter->setBrush(QBrush(QColor(0, 99, 193, 95)));
        painter->drawRect(itemGeometry.childrenRect);
    }

    // transform origin
    if (itemGeometry.itemRect != itemGeometry.boundingRect) {
        painter->setPen(QColor(156, 15, 86, 170));
        painter->drawEllipse(itemGeometry.transformOriginPoint, 2.5, 2.5);
        painter->drawLine(itemGeometry.transformOriginPoint - QPointF(0, 6),
                    itemGeometry.transformOriginPoint + QPointF(0, 6));
        painter->drawLine(itemGeometry.transformOriginPoint - QPointF(6, 0),
                    itemGeometry.transformOriginPoint + QPointF(6, 0));
    }

    // x and y values
    painter->setPen(QColor(136, 136, 136));
    if (!itemGeometry.left
        && !itemGeometry.horizontalCenter
        && !itemGeometry.right
        && itemGeometry.x != 0) {
        QPointF parentEnd
            = (QPointF(itemGeometry.itemRect.x() - itemGeometry.x,
                       itemGeometry.itemRect.y()));
        QPointF itemEnd = itemGeometry.itemRect.topLeft();
        drawArrow(painter, parentEnd, itemEnd);
        painter->drawText(QRectF(parentEnd.x(), parentEnd.y() + 10, itemEnd.x() - parentEnd.x(), 50),
                    Qt::AlignHCenter | Qt::TextDontClip,
                    QStringLiteral("x: %1px").arg(itemGeometry.x / zoom));
    }
    if (!itemGeometry.top
        && !itemGeometry.verticalCenter
        && !itemGeometry.bottom
        && !itemGeometry.baseline
        && itemGeometry.y != 0) {
        QPointF parentEnd
            = (QPointF(itemGeometry.itemRect.x(),
                       itemGeometry.itemRect.y() - itemGeometry.y));
        QPointF itemEnd = itemGeometry.itemRect.topLeft();
        drawArrow(painter, parentEnd, itemEnd);
        painter->drawText(QRectF(parentEnd.x() + 10, parentEnd.y(), 100, itemEnd.y() - parentEnd.y()),
                    Qt::AlignVCenter | Qt::TextDontClip,
                    QStringLiteral("y: %1px").arg(itemGeometry.y / zoom));
    }

    // anchors
    if (itemGeometry.left) {
        drawAnchor(painter, itemGeometry, viewRect, zoom, Qt::Horizontal,
                   itemGeometry.itemRect.left(), itemGeometry.leftMargin,
                   QStringLiteral("margin: %1px").arg(itemGeometry.leftMargin / zoom));
    }

    if (itemGeometry.horizontalCenter) {
        drawAnchor(painter, itemGeometry, viewRect, zoom, Qt::Horizontal,
                   (itemGeometry.itemRect.left() + itemGeometry.itemRect.right()) / 2, itemGeometry.horizontalCenterOffset,
                   QStringLiteral("offset: %1px").arg(itemGeometry.horizontalCenterOffset / zoom));
    }

    if (itemGeometry.right) {
        drawAnchor(painter, itemGeometry, viewRect, zoom, Qt::Horizontal,
                   itemGeometry.itemRect.right(), -itemGeometry.rightMargin,
                   QStringLiteral("margin: %1px").arg(itemGeometry.rightMargin / zoom));
    }

    if (itemGeometry.top) {
        drawAnchor(painter, itemGeometry, viewRect, zoom, Qt::Vertical,
                   itemGeometry.itemRect.top(), itemGeometry.topMargin,
                   QStringLiteral("margin: %1px").arg(itemGeometry.topMargin / zoom));
    }

    if (itemGeometry.verticalCenter) {
        drawAnchor(painter, itemGeometry, viewRect, zoom, Qt::Vertical,
                   (itemGeometry.itemRect.top() + itemGeometry.itemRect.bottom()) / 2, itemGeometry.verticalCenterOffset,
                   QStringLiteral("offset: %1px").arg(itemGeometry.verticalCenterOffset / zoom));
    }

    if (itemGeometry.bottom) {
        drawAnchor(painter, itemGeometry, viewRect, zoom, Qt::Vertical,
                   itemGeometry.itemRect.bottom(), -itemGeometry.bottomMargin,
                   QStringLiteral("margin: %1px").arg(itemGeometry.bottomMargin / zoom));
    }

    if (itemGeometry.baseline) {
        drawAnchor(painter, itemGeometry, viewRect, zoom, Qt::Vertical,
                   itemGeometry.itemRect.top(), itemGeometry.baselineOffset,
                   QStringLiteral("offset: %1px").arg(itemGeometry.baselineOffset / zoom));
    }

    painter->restore();
}

void OverlayItem::drawArrow(QPainter *p, QPointF first, QPointF second)
{
    p->drawLine(first, second);
    QPointF vector(second - first);
    QMatrix m;
    m.rotate(30);
    QVector2D v1 = QVector2D(m.map(vector)).normalized() * 10;
    m.rotate(-60);
    QVector2D v2 = QVector2D(m.map(vector)).normalized() * 10;
    p->drawLine(first, first + v1.toPointF());
    p->drawLine(first, first + v2.toPointF());
    p->drawLine(second, second - v1.toPointF());
    p->drawLine(second, second - v2.toPointF());
}

void OverlayItem::drawAnchor(QPainter *p, const QuickItemGeometry &itemGeometry, const QRectF &viewRect, qreal zoom,
                             Qt::Orientation orientation, qreal ownAnchorLine, qreal offset, const QString &label)
{
    qreal foreignAnchorLine = ownAnchorLine - offset;
    QPen pen(QColor(139, 179, 0));

    // Margin arrow
    if (offset) {
        p->setPen(pen);
        if (orientation == Qt::Horizontal) {
            drawArrow(p,
                      QPointF(foreignAnchorLine,
                              (itemGeometry.itemRect.top()
                               + itemGeometry.itemRect.bottom()) / 2),
                      QPointF(ownAnchorLine,
                              (itemGeometry.itemRect.top()
                               + itemGeometry.itemRect.bottom()) / 2));
        } else {
            drawArrow(p,
                      QPointF((itemGeometry.itemRect.left()
                               + itemGeometry.itemRect.right()) / 2, foreignAnchorLine),
                      QPointF((itemGeometry.itemRect.left()
                               + itemGeometry.itemRect.right()) / 2, ownAnchorLine));
        }

        // Margin text
        if (orientation == Qt::Horizontal) {
            p->drawText(
                QRectF(foreignAnchorLine,
                       (itemGeometry.itemRect.top()
                        + itemGeometry.itemRect.bottom()) / 2 + 10, offset, 50),
                Qt::AlignHCenter | Qt::TextDontClip,
                label);
        } else {
            p->drawText(
                QRectF((itemGeometry.itemRect.left()
                        + itemGeometry.itemRect.right()) / 2 + 10,
                       foreignAnchorLine, 100, offset),
                Qt::AlignVCenter | Qt::TextDontClip,
                label);
        }
    }

    // Own Anchor line
    pen.setWidth(2);
    p->setPen(pen);
    if (orientation == Qt::Horizontal)
        p->drawLine(ownAnchorLine,
                    itemGeometry.itemRect.top(), ownAnchorLine,
                    itemGeometry.itemRect.bottom());
    else
        p->drawLine(
            itemGeometry.itemRect.left(), ownAnchorLine,
            itemGeometry.itemRect.right(), ownAnchorLine);

    // Foreign Anchor line
    pen.setStyle(Qt::DotLine);
    p->setPen(pen);
    if (orientation == Qt::Horizontal)
        p->drawLine(foreignAnchorLine, 0, foreignAnchorLine, viewRect.height() * zoom);
    else
        p->drawLine(0, foreignAnchorLine, viewRect.width() * zoom, foreignAnchorLine);
}
