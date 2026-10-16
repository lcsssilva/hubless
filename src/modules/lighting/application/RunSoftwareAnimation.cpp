#include "modules/lighting/application/RunSoftwareAnimation.hpp"

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

namespace ghub::lighting::application {

RunSoftwareAnimation::RunSoftwareAnimation(LiveKeyboardSession& session, const shared_kernel::Clock& clock,
                                           FrameSource source, unsigned framesPerSecond)
    : session_(&session), clock_(&clock), source_(std::move(source)),
      framesPerSecond_(std::clamp(framesPerSecond, 1U, kMaxFramesPerSecond)), start_(clock.now())
{
}

shared_kernel::Result<TickOutcome> RunSoftwareAnimation::tick()
{
    if (!session_->isOpen()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "sem sessão aberta");
    }
    const auto now = clock_->now();
    const auto interval = std::chrono::milliseconds{1000 / framesPerSecond_};
    if (lastTick_.has_value() && now - *lastTick_ < interval) {
        return TickOutcome::Skipped;
    }
    lastTick_ = now;

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    const auto frame = source_(elapsed);

    std::vector<std::pair<domain::KeyId, domain::RgbColor>> changed;
    for (const auto& [key, color] : frame) {
        if (lastSent_.colorOf(key) != color) {
            changed.emplace_back(key, color);
        }
    }
    if (changed.empty()) {
        return TickOutcome::Unchanged;
    }
    auto delta = domain::KeyColorMap::create(changed);
    if (!delta.has_value()) {
        return std::unexpected(delta.error());
    }
    if (auto painted = session_->paint(*delta); !painted.has_value()) {
        return std::unexpected(painted.error());
    }
    for (const auto& [key, color] : changed) {
        lastSent_ = lastSent_.with(key, color);
    }
    return TickOutcome::Sent;
}

shared_kernel::Result<void> RunSoftwareAnimation::stop()
{
    return session_->end();
}

} // namespace ghub::lighting::application
