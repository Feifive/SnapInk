#ifndef TRAYCONTROLLER_H
#define TRAYCONTROLLER_H

#include <QObject>

class QAction;
class QMenu;
#ifndef Q_OS_MACOS
class QSystemTrayIcon;
#endif

class TrayController final : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject* parent = nullptr);
    ~TrayController() override;

    /// Creates the tray menu and tray icon.
    /// Safe to call multiple times; repeated calls are no-ops.
    void initialize();

    /// Controls whether the "Close All Pins" menu item is enabled.
    void setCloseAllPinsEnabled(bool enabled);

    /// Test-only accessors.
    [[nodiscard]] QMenu* menu() const { return m_menu; }
    [[nodiscard]] QAction* closeAllPinsAction() const { return m_closeAllPinsAction; }
    [[nodiscard]] QAction* regionCaptureAction() const { return m_regionCaptureAction; }
    [[nodiscard]] QAction* showMainWindowAction() const { return m_showMainWindowAction; }
    [[nodiscard]] QAction* quitAction() const { return m_quitAction; }

signals:
    void regionCaptureRequested();
    void closeAllPinsRequested();
    void showMainWindowRequested();
    void quitRequested();

private:
    void setupMenu();
    void setupTrayIcon();

#ifndef Q_OS_MACOS
    QIcon loadAppIcon(QString* sourceName) const;
#endif

private:
    QMenu* m_menu = nullptr;

    QAction* m_regionCaptureAction = nullptr;
    QAction* m_closeAllPinsAction = nullptr;
    QAction* m_showMainWindowAction = nullptr;
    QAction* m_quitAction = nullptr;

#ifndef Q_OS_MACOS
    QSystemTrayIcon* m_trayIcon = nullptr;
#endif

    bool m_initialized = false;
};

#endif // TRAYCONTROLLER_H
