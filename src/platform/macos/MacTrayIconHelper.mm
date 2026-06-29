#include "MacTrayIconHelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>

#import <AppKit/AppKit.h>

namespace {

NSStatusItem* statusItem = nil;
bool configured = false;

NSImage* createTemplateImage(const QString& iconPath)
{
    NSImage* templateImage = [[NSImage alloc] initWithContentsOfFile:iconPath.toNSString()];
    if (templateImage == nil) {
        return nil;
    }

    [templateImage setSize:NSMakeSize(17.0, 17.0)];
    [templateImage setTemplate:YES];
    return templateImage;
}

QString findTemplateIconPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();
    const QStringList candidates = {
        QDir::cleanPath(appDir + QStringLiteral("/../Resources/trayTemplate.png")),
        QDir::cleanPath(appDir + QStringLiteral("/icons/trayTemplate.png")),
        QDir::cleanPath(appDir + QStringLiteral("/../icons/trayTemplate.png")),
        QDir::cleanPath(appDir + QStringLiteral("/../../icons/trayTemplate.png")),
        QDir::cleanPath(currentDir + QStringLiteral("/icons/trayTemplate.png")),
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return {};
}

}

namespace MacTrayIconHelper
{

QString templateIconPath()
{
    return findTemplateIconPath();
}

bool configureTemplateTrayIcon(QMenu* menu, const QString& toolTip)
{
    if (menu == nullptr) {
        return false;
    }

    if (qGuiApp == nullptr || QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        return false;
    }

    const QString iconPath = findTemplateIconPath();
    if (iconPath.isEmpty()) {
        return false;
    }

    NSImage* image = createTemplateImage(iconPath);
    if (image == nil) {
        return false;
    }

    if (statusItem == nil) {
        statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
        [statusItem retain];
    }

    if (statusItem == nil) {
        [image release];
        return false;
    }

    NSStatusBarButton* button = [statusItem button];
    if (button == nil) {
        [image release];
        return false;
    }

    [button setImage:image];
    [button setImagePosition:NSImageOnly];
    [button setEnabled:YES];
    [button setToolTip:toolTip.toNSString()];
    [image release];

    NSMenu* nsMenu = menu->toNSMenu();
    if (nsMenu == nil) {
        return false;
    }

    [statusItem setMenu:nsMenu];
    configured = true;
    return true;
}

void cleanupTemplateTrayIcon()
{
    if (statusItem == nil) {
        configured = false;
        return;
    }

    [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
    [statusItem release];
    statusItem = nil;
    configured = false;
}

bool isTemplateTrayIconConfigured()
{
    return configured;
}

} // namespace MacTrayIconHelper
