#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

class CaptureOverlay;
class QCloseEvent;
class QMenu;
class QSystemTrayIcon;
class Hotkey;
class QString;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, bool registerGlobalHotkeys = true);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupCentralWidget();
    void setupTrayIcon();
    void registerHotkeys();
    void handleHotkeyRegistrationFailure(const QString& shortcut, const QString& reason);
    void startRegionCapture();
    void showOverlay(CaptureOverlay* overlay);
    void showCaptureUnavailable();
    void showMainWindow();
    void quitApplication();

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    Hotkey* m_regionHotkey = nullptr;
    QPointer<CaptureOverlay> m_activeOverlay;
    bool m_isQuitting = false;
};
#endif // MAINWINDOW_H
