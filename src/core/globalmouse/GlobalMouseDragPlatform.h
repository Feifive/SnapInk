#ifndef GLOBALMOUSEDRAGPLATFORM_H
#define GLOBALMOUSEDRAGPLATFORM_H

#include <QObject>
#include <QString>

class GlobalMouseDragMonitor;

class GlobalMouseDragPlatform : public QObject
{
    Q_OBJECT

public:
    explicit GlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor);
    ~GlobalMouseDragPlatform() override;

    virtual bool start(QString* errorReason) = 0;
    virtual void stop() = 0;

protected:
    GlobalMouseDragMonitor* monitor() const;

private:
    GlobalMouseDragMonitor* m_monitor = nullptr;
};

GlobalMouseDragPlatform* createGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor);

#endif // GLOBALMOUSEDRAGPLATFORM_H
