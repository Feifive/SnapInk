#include "mainwindow.h"

#include "CaptureController.h"
#include "HotkeyController.h"
#include "../core/hotkey/HotkeyConfig.h"
#include "../ui/pin/PinWindowManager.h"
#include "../ui/tray/TrayController.h"

#include <QApplication>
#include <QCloseEvent>
#include <QLabel>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent, bool registerGlobalHotkeys)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnapInk"));
    resize(360, 180);

    setupCentralWidget();

    m_pinWindowManager = new PinWindowManager(this);

    m_captureController =
        new CaptureController(m_pinWindowManager, this, this);

    m_trayController = new TrayController(this);
    m_trayController->initialize();

    connect(m_trayController,
            &TrayController::regionCaptureRequested,
            m_captureController,
            &CaptureController::startRegionCapture);

    connect(m_trayController,
            &TrayController::closeAllPinsRequested,
            m_pinWindowManager,
            &PinWindowManager::closeAllPins);

    connect(m_trayController,
            &TrayController::showMainWindowRequested,
            this,
            &MainWindow::showMainWindow);

    connect(m_trayController,
            &TrayController::quitRequested,
            this,
            &MainWindow::quitApplication);

    connect(m_pinWindowManager,
            &PinWindowManager::pinsChanged,
            this,
            [this]() {
                m_trayController->setCloseAllPinsEnabled(
                    m_pinWindowManager->pinCount() > 0);
            });

    m_trayController->setCloseAllPinsEnabled(
        m_pinWindowManager->pinCount() > 0);

    m_hotkeyController = new HotkeyController(this, this);

    connect(m_hotkeyController,
            &HotkeyController::regionCaptureRequested,
            m_captureController,
            &CaptureController::startRegionCapture);

    if (registerGlobalHotkeys) {
        m_hotkeyController->registerShortcuts();
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_isQuitting) {
        event->accept();
        return;
    }

    hide();
    event->ignore();
}

void MainWindow::setupCentralWidget()
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    auto* label = new QLabel(QStringLiteral("SnapInk\n\n%1  Region capture")
                                .arg(HotkeyConfig::regionCaptureLabel()),
                             central);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    setCentralWidget(central);
}

void MainWindow::showMainWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::quitApplication()
{
    m_isQuitting = true;
    qApp->quit();
}
