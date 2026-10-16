#include "modules/headset/domain/HeadsetLighting.hpp"

namespace ghub::headset {

shared_kernel::Result<HeadsetLighting> HeadsetLighting::create(LightingZone zone, LightingEffect effect, Rgb color,
                                                               int periodMs, int brightness)
{
    switch (effect) {
    case LightingEffect::Off:
        return HeadsetLighting{zone, effect, Rgb{}, 0, 0};
    case LightingEffect::Fixed:
        return HeadsetLighting{zone, effect, color, 0, 0};
    case LightingEffect::Breathing:
    case LightingEffect::Cycle:
        break;
    }
    if (periodMs < minPeriodMs || periodMs > maxPeriodMs || periodMs % periodStepMs != 0) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "período fora de 1000-20000 ms em passos de 100");
    }
    if (brightness < minBrightness || brightness > maxBrightness) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "brilho fora de 1-100");
    }
    return HeadsetLighting{zone, effect, effect == LightingEffect::Cycle ? Rgb{} : color, periodMs, brightness};
}

HeadsetLighting HeadsetLighting::off(LightingZone zone) noexcept
{
    return HeadsetLighting{zone, LightingEffect::Off, Rgb{}, 0, 0};
}

HeadsetLighting HeadsetLighting::defaultCycle(LightingZone zone) noexcept
{
    return HeadsetLighting{zone, LightingEffect::Cycle, Rgb{}, 5000, 100};
}

std::vector<std::uint8_t> HeadsetLighting::payload() const
{
    std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(zone_), static_cast<std::uint8_t>(effect_)};
    const auto period = static_cast<std::uint16_t>(periodMs_);
    const auto periodHigh = static_cast<std::uint8_t>(period >> 8U);
    const auto periodLow = static_cast<std::uint8_t>(period & 0xFFU);
    switch (effect_) {
    case LightingEffect::Off:
        break;
    case LightingEffect::Fixed:
        bytes.insert(bytes.end(), {color_.r, color_.g, color_.b, 0x02});
        break;
    case LightingEffect::Breathing:
        bytes.insert(bytes.end(), {color_.r, color_.g, color_.b, periodHigh, periodLow, 0x00,
                                   static_cast<std::uint8_t>(brightness_)});
        break;
    case LightingEffect::Cycle:
        bytes.insert(bytes.end(),
                     {0x00, 0x00, 0x00, 0x00, 0x00, periodHigh, periodLow, static_cast<std::uint8_t>(brightness_)});
        break;
    }
    return bytes;
}

} // namespace ghub::headset
