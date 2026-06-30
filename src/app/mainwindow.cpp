#include "mainwindow.h"

#include "../core/capture/ScreenCaptureService.h"
#include "../core/hotkey/Hotkey.h"
#include "../core/hotkey/HotkeyConfig.h"
#include "../ui/capture/CaptureOverlay.h"
#ifdef Q_OS_MACOS
#include "../platform/macos/MacTrayIconHelper.h"
#endif

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

namespace {

QIcon loadSnapInkAppIcon(QString* sourceName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();
    const QStringList candidates = {
        QDir::cleanPath(appDir + QStringLiteral("/../Resources/app.icns")),
        QDir::cleanPath(appDir + QStringLiteral("/icons/app.icns")),
        QDir::cleanPath(appDir + QStringLiteral("/../icons/app.icns")),
        QDir::cleanPath(appDir + QStringLiteral("/../../icons/app.icns")),
        QDir::cleanPath(currentDir + QStringLiteral("/icons/app.icns")),
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            if (sourceName != nullptr) {
                *sourceName = QStringLiteral("app.icns");
            }
            return QIcon(candidate);
        }
    }

    if (sourceName != nullptr) {
        sourceName->clear();
    }
    return {};
}

} // namespace

MainWindow::MainWindow(QWidget *parent, bool registerGlobalHotkeys)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnapInk"));
    resize(360, 180);

    setupCentralWidget();
    setupTrayIcon();

    if (registerGlobalHotkeys) {
        registerHotkeys();
    }
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_MACOS
    MacTrayIconHelper::cleanupTemplateTrayIcon();
#endif
}

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

void MainWindow::setupTrayIcon()
{
    m_trayMenu = new QMenu(this);
    m_trayMenu->setObjectName(QStringLiteral("SnapInkTrayMenu"));

    QAction* regionCaptureAction = m_trayMenu->addAction(QStringLiteral("Region Capture"));
    connect(regionCaptureAction, &QAction::triggered, this, &MainWindow::startRegionCapture);

    m_trayMenu->addSeparator();

    QAction* showMainWindowAction = m_trayMenu->addAction(QStringLiteral("Show Main Window"));
    connect(showMainWindowAction, &QAction::triggered, this, &MainWindow::showMainWindow);

    QAction* quitAction = m_trayMenu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitApplication);

#ifdef Q_OS_MACOS
    const QString templateIconPath = MacTrayIconHelper::templateIconPath();
    if (!templateIconPath.isEmpty()) {
        setProperty("SnapInkTrayIconSource", QStringLiteral("trayTemplate.png"));
    }
    MacTrayIconHelper::configureTemplateTrayIcon(m_trayMenu, QStringLiteral("SnapInk"));
#else
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setObjectName(QStringLiteral("SnapInkTrayIcon"));
    m_trayIcon->setToolTip(QStringLiteral("SnapInk"));
    m_trayIcon->setContextMenu(m_trayMenu);

    QString iconSource;
    QIcon trayIcon = loadSnapInkAppIcon(&iconSource);
    if (!iconSource.isEmpty()) {
        m_trayIcon->setProperty("SnapInkIconSource", iconSource);
    }
    if (trayIcon.isNull()) {
        trayIcon = windowIcon();
    }
    if (trayIcon.isNull()) {
        trayIcon = QIcon::fromTheme(QStringLiteral("camera-photo"));
    }
    if (trayIcon.isNull()) {
        trayIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    m_trayIcon->setIcon(trayIcon);

    m_trayIcon->show();
#endif
}

void MainWindow::registerHotkeys()
{
    const QKeySequence regionShortcut = HotkeyConfig::regionCapture();
    m_regionHotkey = new Hotkey(regionShortcut, false, this);
    connect(m_regionHotkey, &Hotkey::activated, this, &MainWindow::startRegionCapture);
    connect(m_regionHotkey, &Hotkey::registrationFailed, this, [this, regionShortcut](const QString& reason) {
        handleHotkeyRegistrationFailure(regionShortcut.toString(QKeySequence::NativeText), reason);
    });
    m_regionHotkey->registerShortcut();
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
