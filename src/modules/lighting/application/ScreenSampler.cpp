#include "modules/lighting/application/ScreenSampler.hpp"

#include "modules/lighting/domain/ScreenRegions.hpp"

#include <cstdint>

namespace ghub::lighting::application {

ScreenSampler::ScreenSampler(ScreenColorSource& screen) noexcept : screen_(&screen) {}

ScreenSampler::~ScreenSampler()
{
    stop();
}

shared_kernel::Result<void> ScreenSampler::start()
{
    return screen_->start();
}

void ScreenSampler::stop()
{
    screen_->stop();
}

RunSoftwareAnimation::FrameSource ScreenSampler::frameSource() const
{
    return [screen = screen_](std::int64_t) { return domain::screenRegionsFrame(screen->regions()); };
}

} // namespace ghub::lighting::application
