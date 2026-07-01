#include "GlobalMouseController.h"

#include "../core/globalmouse/GlobalMouseDragMonitor.h"

#include <QDebug>
#include <QGuiApplication>
#include <QMessageBox>

GlobalMouseController::GlobalMouseController(QWidget* dialogParent, QObject* parent)
    : QObject(parent)
    , m_monitor(new GlobalMouseDragMonitor(this))
    , m_dialogParent(dialogParent)
{
    connect(m_monitor,
            &GlobalMouseDragMonitor::dragStarted,
            this,
            &GlobalMouseController::dragStarted);
    connect(m_monitor,
            &GlobalMouseDragMonitor::dragMoved,
            this,
            &GlobalMouseController::dragMoved);
    connect(m_monitor,
            &GlobalMouseDragMonitor::dragFinished,
            this,
            &GlobalMouseController::dragFinished);
    connect(m_monitor,
            &GlobalMouseDragMonitor::dragCanceled,
            this,
            [this](const QPoint&) {
                Q_EMIT dragCanceled();
            });
    connect(m_monitor,
            &GlobalMouseDragMonitor::registrationFailed,
            this,
            &GlobalMouseController::handleRegistrationFailure);
}

GlobalMouseController::~GlobalMouseController()
{
    stop();
}

void GlobalMouseController::start()
{
    if (m_monitor != nullptr) {
        m_monitor->start();
    }
}

void GlobalMouseController::stop()
{
    if (m_monitor != nullptr) {
        m_monitor->stop();
    }
}

bool GlobalMouseController::isRunning() const
{
    return m_monitor != nullptr && m_monitor->isRunning();
}

void GlobalMouseController::handleRegistrationFailure(const QString& reason)
{
    qWarning().noquote() << QStringLiteral("Failed to register global mouse drag: %1").arg(reason);

    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        QMessageBox::warning(m_dialogParent,
                             QStringLiteral("Global Mouse Failed"),
                             reason);
    }

    Q_EMIT registrationFailed(reason);
}
