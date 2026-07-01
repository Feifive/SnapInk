#ifndef GLOBALMOUSECONTROLLER_H
#define GLOBALMOUSECONTROLLER_H

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

class GlobalMouseDragMonitor;
class QWidget;

class GlobalMouseController final : public QObject
{
    Q_OBJECT

public:
    explicit GlobalMouseController(QWidget* dialogParent = nullptr, QObject* parent = nullptr);
    ~GlobalMouseController() override;

    void start();
    void stop();
    bool isRunning() const;

signals:
    void dragStarted(const QPoint& globalPos);
    void dragMoved(const QPoint& globalPos);
    void dragFinished(const QPoint& globalPos);
    void dragCanceled();
    void registrationFailed(const QString& reason);

private:
    void handleRegistrationFailure(const QString& reason);

    GlobalMouseDragMonitor* m_monitor = nullptr;
    QPointer<QWidget> m_dialogParent;
};

#endif // GLOBALMOUSECONTROLLER_H
