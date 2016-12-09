#include "quickwindowgrabber.h"
#include "quickoverlay.h"

#if defined(QT_QUICKWIDGETS_LIB)
#include <QQuickWidget>
#endif
#include <QQuickWindow>

using namespace GammaRay;

QuickWindowGrabber::QuickWindowGrabber(QObject *parent)
    : QObject(parent)
    , m_filterNextSceneChange(false)
{
}

QuickWindowGrabber::~QuickWindowGrabber()
{
    setOverlayItem(nullptr);
    setWindow(nullptr);
}

void QuickWindowGrabber::setOverlayItem(OverlayItem *overlayItem)
{
    m_overlayItem = overlayItem;
}

void QuickWindowGrabber::setWindow(QQuickWindow *window)
{
    if (m_window == window)
        return;

    if (m_window) {
#if defined(QT_QUICKWIDGETS_LIB)
        if (quickWidget(m_window))
            disconnect(m_window, &QQuickWindow::afterRendering, m_window, &QQuickWindow::frameSwapped);
#endif
        disconnect(m_window, &QQuickWindow::frameSwapped, this, &QuickWindowGrabber::sceneRendered);
    }

    m_window = window;
    if (m_overlayItem) {
        m_overlayItem->setIsGrabbingMode(false);
    }

    if (m_window) {
#if defined(QT_QUICKWIDGETS_LIB)
        // frame swapped isn't enough, we don't get that for FBO render targets such as in QQuickWidget
        if (quickWidget(m_window))
            connect(m_window, &QQuickWindow::afterRendering, m_window, &QQuickWindow::frameSwapped);
#endif
        connect(m_window, &QQuickWindow::frameSwapped, this, &QuickWindowGrabber::sceneRendered);
    }
}

#if defined(QT_QUICKWIDGETS_LIB)
void QuickWindowGrabber::setWindowWidget(QQuickWidget *widget)
{
    Q_ASSERT(widget);
    widget->quickWindow()->setProperty("_gammaray_widget_", QVariant::fromValue(widget));
}

bool QuickWindowGrabber::isQuickWidget(QQuickWindow *window)
{
    Q_ASSERT(window);
    return window->property("_gammaray_widget_").isValid();
}

QQuickWidget *QuickWindowGrabber::quickWidget(QQuickWindow *window)
{
    Q_ASSERT(window);
    return window->property("_gammaray_widget_").value<QQuickWidget *>();
}
#endif

void QuickWindowGrabber::requestGrabWindow()
{
    Q_ASSERT(m_window);
    Q_ASSERT(m_overlayItem);

    if (m_overlayItem->isGrabbingMode())
        return;
    m_overlayItem->setIsGrabbingMode(true);
}

#if defined(QT_QUICKWIDGETS_LIB)
QImage QuickWindowGrabber::grabQuickWidget(QQuickWidget *widget)
{
    Q_ASSERT(widget);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    auto image = widget->grabFramebuffer();
    // See QTBUG-53795
    image.setDevicePixelRatio(widget->quickWindow()->effectiveDevicePixelRatio());
    return image;
#endif
    return QImage();
}
#endif

QImage QuickWindowGrabber::grabQuickWindow(QQuickWindow *window)
{
    Q_ASSERT(window);
#if QT_VERSION >= QT_VERSION_CHECK(5, 3, 0)
    auto image = window->grabWindow();
    // See QTBUG-53795
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    image.setDevicePixelRatio(window->effectiveDevicePixelRatio());
#elif QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    image.setDevicePixelRatio(window->devicePixelRatio());
#endif
    return image;
#endif
    return QImage();
}

void QuickWindowGrabber::sceneRendered()
{
    Q_ASSERT(m_window);
    Q_ASSERT(m_overlayItem);

    if (m_filterNextSceneChange) {
        m_filterNextSceneChange = false;
        return;
    }

    if (m_overlayItem->isGrabbingMode()) {
        QImage image;
        m_filterNextSceneChange = true;
#if defined(QT_QUICKWIDGETS_LIB)
        if (isQuickWidget(m_window)) {
            image = grabQuickWidget(quickWidget(m_window));
        }
        else
#endif
        {
            image = grabQuickWindow(m_window);
        }

        m_filterNextSceneChange = true;
        m_overlayItem->setIsGrabbingMode(false);

        if (!image.isNull())
            emit sceneGrabbed(image);
    }
    else {
        emit sceneChanged();
    }
}
