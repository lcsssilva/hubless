#include "modules/lighting/application/SyncColorTargets.hpp"

namespace ghub::lighting::application {

shared_kernel::Result<void> KeyboardEffectTarget::applyFixed(const domain::RgbColor& color)
{
    auto applied = settings_->apply({.kind = "static", .red = color.red(), .green = color.green(),
                                     .blue = color.blue(), .brightness = 100});
    if (!applied) {
        return std::unexpected(applied.error());
    }
    return {};
}

shared_kernel::Result<void> HeadsetFixedTarget::applyFixed(const domain::RgbColor& color)
{
    std::optional<shared_kernel::DomainError> firstError;
    for (const int zone : {0, 1}) {
        auto applied = settings_->applyLight({.zone = zone, .effect = 1, .red = color.red(), .green = color.green(),
                                              .blue = color.blue(), .periodMs = 0, .brightness = 0});
        if (!applied && !firstError.has_value()) {
            firstError = applied.error();
        }
    }
    if (firstError.has_value()) {
        return std::unexpected(*firstError);
    }
    return {};
}

shared_kernel::Result<void> MouseLedTarget::applyFixed(const domain::RgbColor& color)
{
    auto applied = settings_->applyFixed({.red = color.red(), .green = color.green(), .blue = color.blue()});
    if (!applied) {
        return std::unexpected(applied.error());
    }
    return {};
}

} // namespace ghub::lighting::application
