#ifndef GLOBALMOUSEDRAGMONITOR_H
#define GLOBALMOUSEDRAGMONITOR_H

#include "GlobalMouseDragTracker.h"

#include <QObject>
#include <QPoint>

#include <memory>

class GlobalMouseDragPlatform;

class GlobalMouseDragMonitor final : public QObject
{
    Q_OBJECT

public:
    explicit GlobalMouseDragMonitor(QObject* parent = nullptr);
    ~GlobalMouseDragMonitor() override;

    bool start();
    void stop();
    bool isRunning() const;

    void processNativeLeftButtonPress(const QPoint& globalPos, bool modifierDown);
    void processNativeMouseMove(const QPoint& globalPos, bool modifierDown);
    void processNativeLeftButtonRelease(const QPoint& globalPos);
    void processNativeModifierChanged(const QPoint& globalPos, bool modifierDown);
    void processNativeCancel(const QPoint& globalPos);

signals:
    void dragStarted(const QPoint& globalPos);
    void dragMoved(const QPoint& globalPos);
    void dragFinished(const QPoint& globalPos);
    void dragCanceled(const QPoint& globalPos);
    void registrationFailed(const QString& reason);

private:
    void dispatchTransition(const GlobalMouseDragTransition& transition);

    GlobalMouseDragTracker m_tracker;
    std::unique_ptr<GlobalMouseDragPlatform> m_platform;
    bool m_running = false;
};

#endif // GLOBALMOUSEDRAGMONITOR_H
