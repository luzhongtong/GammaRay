/*
  quickoverlay.h

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

#ifndef GAMMARAY_QUICKINSPECTOR_QUICKOVERLAY_H
#define GAMMARAY_QUICKINSPECTOR_QUICKOVERLAY_H

#include <QQuickPaintedItem>
#include <3rdparty/qt/5.5/private/qquicklayout_p.h>

class QQuickLayout;

namespace GammaRay {

class ItemOrLayoutFacade
{
public:
    ItemOrLayoutFacade() : m_object(nullptr) {}
    ItemOrLayoutFacade(QQuickItem *widget) : m_object(widget) {} //krazy:exclude=explicit
    ItemOrLayoutFacade(QQuickLayout *layout) : m_object(layout) {} //krazy:exclude=explicit

    /// Get either the layout of the widget or the this-pointer
    inline QQuickLayout *layout() const
    {
        return isLayout() ? asLayout() : nullptr;
    }

    /// Get either the parent widget of the layout or the this-pointer
    QQuickItem *item() const
    {
        return isLayout() ? asLayout()->parentItem() : asItem();
    }

    QRectF geometry() const;
    bool isVisible() const;
    QPointF pos() const;

    inline bool isNull() const { return !m_object; }
    inline QQuickItem* data() { return m_object; }
    inline QQuickItem* operator->() const { Q_ASSERT(!isNull()); return m_object; }
    inline void clear() { m_object = nullptr; }

private:
    bool isLayout() const;
    inline QQuickLayout *asLayout() const { return static_cast<QQuickLayout *>(m_object); }
    inline QQuickItem *asItem() const { return static_cast<QQuickItem *>(m_object); }

    QQuickItem *m_object;
};

class OverlayItem : public QQuickPaintedItem
{
    Q_OBJECT

public:
    OverlayItem();

    // C++ Helpers
    void show();
    void hide();
    void move(qreal x, qreal y);
    void move(const QPointF &pos);
    void resize(qreal width, qreal height);
    void resize(const QSizeF &size);

    /**
     * Place the overlay on @p item
     *
     * @param item The overlay can be cover a widget or a layout
     */
    void placeOn(ItemOrLayoutFacade item);

    void paint(QPainter *painter) Q_DECL_OVERRIDE;

private:
    void resizeOverlay();
    void updatePositions();
    void updateOverlay();
    void itemParentChanged(QQuickItem *parent);
    void itemWindowChanged(QQuickWindow *window);
    void connectItemChanges(QQuickItem *item);
    void disconnectItemChanges(QQuickItem *item);
    void connectTopItemChanges(QQuickItem *item);
    void disconnectTopItemChanges(QQuickItem *item);

    QQuickItem *m_currentToplevelItem;
    ItemOrLayoutFacade m_currentItem;
    QRectF m_outerRect;
    QColor m_outerRectColor;

    QPainterPath m_layoutPath;
    bool m_drawLayoutOutlineOnly;
};
}

#endif
