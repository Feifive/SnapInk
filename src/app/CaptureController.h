#ifndef CAPTURECONTROLLER_H
#define CAPTURECONTROLLER_H

#include <QObject>
#include <QPointer>

class CaptureOverlay;
class PinWindowManager;
class QPoint;
class QWidget;

class CaptureController final : public QObject
{
    Q_OBJECT

public:
    explicit CaptureController(PinWindowManager* pinWindowManager,
                               QWidget* dialogParent = nullptr,
                               QObject* parent = nullptr);

    [[nodiscard]] bool isCapturing() const;

public slots:
    void startRegionCapture();
    void beginGlobalDragPinCapture(const QPoint& globalPos);
    void updateGlobalDragPinCapture(const QPoint& globalPos);
    void finishGlobalDragPinCapture(const QPoint& globalPos);
    void cancelGlobalDragPinCapture();

signals:
    void captureStarted();
    void captureFinished();
    void captureUnavailable();

private:
    void showOverlay(CaptureOverlay* overlay, bool activate = true);
    QPoint overlayLocalPointForGlobal(const QPoint& globalPos) const;
    void showCaptureUnavailable();

private:
    PinWindowManager* m_pinWindowManager = nullptr;
    QPointer<QWidget> m_dialogParent;
    QPointer<CaptureOverlay> m_activeOverlay;
};

#endif // CAPTURECONTROLLER_H
