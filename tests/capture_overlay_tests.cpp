#include "../src/ui/capture/CaptureAnnotation.h"
#include "../src/ui/capture/CaptureOverlay.h"

#include <QImage>
#include <QTest>

class CaptureOverlayTests : public QObject
{
    Q_OBJECT

private slots:
    void overlayStartsInSelectingState();
    void overlayEditingRenderUsesSelectionSize();
    void overlayAnnotationsUseSelectionRelativeCoordinates();
    void overlayUndoRedoUpdatesRenderedResult();
};

void CaptureOverlayTests::overlayStartsInSelectingState()
{
    QImage desktop(80, 60, QImage::Format_ARGB32);
    desktop.fill(Qt::white);

    CaptureOverlay overlay(desktop, QRect(0, 0, 80, 60));

    QCOMPARE(overlay.state(), CaptureState::Selecting);
    QVERIFY(overlay.renderResultImage().isNull());
}

void CaptureOverlayTests::overlayEditingRenderUsesSelectionSize()
{
    QImage desktop(80, 60, QImage::Format_ARGB32);
    desktop.fill(Qt::white);
    CaptureOverlay overlay(desktop, QRect(0, 0, 80, 60));

    overlay.enterEditing(QRect(10, 12, 30, 20));
    const QImage result = overlay.renderResultImage();

    QCOMPARE(overlay.state(), CaptureState::Editing);
    QCOMPARE(result.size(), QSize(30, 20));
}

void CaptureOverlayTests::overlayAnnotationsUseSelectionRelativeCoordinates()
{
    QImage desktop(80, 60, QImage::Format_ARGB32);
    desktop.fill(Qt::white);
    CaptureOverlay overlay(desktop, QRect(0, 0, 80, 60));
    overlay.enterEditing(QRect(20, 10, 30, 20));

    overlay.addAnnotation(std::make_unique<RectAnnotation>(QRectF(0.0, 0.0, 12.0, 8.0),
                                                           QPen(Qt::red, 3.0)));
    const QImage result = overlay.renderResultImage();

    QCOMPARE(result.pixelColor(1, 1), QColor(Qt::red));
    QCOMPARE(result.pixelColor(14, 10), QColor(Qt::white));
}

void CaptureOverlayTests::overlayUndoRedoUpdatesRenderedResult()
{
    QImage desktop(80, 60, QImage::Format_ARGB32);
    desktop.fill(Qt::white);
    CaptureOverlay overlay(desktop, QRect(0, 0, 80, 60));
    overlay.enterEditing(QRect(0, 0, 30, 20));

    overlay.addAnnotation(std::make_unique<RectAnnotation>(QRectF(0.0, 0.0, 12.0, 8.0),
                                                           QPen(Qt::red, 3.0)));
    QVERIFY(overlay.canUndo());

    overlay.undo();
    QVERIFY(!overlay.canUndo());
    QVERIFY(overlay.canRedo());
    QCOMPARE(overlay.renderResultImage().pixelColor(1, 1), QColor(Qt::white));

    overlay.redo();
    QCOMPARE(overlay.renderResultImage().pixelColor(1, 1), QColor(Qt::red));
}

QTEST_MAIN(CaptureOverlayTests)

#include "capture_overlay_tests.moc"
