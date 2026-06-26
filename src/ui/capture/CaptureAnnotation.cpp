#include "CaptureAnnotation.h"

#include <QPainter>
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

RectAnnotation::RectAnnotation(const QRectF& rect, const QPen& pen)
    : m_rect(rect.normalized())
    , m_pen(drawingPen(pen))
{
}

QRectF RectAnnotation::rect() const
{
    return m_rect;
}

void RectAnnotation::paint(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(m_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_rect);
    painter.restore();
}

std::unique_ptr<Annotation> RectAnnotation::clone() const
{
    return std::make_unique<RectAnnotation>(m_rect, m_pen);
}

ArrowAnnotation::ArrowAnnotation(const QLineF& line, const QPen& pen)
    : m_line(line)
    , m_pen(drawingPen(pen))
{
}

QLineF ArrowAnnotation::line() const
{
    return m_line;
}

void ArrowAnnotation::paint(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(m_pen);
    painter.setBrush(m_pen.color());
    painter.drawLine(m_line);
    painter.drawPolygon(arrowHead());
    painter.restore();
}

std::unique_ptr<Annotation> ArrowAnnotation::clone() const
{
    return std::make_unique<ArrowAnnotation>(m_line, m_pen);
}

QPolygonF ArrowAnnotation::arrowHead() const
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

PenAnnotation::PenAnnotation(const QPainterPath& path, const QPen& pen)
    : m_path(path)
    , m_pen(drawingPen(pen))
{
}

QPainterPath PenAnnotation::path() const
{
    return m_path;
}

void PenAnnotation::paint(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(m_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(m_path);
    painter.restore();
}

std::unique_ptr<Annotation> PenAnnotation::clone() const
{
    return std::make_unique<PenAnnotation>(m_path, m_pen);
}

TextAnnotation::TextAnnotation(const QPointF& position,
                               const QString& text,
                               const QColor& color,
                               const QFont& font)
    : m_position(position)
    , m_text(text)
    , m_color(color)
    , m_font(font)
{
}

QPointF TextAnnotation::position() const
{
    return m_position;
}

QString TextAnnotation::text() const
{
    return m_text;
}

void TextAnnotation::paint(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(m_font);
    painter.setPen(m_color);
    painter.drawText(m_position, m_text);
    painter.restore();
}

std::unique_ptr<Annotation> TextAnnotation::clone() const
{
    return std::make_unique<TextAnnotation>(m_position, m_text, m_color, m_font);
}
