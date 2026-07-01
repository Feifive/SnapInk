#include "../src/core/globalmouse/GlobalMouseDragTracker.h"

#include <QPoint>
#include <QTest>

class GlobalMouseDragTrackerTests : public QObject
{
    Q_OBJECT

private slots:
    void leftPressWithModifierStartsDrag();
    void leftPressWithoutModifierDoesNothing();
    void moveDuringDragEmitsMove();
    void moveAfterModifierReleaseFinishes();
    void releasingDuringDragFinishes();
    void modifierReleaseDuringDragFinishes();
};

void GlobalMouseDragTrackerTests::leftPressWithModifierStartsDrag()
{
    GlobalMouseDragTracker tracker;

    const GlobalMouseDragTransition transition =
        tracker.handleLeftButtonPress(QPoint(10, 20), true);

    QCOMPARE(transition.type, GlobalMouseDragTransitionType::Started);
    QCOMPARE(transition.position, QPoint(10, 20));
    QVERIFY(tracker.isDragging());
}

void GlobalMouseDragTrackerTests::leftPressWithoutModifierDoesNothing()
{
    GlobalMouseDragTracker tracker;

    const GlobalMouseDragTransition transition =
        tracker.handleLeftButtonPress(QPoint(10, 20), false);

    QCOMPARE(transition.type, GlobalMouseDragTransitionType::None);
    QVERIFY(!tracker.isDragging());
}

void GlobalMouseDragTrackerTests::moveDuringDragEmitsMove()
{
    GlobalMouseDragTracker tracker;
    tracker.handleLeftButtonPress(QPoint(10, 20), true);

    const GlobalMouseDragTransition transition =
        tracker.handleMouseMove(QPoint(30, 45), true);

    QCOMPARE(transition.type, GlobalMouseDragTransitionType::Moved);
    QCOMPARE(transition.position, QPoint(30, 45));
    QVERIFY(tracker.isDragging());
}

void GlobalMouseDragTrackerTests::moveAfterModifierReleaseFinishes()
{
    GlobalMouseDragTracker tracker;
    tracker.handleLeftButtonPress(QPoint(10, 20), true);

    const GlobalMouseDragTransition transition =
        tracker.handleMouseMove(QPoint(30, 45), false);

    QCOMPARE(transition.type, GlobalMouseDragTransitionType::Finished);
    QCOMPARE(transition.position, QPoint(30, 45));
    QVERIFY(!tracker.isDragging());
}

void GlobalMouseDragTrackerTests::releasingDuringDragFinishes()
{
    GlobalMouseDragTracker tracker;
    tracker.handleLeftButtonPress(QPoint(10, 20), true);

    const GlobalMouseDragTransition transition =
        tracker.handleLeftButtonRelease(QPoint(35, 50));

    QCOMPARE(transition.type, GlobalMouseDragTransitionType::Finished);
    QCOMPARE(transition.position, QPoint(35, 50));
    QVERIFY(!tracker.isDragging());
}

void GlobalMouseDragTrackerTests::modifierReleaseDuringDragFinishes()
{
    GlobalMouseDragTracker tracker;
    tracker.handleLeftButtonPress(QPoint(10, 20), true);

    const GlobalMouseDragTransition transition =
        tracker.handleModifierChanged(QPoint(20, 30), false);

    QCOMPARE(transition.type, GlobalMouseDragTransitionType::Finished);
    QCOMPARE(transition.position, QPoint(20, 30));
    QVERIFY(!tracker.isDragging());
}

QTEST_APPLESS_MAIN(GlobalMouseDragTrackerTests)

#include "global_mouse_drag_tracker_tests.moc"
