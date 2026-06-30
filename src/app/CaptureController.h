#ifndef CAPTURECONTROLLER_H
#define CAPTURECONTROLLER_H

#include <QObject>
#include <QPointer>

class CaptureOverlay;
class PinWindowManager;
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

signals:
    void captureStarted();
    void captureFinished();
    void captureUnavailable();

private:
    void showOverlay(CaptureOverlay* overlay);
    void showCaptureUnavailable();

private:
    PinWindowManager* m_pinWindowManager = nullptr;
    QPointer<QWidget> m_dialogParent;
    QPointer<CaptureOverlay> m_activeOverlay;
};

#endif // CAPTURECONTROLLER_H
