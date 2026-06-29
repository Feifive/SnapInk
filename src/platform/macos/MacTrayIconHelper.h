#ifndef MACTRAYICONHELPER_H
#define MACTRAYICONHELPER_H

class QMenu;
class QString;

namespace MacTrayIconHelper
{
QString templateIconPath();
bool configureTemplateTrayIcon(QMenu* menu, const QString& toolTip);
void cleanupTemplateTrayIcon();
bool isTemplateTrayIconConfigured();
}

#endif // MACTRAYICONHELPER_H
