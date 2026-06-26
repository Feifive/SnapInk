#ifndef CAPTURETOOLBAR_H
#define CAPTURETOOLBAR_H

#include "CaptureTool.h"

#include <QWidget>

class QToolButton;

class CaptureToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit CaptureToolbar(QWidget* parent = nullptr);

    void setCurrentTool(CaptureTool tool);
    void setUndoAvailable(bool available);
    void setRedoAvailable(bool available);

signals:
    void toolSelected(CaptureTool tool);
    void undoRequested();
    void redoRequested();
    void reselectRequested();
    void copyRequested();
    void saveRequested();
    void pinRequested();
    void cancelRequested();
    void confirmRequested();

private:
    QToolButton* addButton(const QString& text, const QString& tooltip);
    QToolButton* addToolButton(const QString& text, const QString& tooltip, CaptureTool tool);

    QToolButton* m_rectButton = nullptr;
    QToolButton* m_arrowButton = nullptr;
    QToolButton* m_penButton = nullptr;
    QToolButton* m_textButton = nullptr;
    QToolButton* m_undoButton = nullptr;
    QToolButton* m_redoButton = nullptr;
};

#endif // CAPTURETOOLBAR_H
