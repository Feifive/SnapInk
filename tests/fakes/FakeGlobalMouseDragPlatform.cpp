#include "../../src/core/globalmouse/GlobalMouseDragPlatform.h"

class FakeGlobalMouseDragPlatform final : public GlobalMouseDragPlatform
{
public:
    explicit FakeGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
        : GlobalMouseDragPlatform(monitor)
    {
    }

    bool start(QString* errorReason) override
    {
        if (errorReason != nullptr) {
            errorReason->clear();
        }
        return true;
    }

    void stop() override {}
};

GlobalMouseDragPlatform* createGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
{
    return new FakeGlobalMouseDragPlatform(monitor);
}
