#include "mainwindow.h"

#include "CaptureController.h"
#include "GlobalMouseController.h"
#include "HotkeyController.h"
#include "core/hotkey/HotkeyConfig.h"
#include "ui/pin/PinWindowManager.h"
#include "ui/tray/TrayController.h"

#include <QApplication>
#include <QCloseEvent>
#include <QLabel>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent, const bool registerGlobalHotkeys)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnapInk"));
    resize(360, 180);

    setupCentralWidget();

    init(registerGlobalHotkeys);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_isQuitting)
    {
        event->accept();
        return;
    }

    hide();
    event->ignore();
}

void MainWindow::init(const bool registerGlobalHotkeys)
{
    m_pinWindowManager  = new PinWindowManager(this);
    m_captureController = new CaptureController(m_pinWindowManager, this, this);
    m_trayController    = new TrayController(this);
    m_trayController->initialize();

    connect(m_trayController, &TrayController::regionCaptureRequested, m_captureController,
            &CaptureController::startRegionCapture);

    connect(m_trayController, &TrayController::closeAllPinsRequested, m_pinWindowManager,
            &PinWindowManager::closeAllPins);

    connect(m_trayController, &TrayController::showMainWindowRequested, this, &MainWindow::showMainWindow);

    connect(m_trayController, &TrayController::quitRequested, this, &MainWindow::quitApplication);

    connect(m_pinWindowManager, &PinWindowManager::pinsChanged, this, [this]()
    {
        m_trayController->setCloseAllPinsEnabled(m_pinWindowManager->pinCount() > 0);
    });

    m_trayController->setCloseAllPinsEnabled(m_pinWindowManager->pinCount() > 0);

    m_hotkeyController = new HotkeyController(this, this);
    m_globalMouseController = new GlobalMouseController(this, this);

    connect(m_hotkeyController, &HotkeyController::regionCaptureRequested, m_captureController,
            &CaptureController::startRegionCapture);

    connect(m_hotkeyController, &HotkeyController::restorePinRequested, m_pinWindowManager,
            &PinWindowManager::restoreLastClosedPin);

    connect(m_globalMouseController,
            &GlobalMouseController::dragStarted,
            m_captureController,
            &CaptureController::beginGlobalDragPinCapture);
    connect(m_globalMouseController,
            &GlobalMouseController::dragMoved,
            m_captureController,
            &CaptureController::updateGlobalDragPinCapture);
    connect(m_globalMouseController,
            &GlobalMouseController::dragFinished,
            m_captureController,
            &CaptureController::finishGlobalDragPinCapture);
    connect(m_globalMouseController,
            &GlobalMouseController::dragCanceled,
            m_captureController,
            &CaptureController::cancelGlobalDragPinCapture);

    if (registerGlobalHotkeys)
    {
        m_hotkeyController->registerShortcuts();
        m_globalMouseController->start();
    }
}

void MainWindow::setupCentralWidget()
{
    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    auto* label   = new QLabel(
        QStringLiteral("SnapInk\n\n%1  Region capture").arg(HotkeyConfig::regionCaptureLabel()), central);
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
