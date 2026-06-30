#include "PinWindowManager.h"
#include "PinWindow.h"

#include <QCoreApplication>
#include <QImage>
#include <QRect>

PinWindowManager::PinWindowManager(QObject* parent)
    : QObject(parent)
{
}

PinWindow* PinWindowManager::createPin(const QImage& image,
                                       const QRect& sourceGlobalRect)
{
    if (image.isNull()) {
        return nullptr;
    }

    auto* pinWindow = new PinWindow(image, sourceGlobalRect);
    registerPin(pinWindow);

    pinWindow->show();
    pinWindow->raise();
    activatePin(pinWindow);

    Q_EMIT pinCreated(pinWindow);
    return pinWindow;
}

int PinWindowManager::pinCount() const
{
    return m_pinWindows.size();
}

QList<PinWindow*> PinWindowManager::pinWindows() const
{
    QList<PinWindow*> result;
    result.reserve(m_pinWindows.size());
    for (const QPointer<PinWindow>& ptr : m_pinWindows) {
        if (!ptr.isNull()) {
            result.append(ptr.data());
        }
    }
    return result;
}

PinWindow* PinWindowManager::activePin() const
{
    return m_activePin.data();
}

void PinWindowManager::activatePin(PinWindow* pinWindow)
{
    if (pinWindow == nullptr) {
        return;
    }

    bool found = false;
    for (const QPointer<PinWindow>& candidate : m_pinWindows) {
        if (candidate.data() == pinWindow) {
            found = true;
            break;
        }
    }

    if (!found) {
        return;
    }

    if (m_activePin == pinWindow) {
        pinWindow->setActive(true);
        movePinToActivationTop(pinWindow);
        Q_EMIT pinsChanged();
        return;
    }

    if (!m_activePin.isNull()) {
        m_activePin->setActive(false);
    }

    m_activePin = pinWindow;
    movePinToActivationTop(pinWindow);
    pinWindow->setActive(true);

    Q_EMIT pinsChanged();
}

void PinWindowManager::closeAllPins()
{
    m_closingAllPins = true;

    clearActivePin();

    const QList<PinWindow*> windows = pinWindows();
    for (PinWindow* pinWindow : windows) {
        pinWindow->close();
    }

    m_closingAllPins = false;
}

void PinWindowManager::onPinFocusActivated(PinWindow* pinWindow)
{
    if (m_suppressFocusActivation) {
        return;
    }
    activatePin(pinWindow);
}

void PinWindowManager::onPinFocusDeactivated(PinWindow* pinWindow)
{
    if (m_closingAllPins) {
        return;
    }

    if (m_activePin != pinWindow) {
        return;
    }

    // Suppress focus-driven activation while we flush stale focus events
    // caused by the focus transfer away from this pin.
    m_suppressFocusActivation = true;

    pinWindow->setActive(false);
    m_activePin.clear();

    Q_EMIT pinsChanged();

    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    m_suppressFocusActivation = false;
}

void PinWindowManager::onPinAboutToClose(PinWindow* pinWindow)
{
    if (m_closingAllPins || pinWindow == nullptr) {
        return;
    }

    const bool wasActive = (m_activePin == pinWindow);

    // Remove the closing window from the history list early,
    // so activatePreviousAvailablePin() won't select it again.
    for (auto it = m_pinWindows.begin(); it != m_pinWindows.end();) {
        if (it->isNull() || it->data() == pinWindow) {
            it = m_pinWindows.erase(it);
        } else {
            ++it;
        }
    }

    if (!wasActive) {
        // The closing pin was not active. Suppress focus-driven activation
        // to prevent the window system's focus transfer from activating
        // another pin.
        m_suppressFocusActivation = true;
        Q_EMIT pinsChanged();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        m_suppressFocusActivation = false;
        return;
    }

    m_activePin.clear();
    activatePreviousAvailablePin();

    Q_EMIT pinsChanged();
}

void PinWindowManager::registerPin(PinWindow* pinWindow)
{
    m_pinWindows.append(pinWindow);

    connect(pinWindow,
            &PinWindow::focusActivated,
            this,
            &PinWindowManager::onPinFocusActivated);

    connect(pinWindow,
            &PinWindow::focusDeactivated,
            this,
            &PinWindowManager::onPinFocusDeactivated);

    connect(pinWindow,
            &PinWindow::aboutToClose,
            this,
            &PinWindowManager::onPinAboutToClose);

    connect(pinWindow,
            &QObject::destroyed,
            this,
            [this, pinWindow]() {
                removePin(pinWindow);
            });
}

void PinWindowManager::removePin(PinWindow* pinWindow)
{
    for (auto it = m_pinWindows.begin(); it != m_pinWindows.end();) {
        if (it->isNull() || it->data() == pinWindow) {
            it = m_pinWindows.erase(it);
        } else {
            ++it;
        }
    }

    if (m_activePin == pinWindow) {
        m_activePin.clear();
    }

    Q_EMIT pinsChanged();
}

void PinWindowManager::movePinToActivationTop(PinWindow* pinWindow)
{
    for (auto it = m_pinWindows.begin(); it != m_pinWindows.end(); ++it) {
        if (it->data() == pinWindow) {
            m_pinWindows.erase(it);
            break;
        }
    }
    m_pinWindows.append(pinWindow);
}

void PinWindowManager::activatePreviousAvailablePin()
{
    for (auto it = m_pinWindows.crbegin(); it != m_pinWindows.crend(); ++it) {
        if (it->isNull()) {
            continue;
        }

        PinWindow* previous = it->data();

        // Suppress focus-driven activation while we explicitly bring the
        // previous window to the foreground.
        m_suppressFocusActivation = true;

        previous->raise();
        previous->activateWindow();

        // Sync visual state immediately since some platforms delay focus events.
        activatePin(previous);

        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        m_suppressFocusActivation = false;
        return;
    }

    m_activePin.clear();
}

void PinWindowManager::clearActivePin()
{
    if (!m_activePin.isNull()) {
        m_activePin->setActive(false);
    }

    m_activePin.clear();
}
