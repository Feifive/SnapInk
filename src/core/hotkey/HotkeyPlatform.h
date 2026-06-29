#ifndef HOTKEYPLATFORM_H
#define HOTKEYPLATFORM_H

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QKeySequence>
#include <QString>

class HotkeyPrivate;

class HotkeyPlatform : public QAbstractNativeEventFilter
{
public:
    static HotkeyPlatform& instance();

    ~HotkeyPlatform() override;

    bool registerHotkey(HotkeyPrivate* hotkey,
                        const QKeySequence& sequence,
                        QString* errorReason = nullptr);
    void unregisterHotkey(HotkeyPrivate* hotkey);

    bool nativeEventFilter(const QByteArray& eventType,
                           void* message,
                           qintptr* result) override;

    /// Called by platform backends to dispatch a hotkey activation when the
    /// native id is known (e.g. from a Carbon event handler callback on
    /// macOS). Returns true if a Hotkey was found and activated.
    bool dispatchActivationByNativeId(int nativeId);

private:
    Q_DISABLE_COPY_MOVE(HotkeyPlatform)

    HotkeyPlatform();

    void ensureNativeEventFilterInstalled();
    int allocateNativeId();

    int m_nextNativeId = 1;
    bool m_eventFilterInstalled = false;
    QHash<int, HotkeyPrivate*> m_hotkeysByNativeId;
    QHash<HotkeyPrivate*, int> m_nativeIdsByHotkey;
};

#endif // HOTKEYPLATFORM_H
