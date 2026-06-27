#ifndef CAPTUREANNOTATION_H
#define CAPTUREANNOTATION_H

#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QLineF>
#include <QPen>

#include <functional>

class RectAnnotationItem final : public QGraphicsRectItem
{
public:
    RectAnnotationItem(const QRectF& rect, const QPen& pen);
};

class ArrowAnnotationItem final : public QGraphicsItem
{
public:
    ArrowAnnotationItem(const QLineF& line, const QPen& pen);

    QLineF line() const;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QPolygonF arrowHead() const;

    QLineF m_line;
    QPen m_pen;
};

class PenAnnotationItem final : public QGraphicsPathItem
{
public:
    PenAnnotationItem(const QPainterPath& path, const QPen& pen);
};

class TextAnnotationItem final : public QGraphicsTextItem
{
public:
    enum class FinishAction
    {
        Commit,
        Cancel
    };

    TextAnnotationItem(const QPointF& position,
                       const QColor& color,
                       const QFont& font = QFont(QStringLiteral("Segoe UI"), 18),
                       QGraphicsItem* parent = nullptr);

    void setFinishedHandler(std::function<void(TextAnnotationItem*, FinishAction)> handler);
    void setMaximumTextWidth(qreal width);
    void finishEditing(FinishAction action);
    void commitEditing();
    void cancelEditing();

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateTextWidthConstraint();

    std::function<void(TextAnnotationItem*, FinishAction)> m_finishedHandler;
    qreal m_maximumTextWidth = -1.0;
    bool m_finishing = false;
    bool m_updatingTextWidth = false;
};

#endif // CAPTUREANNOTATION_H
