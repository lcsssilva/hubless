#include "modules/lighting/domain/HeadsetEffect.hpp"

#include <string>

namespace ghub::lighting::domain {

shared_kernel::Result<HeadsetZone> HeadsetZone::create(std::uint32_t index)
{
    if (index > 255) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "zona do headset fora da faixa 0..255: " + std::to_string(index));
    }
    return HeadsetZone{static_cast<std::uint8_t>(index)};
}

HeadsetEffect HeadsetEffect::off(HeadsetZone zone)
{
    return HeadsetEffect{zone, Off{}};
}

HeadsetEffect HeadsetEffect::staticColor(HeadsetZone zone, RgbColor color)
{
    return HeadsetEffect{zone, Static{color}};
}

HeadsetEffect HeadsetEffect::breathing(HeadsetZone zone, RgbColor color, EffectPeriod period,
                                       EffectBrightness brightness)
{
    return HeadsetEffect{zone, Breathing{.color = color, .period = period, .brightness = brightness}};
}

HeadsetEffectKind HeadsetEffect::kind() const noexcept
{
    return static_cast<HeadsetEffectKind>(value_.index());
}

std::optional<RgbColor> HeadsetEffect::color() const
{
    return std::visit(
        [](const auto& effect) -> std::optional<RgbColor> {
            if constexpr (requires { effect.color; }) {
                return effect.color;
            } else {
                return std::nullopt;
            }
        },
        value_);
}

std::optional<EffectPeriod> HeadsetEffect::period() const
{
    return std::visit(
        [](const auto& effect) -> std::optional<EffectPeriod> {
            if constexpr (requires { effect.period; }) {
                return effect.period;
            } else {
                return std::nullopt;
            }
        },
        value_);
}

std::optional<EffectBrightness> HeadsetEffect::brightness() const
{
    return std::visit(
        [](const auto& effect) -> std::optional<EffectBrightness> {
            if constexpr (requires { effect.brightness; }) {
                return effect.brightness;
            } else {
                return std::nullopt;
            }
        },
        value_);
}

} // namespace ghub::lighting::domain
