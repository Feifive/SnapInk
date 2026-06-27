#include "AddAnnotationCommand.h"

#include <QGraphicsItem>
#include <QGraphicsScene>

AddAnnotationCommand::AddAnnotationCommand(QGraphicsScene* scene,
                                           QGraphicsItem* item,
                                           std::function<void()> changed,
                                           QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_item(item)
    , m_changed(std::move(changed))
{
    setText(QStringLiteral("Add annotation"));
}

AddAnnotationCommand::~AddAnnotationCommand()
{
    if (m_item == nullptr) {
        return;
    }

    if (m_item->scene() != nullptr) {
        m_item->scene()->removeItem(m_item);
    }

    delete m_item;
    m_item = nullptr;
}

void AddAnnotationCommand::undo()
{
    if (m_scene == nullptr || m_item == nullptr || !m_inserted) {
        return;
    }

    m_scene->removeItem(m_item);
    m_inserted = false;

    if (m_changed) {
        m_changed();
    }
}

void AddAnnotationCommand::redo()
{
    if (m_scene == nullptr || m_item == nullptr || m_inserted) {
        return;
    }

    m_scene->addItem(m_item);
    m_inserted = true;

    if (m_changed) {
        m_changed();
    }
}
