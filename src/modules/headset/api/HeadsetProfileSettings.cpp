#include "modules/headset/api/HeadsetProfileSettings.hpp"

#include <algorithm>
#include <cstdint>

namespace ghub::headset {

namespace {

using shared_kernel::ErrorCode;

bool channelInRange(int value)
{
    return value >= 0 && value <= 255;
}

} // namespace

shared_kernel::Result<int> HeadsetProfileSettings::applySidetone(int level) const
{
    auto sidetone = SidetoneLevel::create(level);
    if (!sidetone) {
        return std::unexpected(sidetone.error());
    }
    auto applied = changeSetting_->setSidetone(*sidetone);
    if (!applied) {
        return std::unexpected(applied.error());
    }
    rememberSidetone(applied->value());
    return applied->value();
}

shared_kernel::Result<LightZoneValue> HeadsetProfileSettings::applyLight(const LightZoneValue& value) const
{
    if (value.zone < 0 || value.zone > 1) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "zona de luz do headset fora de 0-1");
    }
    if (value.effect < 0 || value.effect > 3) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "efeito de luz do headset fora de 0-3");
    }
    if (!channelInRange(value.red) || !channelInRange(value.green) || !channelInRange(value.blue)) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "cor do headset fora de 0-255");
    }
    const Rgb color{static_cast<std::uint8_t>(value.red), static_cast<std::uint8_t>(value.green),
                    static_cast<std::uint8_t>(value.blue)};
    auto lighting = HeadsetLighting::create(static_cast<LightingZone>(value.zone),
                                            static_cast<LightingEffect>(value.effect), color, value.periodMs,
                                            value.brightness);
    if (!lighting) {
        return std::unexpected(lighting.error());
    }
    auto applied = changeLighting_->apply(*lighting);
    if (!applied) {
        return std::unexpected(applied.error());
    }
    rememberLight(value);
    return value;
}

std::optional<int> HeadsetProfileSettings::lastSidetone() const
{
    const std::scoped_lock lock{mutex_};
    return lastSidetone_;
}

std::vector<LightZoneValue> HeadsetProfileSettings::lastLights() const
{
    const std::scoped_lock lock{mutex_};
    return lastLights_;
}

void HeadsetProfileSettings::rememberSidetone(int level) const
{
    const std::scoped_lock lock{mutex_};
    lastSidetone_ = level;
}

void HeadsetProfileSettings::rememberLight(const LightZoneValue& value) const
{
    const std::scoped_lock lock{mutex_};
    std::erase_if(lastLights_, [&value](const LightZoneValue& known) { return known.zone == value.zone; });
    lastLights_.push_back(value);
    std::ranges::sort(lastLights_, {}, &LightZoneValue::zone);
}

} // namespace ghub::headset
