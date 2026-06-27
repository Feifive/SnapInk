#ifndef CAPTURE_ADDANNOTATIONCOMMAND_H
#define CAPTURE_ADDANNOTATIONCOMMAND_H

#include <QUndoCommand>

#include <functional>

class QGraphicsItem;
class QGraphicsScene;

class AddAnnotationCommand : public QUndoCommand
{
public:
    AddAnnotationCommand(QGraphicsScene* scene,
                         QGraphicsItem* item,
                         std::function<void()> changed,
                         QUndoCommand* parent = nullptr);
    ~AddAnnotationCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene* m_scene = nullptr;
    QGraphicsItem* m_item = nullptr;
    std::function<void()> m_changed;
    bool m_inserted = false;
};

#endif // CAPTURE_ADDANNOTATIONCOMMAND_H
