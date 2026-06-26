#include "CaptureToolbar.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

CaptureToolbar::CaptureToolbar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral(
        "CaptureToolbar { background: #fafafa; border: 1px solid #b8c0cc; border-radius: 5px; }"
        "QToolButton { border: 0; padding: 6px 9px; color: #1f2937; font-size: 13px; }"
        "QToolButton:hover { background: #e8f1ff; }"
        "QToolButton:checked { background: #2f80ed; color: white; border-radius: 3px; }"
        "QToolButton:disabled { color: #a0a7b3; }"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    auto* tools = new QButtonGroup(this);
    tools->setExclusive(true);

    m_rectButton = addToolButton(QStringLiteral("Rect"), QStringLiteral("Rectangle"), CaptureTool::Rect);
    m_arrowButton = addToolButton(QStringLiteral("Arrow"), QStringLiteral("Arrow"), CaptureTool::Arrow);
    m_penButton = addToolButton(QStringLiteral("Pen"), QStringLiteral("Pen"), CaptureTool::Pen);
    m_textButton = addToolButton(QStringLiteral("Text"), QStringLiteral("Text"), CaptureTool::Text);

    for (QToolButton* button : {m_rectButton, m_arrowButton, m_penButton, m_textButton}) {
        tools->addButton(button);
        layout->addWidget(button);
    }

    layout->addSpacing(6);

    m_undoButton = addButton(QStringLiteral("Undo"), QStringLiteral("Undo"));
    m_redoButton = addButton(QStringLiteral("Redo"), QStringLiteral("Redo"));
    layout->addWidget(m_undoButton);
    layout->addWidget(m_redoButton);

    layout->addSpacing(6);

    QToolButton* reselectButton = addButton(QStringLiteral("Reselect"), QStringLiteral("Select another region"));
    QToolButton* copyButton = addButton(QStringLiteral("Copy"), QStringLiteral("Copy"));
    QToolButton* saveButton = addButton(QStringLiteral("Save"), QStringLiteral("Save PNG"));
    QToolButton* pinButton = addButton(QStringLiteral("Pin"), QStringLiteral("Pin placeholder"));
    QToolButton* cancelButton = addButton(QStringLiteral("Cancel"), QStringLiteral("Cancel"));
    QToolButton* confirmButton = addButton(QStringLiteral("OK"), QStringLiteral("Copy and close"));

    for (QToolButton* button : {reselectButton, copyButton, saveButton, pinButton, cancelButton, confirmButton}) {
        layout->addWidget(button);
    }

    connect(m_rectButton, &QToolButton::clicked, this, [this]() { Q_EMIT toolSelected(CaptureTool::Rect); });
    connect(m_arrowButton, &QToolButton::clicked, this, [this]() { Q_EMIT toolSelected(CaptureTool::Arrow); });
    connect(m_penButton, &QToolButton::clicked, this, [this]() { Q_EMIT toolSelected(CaptureTool::Pen); });
    connect(m_textButton, &QToolButton::clicked, this, [this]() { Q_EMIT toolSelected(CaptureTool::Text); });
    connect(m_undoButton, &QToolButton::clicked, this, &CaptureToolbar::undoRequested);
    connect(m_redoButton, &QToolButton::clicked, this, &CaptureToolbar::redoRequested);
    connect(reselectButton, &QToolButton::clicked, this, &CaptureToolbar::reselectRequested);
    connect(copyButton, &QToolButton::clicked, this, &CaptureToolbar::copyRequested);
    connect(saveButton, &QToolButton::clicked, this, &CaptureToolbar::saveRequested);
    connect(pinButton, &QToolButton::clicked, this, &CaptureToolbar::pinRequested);
    connect(cancelButton, &QToolButton::clicked, this, &CaptureToolbar::cancelRequested);
    connect(confirmButton, &QToolButton::clicked, this, &CaptureToolbar::confirmRequested);

    setCurrentTool(CaptureTool::Rect);
    setUndoAvailable(false);
    setRedoAvailable(false);
}

void CaptureToolbar::setCurrentTool(CaptureTool tool)
{
    m_rectButton->setChecked(tool == CaptureTool::Rect);
    m_arrowButton->setChecked(tool == CaptureTool::Arrow);
    m_penButton->setChecked(tool == CaptureTool::Pen);
    m_textButton->setChecked(tool == CaptureTool::Text);
}

void CaptureToolbar::setUndoAvailable(bool available)
{
    m_undoButton->setEnabled(available);
}

void CaptureToolbar::setRedoAvailable(bool available)
{
    m_redoButton->setEnabled(available);
}

QToolButton* CaptureToolbar::addButton(const QString& text, const QString& tooltip)
{
    auto* button = new QToolButton(this);
    button->setText(text);
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    return button;
}

QToolButton* CaptureToolbar::addToolButton(const QString& text, const QString& tooltip, CaptureTool tool)
{
    Q_UNUSED(tool)

    QToolButton* button = addButton(text, tooltip);
    button->setCheckable(true);
    return button;
}
