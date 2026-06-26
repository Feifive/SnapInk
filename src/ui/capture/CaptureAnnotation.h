#ifndef CAPTUREANNOTATION_H
#define CAPTUREANNOTATION_H

#include <QFont>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>

#include <memory>
#include <vector>

class Annotation
{
public:
    virtual ~Annotation() = default;

    virtual void paint(QPainter& painter) const = 0;
    virtual std::unique_ptr<Annotation> clone() const = 0;
};

using AnnotationList = std::vector<std::unique_ptr<Annotation>>;

class RectAnnotation final : public Annotation
{
public:
    RectAnnotation(const QRectF& rect, const QPen& pen);

    QRectF rect() const;
    void paint(QPainter& painter) const override;
    std::unique_ptr<Annotation> clone() const override;

private:
    QRectF m_rect;
    QPen m_pen;
};

class ArrowAnnotation final : public Annotation
{
public:
    ArrowAnnotation(const QLineF& line, const QPen& pen);

    QLineF line() const;
    void paint(QPainter& painter) const override;
    std::unique_ptr<Annotation> clone() const override;

private:
    QPolygonF arrowHead() const;

    QLineF m_line;
    QPen m_pen;
};

class PenAnnotation final : public Annotation
{
public:
    PenAnnotation(const QPainterPath& path, const QPen& pen);

    QPainterPath path() const;
    void paint(QPainter& painter) const override;
    std::unique_ptr<Annotation> clone() const override;

private:
    QPainterPath m_path;
    QPen m_pen;
};

class TextAnnotation final : public Annotation
{
public:
    TextAnnotation(const QPointF& position,
                   const QString& text,
                   const QColor& color,
                   const QFont& font = QFont(QStringLiteral("Segoe UI"), 18));

    QPointF position() const;
    QString text() const;
    void paint(QPainter& painter) const override;
    std::unique_ptr<Annotation> clone() const override;

private:
    QPointF m_position;
    QString m_text;
    QColor m_color;
    QFont m_font;
};

#endif // CAPTUREANNOTATION_H
