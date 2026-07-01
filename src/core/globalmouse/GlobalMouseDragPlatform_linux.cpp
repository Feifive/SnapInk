#include "GlobalMouseDragPlatform.h"

#include <QString>

namespace
{
class UnsupportedGlobalMouseDragPlatform final : public GlobalMouseDragPlatform
{
public:
    explicit UnsupportedGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
        : GlobalMouseDragPlatform(monitor)
    {
    }

    bool start(QString* errorReason) override
    {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("Global mouse drag is not implemented on Linux yet.");
        }
        return false;
    }

    void stop() override
    {
    }
};
}

GlobalMouseDragPlatform* createGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
{
    return new UnsupportedGlobalMouseDragPlatform(monitor);
}
