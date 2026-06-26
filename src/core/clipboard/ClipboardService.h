#ifndef CLIPBOARDSERVICE_H
#define CLIPBOARDSERVICE_H

#include <QImage>

class ClipboardService
{
public:
    static void copyImage(const QImage& image);
};

#endif // CLIPBOARDSERVICE_H
