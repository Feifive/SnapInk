#ifndef HOTKEYCONTROLLER_H
#define HOTKEYCONTROLLER_H

#include <QObject>

class Hotkey;
class QString;
class QWidget;

class HotkeyController final : public QObject
{
    Q_OBJECT

public:
    explicit HotkeyController(QWidget* dialogParent = nullptr,
                              QObject* parent = nullptr);
    ~HotkeyController() override;

    void registerShortcuts();
    void unregisterShortcuts();

signals:
    void regionCaptureRequested();
    void shortcutRegistrationFailed(const QString& shortcut,
                                    const QString& reason);

private:
    void handleRegistrationFailure(const QString& shortcut,
                                   const QString& reason);

private:
    QWidget* m_dialogParent = nullptr;
    Hotkey* m_regionHotkey = nullptr;
    bool m_registered = false;
};

#endif // HOTKEYCONTROLLER_H
