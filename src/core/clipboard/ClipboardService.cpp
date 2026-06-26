#include "ClipboardService.h"

#include <QClipboard>
#include <QGuiApplication>

void ClipboardService::copyImage(const QImage& image)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr || image.isNull()) {
        return;
    }

    clipboard->setImage(image);
}
