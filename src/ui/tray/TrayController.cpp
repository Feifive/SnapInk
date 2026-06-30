#include "TrayController.h"

#ifdef Q_OS_MACOS
#include "../../platform/macos/MacTrayIconHelper.h"
#endif

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QStyle>
#ifndef Q_OS_MACOS
#include <QSystemTrayIcon>
#endif

TrayController::TrayController(QObject* parent)
    : QObject(parent)
{
}

TrayController::~TrayController()
{
    delete m_menu;
#ifdef Q_OS_MACOS
    MacTrayIconHelper::cleanupTemplateTrayIcon();
#endif
}

void TrayController::initialize()
{
    if (m_initialized) {
        return;
    }

    setupMenu();
    setupTrayIcon();

    setCloseAllPinsEnabled(false);

    m_initialized = true;
}

void TrayController::setCloseAllPinsEnabled(bool enabled)
{
    if (m_closeAllPinsAction != nullptr) {
        m_closeAllPinsAction->setEnabled(enabled);
    }
}

void TrayController::setupMenu()
{
    m_menu = new QMenu();
    m_menu->setObjectName(QStringLiteral("SnapInkTrayMenu"));

    m_regionCaptureAction = m_menu->addAction(QStringLiteral("Region Capture"));
    m_regionCaptureAction->setObjectName(QStringLiteral("SnapInkRegionCaptureAction"));
    connect(m_regionCaptureAction, &QAction::triggered, this, &TrayController::regionCaptureRequested);

    m_closeAllPinsAction = m_menu->addAction(QStringLiteral("Close All Pins"));
    m_closeAllPinsAction->setObjectName(QStringLiteral("SnapInkCloseAllPinsAction"));
    connect(m_closeAllPinsAction, &QAction::triggered, this, &TrayController::closeAllPinsRequested);

    m_menu->addSeparator();

    m_showMainWindowAction = m_menu->addAction(QStringLiteral("Show Main Window"));
    m_showMainWindowAction->setObjectName(QStringLiteral("SnapInkShowMainWindowAction"));
    connect(m_showMainWindowAction, &QAction::triggered, this, &TrayController::showMainWindowRequested);

    m_quitAction = m_menu->addAction(QStringLiteral("Quit"));
    m_quitAction->setObjectName(QStringLiteral("SnapInkQuitAction"));
    connect(m_quitAction, &QAction::triggered, this, &TrayController::quitRequested);
}

void TrayController::setupTrayIcon()
{
#ifdef Q_OS_MACOS
    const QString templateIconPath = MacTrayIconHelper::templateIconPath();
    if (!templateIconPath.isEmpty()) {
        setProperty("SnapInkTrayIconSource", QStringLiteral("trayTemplate.png"));
    }
    MacTrayIconHelper::configureTemplateTrayIcon(m_menu, QStringLiteral("SnapInk"));
#else
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setObjectName(QStringLiteral("SnapInkTrayIcon"));
    m_trayIcon->setToolTip(QStringLiteral("SnapInk"));
    m_trayIcon->setContextMenu(m_menu);

    QString iconSource;
    QIcon trayIcon = loadAppIcon(&iconSource);
    if (!iconSource.isEmpty()) {
        m_trayIcon->setProperty("SnapInkIconSource", iconSource);
    }
    if (trayIcon.isNull()) {
        trayIcon = QApplication::windowIcon();
    }
    if (trayIcon.isNull()) {
        trayIcon = QIcon::fromTheme(QStringLiteral("camera-photo"));
    }
    if (trayIcon.isNull()) {
        trayIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    m_trayIcon->setIcon(trayIcon);

    m_trayIcon->show();
#endif
}

#ifndef Q_OS_MACOS
QIcon TrayController::loadAppIcon(QString* sourceName) const
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
#endif
