#include "mainwindow.h"

#include "../core/capture/ScreenCaptureService.h"
#include "../core/hotkey/Hotkey.h"
#include "../ui/capture/CaptureOverlay.h"

#include <QDebug>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnapInk"));
    resize(360, 180);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    auto* label = new QLabel(QStringLiteral("SnapInk\n\nCtrl+Alt+A  Region capture\nCtrl+Alt+S  Full screen capture"),
                             central);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    setCentralWidget(central);

    registerHotkeys();
}

MainWindow::~MainWindow() {}

void MainWindow::registerHotkeys()
{
    m_regionHotkey = new Hotkey(QKeySequence(QStringLiteral("Ctrl+Alt+A")), false, this);
    connect(m_regionHotkey, &Hotkey::activated, this, &MainWindow::startRegionCapture);
    connect(m_regionHotkey, &Hotkey::registrationFailed, this, [this](const QString& reason) {
        handleHotkeyRegistrationFailure(QStringLiteral("Ctrl+Alt+A"), reason);
    });
    m_regionHotkey->registerShortcut();

    m_fullScreenHotkey = new Hotkey(QKeySequence(QStringLiteral("Ctrl+Alt+S")), false, this);
    connect(m_fullScreenHotkey, &Hotkey::activated, this, &MainWindow::startFullScreenCapture);
    connect(m_fullScreenHotkey, &Hotkey::registrationFailed, this, [this](const QString& reason) {
        handleHotkeyRegistrationFailure(QStringLiteral("Ctrl+Alt+S"), reason);
    });
    m_fullScreenHotkey->registerShortcut();
}

void MainWindow::handleHotkeyRegistrationFailure(const QString& shortcut, const QString& reason)
{
    qWarning().noquote() << QStringLiteral("Failed to register %1: %2").arg(shortcut, reason);
    QMessageBox::warning(this,
                         QStringLiteral("Global Hotkey Failed"),
                         QStringLiteral("Could not register %1.\n%2").arg(shortcut, reason));
}

void MainWindow::startRegionCapture()
{
    if (!m_activeOverlay.isNull()) {
        return;
    }

    const QRect virtualGeometry = ScreenCaptureService::virtualDesktopGeometry();
    CaptureResult captureResult = ScreenCaptureService::captureScreens();
    if (virtualGeometry.isEmpty() || captureResult.screens().isEmpty()) {
        showCaptureUnavailable();
        return;
    }

    auto* overlay = new CaptureOverlay(std::move(captureResult), virtualGeometry);
    showOverlay(overlay);
}

void MainWindow::startFullScreenCapture()
{
    if (!m_activeOverlay.isNull()) {
        return;
    }

    const QRect virtualGeometry = ScreenCaptureService::virtualDesktopGeometry();
    CaptureResult captureResult = ScreenCaptureService::captureScreens();
    if (virtualGeometry.isEmpty() || captureResult.screens().isEmpty()) {
        showCaptureUnavailable();
        return;
    }

    auto* overlay = new CaptureOverlay(std::move(captureResult), virtualGeometry);
    overlay->enterEditing(QRect(QPoint(0, 0), virtualGeometry.size()));
    showOverlay(overlay);
}

void MainWindow::showOverlay(CaptureOverlay* overlay)
{
    if (overlay == nullptr) {
        return;
    }

    m_activeOverlay = overlay;
    connect(overlay, &QObject::destroyed, this, [this, overlay]() {
        if (m_activeOverlay == overlay) {
            m_activeOverlay.clear();
        }
    });

    overlay->show();
    overlay->raise();
    overlay->activateWindow();
}

void MainWindow::showCaptureUnavailable()
{
    QMessageBox::warning(this,
                         QStringLiteral("Capture Failed"),
                         QStringLiteral("Could not capture the current desktop."));
}
