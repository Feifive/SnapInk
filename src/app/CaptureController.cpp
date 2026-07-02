#include "CaptureController.h"

#include "core/capture/ScreenCaptureService.h"
#include "ui/capture/CaptureOverlay.h"
#include "ui/pin/PinWindowManager.h"

#include <QMessageBox>
#include <QtAssert>

CaptureController::CaptureController(PinWindowManager* pinWindowManager,
                                     QWidget* dialogParent,
                                     QObject* parent)
    : QObject(parent)
    , m_pinWindowManager(pinWindowManager)
    , m_dialogParent(dialogParent)
{
    Q_ASSERT(m_pinWindowManager != nullptr);
}

bool CaptureController::isCapturing() const
{
    return !m_activeOverlay.isNull();
}

void CaptureController::startRegionCapture()
{
    if (!m_activeOverlay.isNull()) {
        return;
    }

    const QRect virtualGeometry = ScreenCaptureService::virtualDesktopGeometry();
    CaptureResult captureResult = ScreenCaptureService::captureScreens();
    if (virtualGeometry.isEmpty() || captureResult.screens().isEmpty()) {
        showCaptureUnavailable();
        Q_EMIT captureUnavailable();
        return;
    }

    auto* overlay = new CaptureOverlay(std::move(captureResult), virtualGeometry);
    showOverlay(overlay);
}

void CaptureController::beginGlobalDragPinCapture(const QPoint& globalPos)
{
    if (!m_activeOverlay.isNull()) {
        return;
    }

    const QRect virtualGeometry = ScreenCaptureService::virtualDesktopGeometry();
    CaptureResult captureResult = ScreenCaptureService::captureScreens();
    if (virtualGeometry.isEmpty() || captureResult.screens().isEmpty()) {
        Q_EMIT captureUnavailable();
        return;
    }

    auto* overlay = new CaptureOverlay(std::move(captureResult),
                                       virtualGeometry,
                                       CaptureOverlayMode::GlobalDragPin);
    showOverlay(overlay, false);
    overlay->beginExternalSelection(overlayLocalPointForGlobal(globalPos));
}

void CaptureController::updateGlobalDragPinCapture(const QPoint& globalPos)
{
    if (m_activeOverlay.isNull()) {
        return;
    }

    m_activeOverlay->updateExternalSelection(overlayLocalPointForGlobal(globalPos));
}

void CaptureController::finishGlobalDragPinCapture(const QPoint& globalPos)
{
    if (m_activeOverlay.isNull()) {
        return;
    }

    m_activeOverlay->finishExternalSelectionAndPin(overlayLocalPointForGlobal(globalPos));
}

void CaptureController::cancelGlobalDragPinCapture()
{
    if (m_activeOverlay.isNull()) {
        return;
    }

    m_activeOverlay->cancelExternalSelection();
}

void CaptureController::showOverlay(CaptureOverlay* overlay, bool activate)
{
    if (overlay == nullptr) {
        return;
    }

    m_activeOverlay = overlay;

    connect(overlay, &QObject::destroyed, this, [this, overlay]() {
        if (m_activeOverlay == overlay) {
            m_activeOverlay.clear();
            Q_EMIT captureFinished();
        }
    });

    connect(overlay,
            &CaptureOverlay::pinRequested,
            m_pinWindowManager,
            &PinWindowManager::createPin);

    overlay->show();
    overlay->raise();
    if (activate) {
        overlay->activateWindow();
    }

    Q_EMIT captureStarted();
}

QPoint CaptureController::overlayLocalPointForGlobal(const QPoint& globalPos) const
{
    if (m_activeOverlay.isNull()) {
        return globalPos;
    }

    return globalPos - m_activeOverlay->geometry().topLeft();
}

void CaptureController::showCaptureUnavailable()
{
    QMessageBox::warning(m_dialogParent,
                         QStringLiteral("Capture Failed"),
                         QStringLiteral("Could not capture the current desktop."));
}
