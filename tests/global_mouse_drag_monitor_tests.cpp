#include "core/globalmouse/GlobalMouseDragMonitor.h"

#include <QSignalSpy>
#include <QTest>

class GlobalMouseDragMonitorTests : public QObject
{
    Q_OBJECT

private slots:
    void nativeEventsEmitDragLifecycleSignals();
    void modifierReleaseEmitsFinished();
    void explicitCancelEmitsCanceled();
};

void GlobalMouseDragMonitorTests::nativeEventsEmitDragLifecycleSignals()
{
    GlobalMouseDragMonitor monitor;
    QSignalSpy startedSpy(&monitor, &GlobalMouseDragMonitor::dragStarted);
    QSignalSpy movedSpy(&monitor, &GlobalMouseDragMonitor::dragMoved);
    QSignalSpy finishedSpy(&monitor, &GlobalMouseDragMonitor::dragFinished);

    monitor.processNativeLeftButtonPress(QPoint(10, 20), true);
    monitor.processNativeMouseMove(QPoint(30, 40), true);
    monitor.processNativeLeftButtonRelease(QPoint(50, 60));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(movedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(startedSpy.takeFirst().at(0).toPoint(), QPoint(10, 20));
    QCOMPARE(movedSpy.takeFirst().at(0).toPoint(), QPoint(30, 40));
    QCOMPARE(finishedSpy.takeFirst().at(0).toPoint(), QPoint(50, 60));
}

void GlobalMouseDragMonitorTests::modifierReleaseEmitsFinished()
{
    GlobalMouseDragMonitor monitor;
    QSignalSpy canceledSpy(&monitor, &GlobalMouseDragMonitor::dragCanceled);
    QSignalSpy finishedSpy(&monitor, &GlobalMouseDragMonitor::dragFinished);

    monitor.processNativeLeftButtonPress(QPoint(10, 20), true);
    monitor.processNativeModifierChanged(QPoint(15, 25), false);
    monitor.processNativeLeftButtonRelease(QPoint(20, 30));

    QCOMPARE(canceledSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.takeFirst().at(0).toPoint(), QPoint(15, 25));
}

void GlobalMouseDragMonitorTests::explicitCancelEmitsCanceled()
{
    GlobalMouseDragMonitor monitor;
    QSignalSpy canceledSpy(&monitor, &GlobalMouseDragMonitor::dragCanceled);
    QSignalSpy finishedSpy(&monitor, &GlobalMouseDragMonitor::dragFinished);

    monitor.processNativeLeftButtonPress(QPoint(10, 20), true);
    monitor.processNativeCancel(QPoint(15, 25));

    QCOMPARE(canceledSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(canceledSpy.takeFirst().at(0).toPoint(), QPoint(15, 25));
}

QTEST_APPLESS_MAIN(GlobalMouseDragMonitorTests)

#include "global_mouse_drag_monitor_tests.moc"
