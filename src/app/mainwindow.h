#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QCloseEvent;
class CaptureController;
class HotkeyController;
class PinWindowManager;
class TrayController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr, bool registerGlobalHotkeys = true);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void init(bool registerGlobalHotkeys);
    void setupCentralWidget();
    void showMainWindow();
    void quitApplication();

    TrayController* m_trayController       = nullptr;
    CaptureController* m_captureController = nullptr;
    HotkeyController* m_hotkeyController   = nullptr;
    PinWindowManager* m_pinWindowManager   = nullptr;
    bool m_isQuitting                      = false;
};
#endif // MAINWINDOW_H
