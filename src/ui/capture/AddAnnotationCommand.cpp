#include "AddAnnotationCommand.h"

AddAnnotationCommand::AddAnnotationCommand(AnnotationList* annotations,
                                           std::unique_ptr<Annotation> annotation,
                                           std::function<void()> changed,
                                           QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_annotations(annotations)
    , m_annotation(std::move(annotation))
    , m_changed(std::move(changed))
{
    setText(QStringLiteral("Add annotation"));
}

AddAnnotationCommand::~AddAnnotationCommand() = default;

void AddAnnotationCommand::undo()
{
    if (m_annotations == nullptr || !m_inserted || m_index < 0
        || m_index >= static_cast<int>(m_annotations->size())) {
        return;
    }

    auto it = m_annotations->begin() + m_index;
    m_annotation = std::move(*it);
    m_annotations->erase(it);
    m_inserted = false;

    if (m_changed) {
        m_changed();
    }
}

void AddAnnotationCommand::redo()
{
    if (m_annotations == nullptr || m_inserted || m_annotation == nullptr) {
        return;
    }

    if (m_index < 0 || m_index > static_cast<int>(m_annotations->size())) {
        m_index = static_cast<int>(m_annotations->size());
    }

    m_annotations->insert(m_annotations->begin() + m_index, std::move(m_annotation));
    m_inserted = true;

    if (m_changed) {
        m_changed();
    }
}
