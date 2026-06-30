#ifndef PINWINDOWMANAGER_H
#define PINWINDOWMANAGER_H

#include <QObject>
#include <QList>
#include <QPointer>

class PinWindow;
class QImage;
class QRect;

class PinWindowManager final : public QObject
{
    Q_OBJECT

public:
    explicit PinWindowManager(QObject* parent = nullptr);

    /// Create, show and activate a pin window.
    /// Returns nullptr when @p image is null.
    PinWindow* createPin(const QImage& image,
                         const QRect& sourceGlobalRect);

    [[nodiscard]] int pinCount() const;
    [[nodiscard]] QList<PinWindow*> pinWindows() const;
    [[nodiscard]] PinWindow* activePin() const;

    /// Set @p pinWindow as the current active pin and update visual highlight.
    void activatePin(PinWindow* pinWindow);

    /// Request to close all pin windows.
    void closeAllPins();

signals:
    void pinCreated(PinWindow* pinWindow);

    /// Emitted when the pin collection or active pin changes.
    void pinsChanged();

private slots:
    void onPinFocusActivated(PinWindow* pinWindow);
    void onPinFocusDeactivated(PinWindow* pinWindow);
    void onPinAboutToClose(PinWindow* pinWindow);

private:
    void registerPin(PinWindow* pinWindow);
    void removePin(PinWindow* pinWindow);

    void movePinToActivationTop(PinWindow* pinWindow);
    void activatePreviousAvailablePin();
    void clearActivePin();

    QList<QPointer<PinWindow>> m_pinWindows;
    QPointer<PinWindow> m_activePin;
    bool m_closingAllPins = false;
    bool m_suppressFocusActivation = false;
};

#endif // PINWINDOWMANAGER_H
