#include "ui/capture/CaptureSelectionModel.h"

#include <QRect>
#include <QTest>

class CaptureSelectionModelTests : public QObject
{
    Q_OBJECT

private slots:
    void initialSelectionBuildsExpectedRect();
    void reverseDragSelectionBuildsExpectedRect();
    void tooSmallSelectionCannotCommit();
    void movingSelectionStaysInsideBounds();
    void resizingHandlesAdjustExpectedEdges();
    void resizingCannotShrinkBelowMinimumSize();
    void hitTestingFindsHandlesAndSelectionInterior();
    void wheelResizeChangesEachEdgeByOnePixel();
    void wheelResizeStopsAtBoundsAndMinimumSize();
    void nonZeroBoundsOriginIsRespected();
    void moveToBottomRightUsesExclusiveBoundsEdge();
    void bottomRightResizeStopsAtExclusiveBoundsEdge();
    void wheelResizeWithNonZeroBoundsStopsAtExclusiveBoundsEdge();
};

void CaptureSelectionModelTests::initialSelectionBuildsExpectedRect()
{
    CaptureSelectionModel model(QRect(0, 0, 100, 80));

    model.beginSelection(QPoint(10, 12));
    model.updateSelection(QPoint(30, 25));

    QCOMPARE(model.currentSelection(), QRect(10, 12, 20, 13));
    QVERIFY(model.commitSelection());
    QCOMPARE(model.selectionRect(), QRect(10, 12, 20, 13));
}

void CaptureSelectionModelTests::reverseDragSelectionBuildsExpectedRect()
{
    CaptureSelectionModel model(QRect(0, 0, 100, 80));

    model.beginSelection(QPoint(30, 25));
    model.updateSelection(QPoint(10, 12));

    QCOMPARE(model.currentSelection(), QRect(10, 12, 20, 13));
    QVERIFY(model.commitSelection());
    QCOMPARE(model.selectionRect(), QRect(10, 12, 20, 13));
}

void CaptureSelectionModelTests::tooSmallSelectionCannotCommit()
{
    CaptureSelectionModel model(QRect(0, 0, 100, 80));
    constexpr int minimumSize = CaptureSelectionModel::minimumSelectionSize();

    model.beginSelection(QPoint(10, 10));
    model.updateSelection(QPoint(10 + minimumSize - 1, 20));

    QCOMPARE(model.currentSelection(), QRect(10, 10, minimumSize - 1, 10));
    QVERIFY(!model.commitSelection());
    QVERIFY(!model.hasSelection());
    QVERIFY(model.selectionRect().isEmpty());
}

void CaptureSelectionModelTests::movingSelectionStaysInsideBounds()
{
    CaptureSelectionModel model(QRect(0, 0, 100, 80));
    model.setSelectionRect(QRect(20, 15, 30, 20));

    model.beginMove(QPoint(25, 20));
    QVERIFY(model.updateMove(QPoint(-100, -100)));
    QCOMPARE(model.selectionRect(), QRect(0, 0, 30, 20));

    model.beginMove(QPoint(0, 0));
    QVERIFY(model.updateMove(QPoint(200, 200)));
    QCOMPARE(model.selectionRect(), QRect(70, 60, 30, 20));
}

void CaptureSelectionModelTests::resizingHandlesAdjustExpectedEdges()
{
    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        model.setSelectionRect(QRect(20, 20, 30, 20));
        model.beginResize(SelectionHandle::TopLeft, QPoint(20, 20));
        QVERIFY(model.updateResize(QPoint(15, 16)));
        QCOMPARE(model.selectionRect(), QRect(15, 16, 35, 24));
    }

    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        model.setSelectionRect(QRect(20, 20, 30, 20));
        model.beginResize(SelectionHandle::BottomRight, QPoint(49, 39));
        QVERIFY(model.updateResize(QPoint(54, 43)));
        QCOMPARE(model.selectionRect(), QRect(20, 20, 35, 24));
    }

    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        model.setSelectionRect(QRect(20, 20, 30, 20));
        model.beginResize(SelectionHandle::Top, QPoint(34, 20));
        QVERIFY(model.updateResize(QPoint(34, 16)));
        QCOMPARE(model.selectionRect(), QRect(20, 16, 30, 24));
    }

    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        model.setSelectionRect(QRect(20, 20, 30, 20));
        model.beginResize(SelectionHandle::Right, QPoint(49, 29));
        QVERIFY(model.updateResize(QPoint(54, 29)));
        QCOMPARE(model.selectionRect(), QRect(20, 20, 35, 20));
    }

    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        model.setSelectionRect(QRect(20, 20, 30, 20));
        model.beginResize(SelectionHandle::Bottom, QPoint(34, 39));
        QVERIFY(model.updateResize(QPoint(34, 43)));
        QCOMPARE(model.selectionRect(), QRect(20, 20, 30, 24));
    }

    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        model.setSelectionRect(QRect(20, 20, 30, 20));
        model.beginResize(SelectionHandle::Left, QPoint(20, 29));
        QVERIFY(model.updateResize(QPoint(15, 29)));
        QCOMPARE(model.selectionRect(), QRect(15, 20, 35, 20));
    }
}

void CaptureSelectionModelTests::resizingCannotShrinkBelowMinimumSize()
{
    CaptureSelectionModel model(QRect(0, 0, 100, 80));
    constexpr int minimumSize = CaptureSelectionModel::minimumSelectionSize();
    model.setSelectionRect(QRect(10, 10, 10, 10));

    model.beginResize(SelectionHandle::Left, QPoint(10, 14));
    QVERIFY(model.updateResize(QPoint(30, 14)));

    QCOMPARE(model.selectionRect(), QRect(20 - minimumSize, 10, minimumSize, 10));
}

void CaptureSelectionModelTests::hitTestingFindsHandlesAndSelectionInterior()
{
    CaptureSelectionModel model(QRect(0, 0, 100, 80));
    model.setSelectionRect(QRect(20, 10, 40, 30));
    const QRect selection = model.selectionRect();

    QCOMPARE(model.hitTestHandle(selection.topLeft()), SelectionHandle::TopLeft);
    QCOMPARE(model.hitTestHandle(QPoint(selection.center().x(), selection.top())),
             SelectionHandle::Top);
    QCOMPARE(model.hitTestHandle(selection.topRight()), SelectionHandle::TopRight);
    QCOMPARE(model.hitTestHandle(QPoint(selection.right(), selection.center().y())),
             SelectionHandle::Right);
    QCOMPARE(model.hitTestHandle(selection.bottomRight()), SelectionHandle::BottomRight);
    QCOMPARE(model.hitTestHandle(QPoint(selection.center().x(), selection.bottom())),
             SelectionHandle::Bottom);
    QCOMPARE(model.hitTestHandle(selection.bottomLeft()), SelectionHandle::BottomLeft);
    QCOMPARE(model.hitTestHandle(QPoint(selection.left(), selection.center().y())),
             SelectionHandle::Left);
    QCOMPARE(model.hitTestHandle(QPoint(0, 0)), SelectionHandle::None);

    QVERIFY(model.contains(QPoint(25, 15)));
    QVERIFY(!model.contains(QPoint(10, 15)));
}

void CaptureSelectionModelTests::wheelResizeChangesEachEdgeByOnePixel()
{
    CaptureSelectionModel model(QRect(0, 0, 100, 80));
    model.setSelectionRect(QRect(20, 20, 30, 20));

    QVERIFY(model.resizeByWheel(1));
    QCOMPARE(model.selectionRect(), QRect(19, 19, 32, 22));

    QVERIFY(model.resizeByWheel(-1));
    QCOMPARE(model.selectionRect(), QRect(20, 20, 30, 20));
}

void CaptureSelectionModelTests::wheelResizeStopsAtBoundsAndMinimumSize()
{
    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        model.setSelectionRect(QRect(0, 0, 100, 80));

        QVERIFY(!model.resizeByWheel(1));
        QCOMPARE(model.selectionRect(), QRect(0, 0, 100, 80));
    }

    {
        CaptureSelectionModel model(QRect(0, 0, 100, 80));
        constexpr int minimumSize = CaptureSelectionModel::minimumSelectionSize();
        model.setSelectionRect(QRect(20, 20, minimumSize, minimumSize));

        QVERIFY(!model.resizeByWheel(-1));
        QCOMPARE(model.selectionRect(), QRect(20, 20, minimumSize, minimumSize));
    }
}

void CaptureSelectionModelTests::nonZeroBoundsOriginIsRespected()
{
    CaptureSelectionModel model(QRect(10, 20, 100, 80));

    model.beginSelection(QPoint(20, 30));
    model.updateSelection(QPoint(40, 55));
    QVERIFY(model.commitSelection());
    QCOMPARE(model.selectionRect(), QRect(20, 30, 20, 25));

    model.beginMove(QPoint(20, 30));
    QVERIFY(model.updateMove(QPoint(-100, -100)));
    QCOMPARE(model.selectionRect(), QRect(10, 20, 20, 25));

    model.setSelectionRect(QRect(20, 30, 20, 20));
    model.beginResize(SelectionHandle::TopLeft, QPoint(20, 30));
    QVERIFY(model.updateResize(QPoint(0, 0)));
    QCOMPARE(model.selectionRect(), QRect(10, 20, 30, 30));
}

void CaptureSelectionModelTests::moveToBottomRightUsesExclusiveBoundsEdge()
{
    CaptureSelectionModel model(QRect(10, 20, 100, 80));
    model.setSelectionRect(QRect(20, 30, 20, 25));

    model.beginMove(QPoint(20, 30));
    QVERIFY(model.updateMove(QPoint(1000, 1000)));

    QCOMPARE(model.selectionRect(), QRect(90, 75, 20, 25));
    QCOMPARE(model.selectionRect().right(), 109);
    QCOMPARE(model.selectionRect().bottom(), 99);
}

void CaptureSelectionModelTests::bottomRightResizeStopsAtExclusiveBoundsEdge()
{
    CaptureSelectionModel model(QRect(10, 20, 100, 80));
    model.setSelectionRect(QRect(80, 70, 20, 20));

    model.beginResize(SelectionHandle::BottomRight, QPoint(99, 89));
    QVERIFY(model.updateResize(QPoint(1000, 1000)));

    QCOMPARE(model.selectionRect(), QRect(80, 70, 30, 30));
    QCOMPARE(model.selectionRect().right(), 109);
    QCOMPARE(model.selectionRect().bottom(), 99);
}

void CaptureSelectionModelTests::wheelResizeWithNonZeroBoundsStopsAtExclusiveBoundsEdge()
{
    CaptureSelectionModel model(QRect(10, 20, 100, 80));
    model.setSelectionRect(QRect(11, 21, 98, 78));

    QVERIFY(model.resizeByWheel(1));
    QCOMPARE(model.selectionRect(), QRect(10, 20, 100, 80));

    QVERIFY(!model.resizeByWheel(1));
    QCOMPARE(model.selectionRect(), QRect(10, 20, 100, 80));
    QCOMPARE(model.selectionRect().right(), 109);
    QCOMPARE(model.selectionRect().bottom(), 99);
}

QTEST_APPLESS_MAIN(CaptureSelectionModelTests)

#include "capture_selection_model_tests.moc"
