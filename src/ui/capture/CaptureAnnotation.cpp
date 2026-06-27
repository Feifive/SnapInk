#include "CaptureAnnotation.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <QTextOption>
#include <QtMath>

namespace
{
QPen drawingPen(QPen pen)
{
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}
}

RectAnnotationItem::RectAnnotationItem(const QRectF& rect, const QPen& pen)
    : QGraphicsRectItem(rect.normalized())
{
    setPen(drawingPen(pen));
    setBrush(Qt::NoBrush);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

ArrowAnnotationItem::ArrowAnnotationItem(const QLineF& line, const QPen& pen)
    : m_line(line)
    , m_pen(drawingPen(pen))
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QLineF ArrowAnnotationItem::line() const
{
    return m_line;
}

QRectF ArrowAnnotationItem::boundingRect() const
{
    const qreal margin = qMax<qreal>(12.0, m_pen.widthF() * 5.0);
    return QRectF(m_line.p1(), m_line.p2()).normalized().adjusted(-margin, -margin, margin, margin);
}

void ArrowAnnotationItem::paint(QPainter* painter,
                                const QStyleOptionGraphicsItem* option,
                                QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(m_pen);
    painter->setBrush(m_pen.color());
    painter->drawLine(m_line);
    painter->drawPolygon(arrowHead());
    painter->restore();
}

QPolygonF ArrowAnnotationItem::arrowHead() const
{
    if (m_line.length() <= 0.0) {
        return {};
    }

    const qreal size = qMax<qreal>(10.0, m_pen.widthF() * 4.0);
    const qreal angle = std::atan2(-m_line.dy(), m_line.dx());
    const QPointF end = m_line.p2();
    const QPointF p1 = end - QPointF(std::cos(angle + M_PI / 6.0) * size,
                                     -std::sin(angle + M_PI / 6.0) * size);
    const QPointF p2 = end - QPointF(std::cos(angle - M_PI / 6.0) * size,
                                     -std::sin(angle - M_PI / 6.0) * size);
    return QPolygonF({end, p1, p2});
}

PenAnnotationItem::PenAnnotationItem(const QPainterPath& path, const QPen& pen)
    : QGraphicsPathItem(path)
{
    setPen(drawingPen(pen));
    setBrush(Qt::NoBrush);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

TextAnnotationItem::TextAnnotationItem(const QPointF& position,
                                       const QColor& color,
                                       const QFont& font,
                                       QGraphicsItem* parent)
    : QGraphicsTextItem(parent)
{
    setPos(position);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setDefaultTextColor(color);
    setFont(font);

    QTextOption option = document()->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAnywhere);
    document()->setDefaultTextOption(option);

    connect(document(), &QTextDocument::contentsChanged,
            this, &TextAnnotationItem::updateTextWidthConstraint);
}

void TextAnnotationItem::setFinishedHandler(std::function<void(TextAnnotationItem*, FinishAction)> handler)
{
    m_finishedHandler = std::move(handler);
}

void TextAnnotationItem::setMaximumTextWidth(qreal width)
{
    m_maximumTextWidth = width;
    updateTextWidthConstraint();
}

void TextAnnotationItem::finishEditing(FinishAction action)
{
    if (m_finishing) {
        return;
    }

    m_finishing = true;
    if (m_finishedHandler) {
        m_finishedHandler(this, action);
    }
    m_finishing = false;
}

void TextAnnotationItem::commitEditing()
{
    finishEditing(FinishAction::Commit);
}

void TextAnnotationItem::cancelEditing()
{
    finishEditing(FinishAction::Cancel);
}

void TextAnnotationItem::focusOutEvent(QFocusEvent* event)
{
    QGraphicsTextItem::focusOutEvent(event);
    finishEditing(FinishAction::Commit);
}

void TextAnnotationItem::keyPressEvent(QKeyEvent* event)
{
    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    if (control && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        event->accept();
        finishEditing(FinishAction::Commit);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        event->accept();
        const FinishAction action = toPlainText().trimmed().isEmpty()
                                        ? FinishAction::Cancel
                                        : FinishAction::Commit;
        finishEditing(action);
        return;
    }

    QGraphicsTextItem::keyPressEvent(event);
}

void TextAnnotationItem::updateTextWidthConstraint()
{
    if (m_updatingTextWidth) {
        return;
    }

    m_updatingTextWidth = true;

    if (m_maximumTextWidth <= 0.0 || toPlainText().isEmpty()) {
        setTextWidth(-1.0);
        m_updatingTextWidth = false;
        return;
    }

    setTextWidth(-1.0);
    const qreal naturalWidth = document()->idealWidth();
    if (naturalWidth > m_maximumTextWidth) {
        setTextWidth(m_maximumTextWidth);
    }

    m_updatingTextWidth = false;
}
