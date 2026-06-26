#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

class CaptureOverlay;
class Hotkey;
class QString;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void registerHotkeys();
    void handleHotkeyRegistrationFailure(const QString& shortcut, const QString& reason);
    void startRegionCapture();
    void startFullScreenCapture();
    void showOverlay(CaptureOverlay* overlay);
    void showCaptureUnavailable();

    Hotkey* m_regionHotkey = nullptr;
    Hotkey* m_fullScreenHotkey = nullptr;
    QPointer<CaptureOverlay> m_activeOverlay;
};
#endif // MAINWINDOW_H
