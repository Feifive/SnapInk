#ifndef HOTKEY_H
#define HOTKEY_H

#include <QObject>
#include <QKeySequence>

class HotkeyPrivate;

class Hotkey : public QObject
{
    Q_OBJECT

public:
    explicit Hotkey(QObject* parent = nullptr);
    explicit Hotkey(const QKeySequence& shortcut, bool autoRegister = false, QObject* parent = nullptr);

    ~Hotkey() override;

    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence& shortcut);

    bool isRegistered() const;

    bool registerShortcut();
    void unregisterShortcut();

signals:
    void activated();
    void registrationFailed(const QString& reason);

private:
    Q_DISABLE_COPY_MOVE(Hotkey)

    HotkeyPrivate* d;
};

#endif // HOTKEY_H
