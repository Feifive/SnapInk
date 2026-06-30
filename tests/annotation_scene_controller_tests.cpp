#include "../src/ui/capture/AnnotationSceneController.h"

#include "../src/ui/capture/CaptureAnnotation.h"

#include <QColor>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QTest>

namespace
{
QPixmap pixmapWithColor(const QSize& size, const QColor& color)
{
    QPixmap pixmap(size);
    pixmap.fill(color);
    return pixmap;
}

template <typename T>
QList<T*> itemsOfType(QGraphicsScene* scene)
{
    QList<T*> result;
    for (QGraphicsItem* item : scene->items()) {
        if (auto* typed = dynamic_cast<T*>(item)) {
            result.append(typed);
        }
    }
    return result;
}

QImage renderController(AnnotationSceneController& controller, const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    controller.renderScene(&painter, QRectF(QPointF(0.0, 0.0), QSizeF(size)));
    painter.end();

    return image;
}
}

class AnnotationSceneControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void setBackgroundPixmapCreatesAndReplacesBackgroundItem();
    void backgroundPixmapUsesLowZValue();
    void rectangleAnnotationFinishesAsUndoableSceneItem();
    void arrowAnnotationFinishesAsUndoableSceneItem();
    void penAnnotationFinishesAsUndoableSceneItem();
    void emptyTextCommitDoesNotCreateUndoableItem();
    void normalTextCommitCanUndoAndRedo();
    void undoRemovesAnnotationAndRedoRestoresIt();
    void shiftAllAnnotationsMovesCommittedItems();
    void shiftAllAnnotationsDoesNotMoveBackground();
    void previewItemIsExcludedFromRender();
    void renderCommitsActiveTextEditing();
    void clearAllResetsSceneInteractionStateAndUndoStack();
};

void AnnotationSceneControllerTests::setBackgroundPixmapCreatesAndReplacesBackgroundItem()
{
    AnnotationSceneController controller;

    controller.setBackgroundPixmap(pixmapWithColor(QSize(12, 8), Qt::red));
    auto backgroundItems = itemsOfType<QGraphicsPixmapItem>(controller.scene());
    QCOMPARE(backgroundItems.size(), 1);
    QCOMPARE(backgroundItems.first()->pixmap().toImage().pixelColor(0, 0), QColor(Qt::red));

    controller.setBackgroundPixmap(pixmapWithColor(QSize(10, 6), Qt::blue));
    backgroundItems = itemsOfType<QGraphicsPixmapItem>(controller.scene());
    QCOMPARE(backgroundItems.size(), 1);
    QCOMPARE(backgroundItems.first()->pixmap().size(), QSize(10, 6));
    QCOMPARE(backgroundItems.first()->pixmap().toImage().pixelColor(0, 0), QColor(Qt::blue));
}

void AnnotationSceneControllerTests::backgroundPixmapUsesLowZValue()
{
    AnnotationSceneController controller;

    controller.setBackgroundPixmap(pixmapWithColor(QSize(12, 8), Qt::white));

    const auto backgroundItems = itemsOfType<QGraphicsPixmapItem>(controller.scene());
    QCOMPARE(backgroundItems.size(), 1);
    QVERIFY(backgroundItems.first()->zValue() < 0.0);
}

void AnnotationSceneControllerTests::rectangleAnnotationFinishesAsUndoableSceneItem()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
    controller.setCurrentTool(CaptureTool::Rect);

    controller.beginAnnotation(QPointF(4.0, 5.0));
    controller.updateAnnotation(QPointF(24.0, 18.0));
    controller.finishAnnotation(QPointF(24.0, 18.0));

    QCOMPARE(itemsOfType<RectAnnotationItem>(controller.scene()).size(), 1);
    QVERIFY(controller.canUndo());
    QVERIFY(!controller.hasActivePreview());
}

void AnnotationSceneControllerTests::arrowAnnotationFinishesAsUndoableSceneItem()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
    controller.setCurrentTool(CaptureTool::Arrow);

    controller.beginAnnotation(QPointF(4.0, 5.0));
    controller.updateAnnotation(QPointF(28.0, 25.0));
    controller.finishAnnotation(QPointF(28.0, 25.0));

    QCOMPARE(itemsOfType<ArrowAnnotationItem>(controller.scene()).size(), 1);
    QVERIFY(controller.canUndo());
}

void AnnotationSceneControllerTests::penAnnotationFinishesAsUndoableSceneItem()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
    controller.setCurrentTool(CaptureTool::Pen);

    controller.beginAnnotation(QPointF(4.0, 5.0));
    controller.updateAnnotation(QPointF(10.0, 9.0));
    controller.updateAnnotation(QPointF(20.0, 14.0));
    controller.finishAnnotation(QPointF(20.0, 14.0));

    QCOMPARE(itemsOfType<PenAnnotationItem>(controller.scene()).size(), 1);
    QVERIFY(controller.canUndo());
}

void AnnotationSceneControllerTests::emptyTextCommitDoesNotCreateUndoableItem()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));

    controller.beginTextEditing(QPointF(5.0, 6.0));
    QVERIFY(controller.hasActiveTextEditing());
    controller.commitActiveTextEditing();

    QCOMPARE(itemsOfType<TextAnnotationItem>(controller.scene()).size(), 0);
    QVERIFY(!controller.canUndo());
    QVERIFY(!controller.hasActiveTextEditing());
}

void AnnotationSceneControllerTests::normalTextCommitCanUndoAndRedo()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));

    controller.beginTextEditing(QPointF(5.0, 6.0));
    auto textItems = itemsOfType<TextAnnotationItem>(controller.scene());
    QCOMPARE(textItems.size(), 1);
    textItems.first()->setPlainText(QStringLiteral("Snap"));

    controller.commitActiveTextEditing();
    QCOMPARE(itemsOfType<TextAnnotationItem>(controller.scene()).size(), 1);
    QVERIFY(controller.canUndo());

    controller.undo();
    QCOMPARE(itemsOfType<TextAnnotationItem>(controller.scene()).size(), 0);
    QVERIFY(controller.canRedo());

    controller.redo();
    QCOMPARE(itemsOfType<TextAnnotationItem>(controller.scene()).size(), 1);
}

void AnnotationSceneControllerTests::undoRemovesAnnotationAndRedoRestoresIt()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
    controller.addAnnotationItem(new RectAnnotationItem(QRectF(4.0, 5.0, 20.0, 12.0), QPen(Qt::red, 3.0)));

    QCOMPARE(itemsOfType<RectAnnotationItem>(controller.scene()).size(), 1);

    controller.undo();
    QCOMPARE(itemsOfType<RectAnnotationItem>(controller.scene()).size(), 0);
    QVERIFY(controller.canRedo());

    controller.redo();
    QCOMPARE(itemsOfType<RectAnnotationItem>(controller.scene()).size(), 1);
}

void AnnotationSceneControllerTests::shiftAllAnnotationsMovesCommittedItems()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
    auto* item = new RectAnnotationItem(QRectF(4.0, 5.0, 20.0, 12.0), QPen(Qt::red, 3.0));
    controller.addAnnotationItem(item);

    controller.shiftAllAnnotations(QPointF(7.0, -3.0));

    QCOMPARE(item->pos(), QPointF(7.0, -3.0));
}

void AnnotationSceneControllerTests::shiftAllAnnotationsDoesNotMoveBackground()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
    controller.setBackgroundPixmap(pixmapWithColor(QSize(80, 60), Qt::white));
    const auto backgroundItems = itemsOfType<QGraphicsPixmapItem>(controller.scene());
    QCOMPARE(backgroundItems.size(), 1);

    controller.addAnnotationItem(new RectAnnotationItem(QRectF(4.0, 5.0, 20.0, 12.0), QPen(Qt::red, 3.0)));
    controller.shiftAllAnnotations(QPointF(7.0, -3.0));

    QCOMPARE(backgroundItems.first()->pos(), QPointF(0.0, 0.0));
}

void AnnotationSceneControllerTests::previewItemIsExcludedFromRender()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 40.0, 30.0));
    controller.setBackgroundPixmap(pixmapWithColor(QSize(40, 30), Qt::white));
    controller.setCurrentTool(CaptureTool::Rect);

    controller.beginAnnotation(QPointF(1.0, 1.0));
    controller.updateAnnotation(QPointF(25.0, 18.0));
    QVERIFY(controller.hasActivePreview());

    const QImage image = renderController(controller, QSize(40, 30));
    QCOMPARE(image.pixelColor(1, 1), QColor(Qt::white));
    QVERIFY(controller.hasActivePreview());
}

void AnnotationSceneControllerTests::renderCommitsActiveTextEditing()
{
    AnnotationSceneController controller;
    controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
    controller.beginTextEditing(QPointF(5.0, 6.0));
    auto textItems = itemsOfType<TextAnnotationItem>(controller.scene());
    QCOMPARE(textItems.size(), 1);
    textItems.first()->setPlainText(QStringLiteral("Snap"));

    const QImage image = renderController(controller, QSize(80, 60));

    QVERIFY(!image.isNull());
    QVERIFY(controller.canUndo());
    QVERIFY(!controller.hasActiveTextEditing());
    QCOMPARE(itemsOfType<TextAnnotationItem>(controller.scene()).size(), 1);
}

void AnnotationSceneControllerTests::clearAllResetsSceneInteractionStateAndUndoStack()
{
    {
        AnnotationSceneController controller;
        controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
        controller.setBackgroundPixmap(pixmapWithColor(QSize(80, 60), Qt::white));
        controller.addAnnotationItem(new RectAnnotationItem(QRectF(4.0, 5.0, 20.0, 12.0), QPen(Qt::red, 3.0)));
        controller.setCurrentTool(CaptureTool::Arrow);
        controller.beginAnnotation(QPointF(10.0, 10.0));
        controller.updateAnnotation(QPointF(30.0, 25.0));
        QVERIFY(controller.hasActivePreview());

        controller.clearAll();

        QCOMPARE(controller.scene()->items().size(), 0);
        QVERIFY(!controller.hasActivePreview());
        QVERIFY(!controller.hasActiveTextEditing());
        QVERIFY(!controller.canUndo());
        QVERIFY(!controller.canRedo());
    }

    {
        AnnotationSceneController controller;
        controller.setSceneRect(QRectF(0.0, 0.0, 80.0, 60.0));
        controller.beginTextEditing(QPointF(12.0, 14.0));
        QVERIFY(controller.hasActiveTextEditing());

        controller.clearAll();

        QCOMPARE(controller.scene()->items().size(), 0);
        QVERIFY(!controller.hasActiveTextEditing());
    }
}

QTEST_MAIN(AnnotationSceneControllerTests)

#include "annotation_scene_controller_tests.moc"
