#ifndef CAPTURE_ADDANNOTATIONCOMMAND_H
#define CAPTURE_ADDANNOTATIONCOMMAND_H

#include "CaptureAnnotation.h"

#include <QUndoCommand>

#include <functional>
#include <memory>

class AddAnnotationCommand : public QUndoCommand
{
public:
    AddAnnotationCommand(AnnotationList* annotations,
                         std::unique_ptr<Annotation> annotation,
                         std::function<void()> changed,
                         QUndoCommand* parent = nullptr);
    ~AddAnnotationCommand() override;

    void undo() override;
    void redo() override;

private:
    AnnotationList* m_annotations = nullptr;
    std::unique_ptr<Annotation> m_annotation;
    std::function<void()> m_changed;
    int m_index = -1;
    bool m_inserted = false;
};

#endif // CAPTURE_ADDANNOTATIONCOMMAND_H
