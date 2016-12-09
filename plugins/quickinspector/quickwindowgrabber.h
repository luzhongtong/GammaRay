#ifndef GAMMARAY_QUICKINSPECTOR_QUICKWINDOWGRABBER_H
#define GAMMARAY_QUICKINSPECTOR_QUICKWINDOWGRABBER_H

#include <QObject>
#include <QPointer>

QT_BEGIN_NAMESPACE
#if defined(QT_QUICKWIDGETS_LIB)
class QQuickWidget;
#endif
class QQuickWindow;
QT_END_NAMESPACE

namespace GammaRay {
class OverlayItem;
class QuickWindowGrabber : public QObject
{
    Q_OBJECT

public:
    QuickWindowGrabber(QObject *parent = nullptr);
    ~QuickWindowGrabber() override;

    void setOverlayItem(OverlayItem *overlayItem);
    void setWindow(QQuickWindow *window);

#if defined(QT_QUICKWIDGETS_LIB)
    static void setWindowWidget(QQuickWidget *widget);
    static bool isQuickWidget(QQuickWindow *window);
    static QQuickWidget *quickWidget(QQuickWindow *window);
#endif

public slots:
    void requestGrabWindow();

signals:
    void sceneChanged();
    void sceneGrabbed(const QImage &image);

private:
    QPointer<QQuickWindow> m_window;
    QPointer<OverlayItem> m_overlayItem;
    bool m_filterNextSceneChange;

#if defined(QT_QUICKWIDGETS_LIB)
    static QImage grabQuickWidget(QQuickWidget *widget);
#endif
    static QImage grabQuickWindow(QQuickWindow *window);

private slots:
    void sceneRendered();
};
}

#endif
