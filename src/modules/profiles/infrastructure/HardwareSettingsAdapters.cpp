#include "modules/profiles/infrastructure/HardwareSettingsAdapters.hpp"

#include <utility>

namespace ghub::profiles {

namespace {

headset::LightZoneValue toOwner(const HeadsetLightSetting& light)
{
    return {.zone = light.zone, .effect = light.effect, .red = light.red, .green = light.green, .blue = light.blue,
            .periodMs = light.periodMs, .brightness = light.brightness};
}

HeadsetLightSetting fromOwner(const headset::LightZoneValue& light)
{
    return {.zone = light.zone, .effect = light.effect, .red = light.red, .green = light.green, .blue = light.blue,
            .periodMs = light.periodMs, .brightness = light.brightness};
}

lighting::KeyboardEffectValue toOwner(const KeyboardEffectSetting& effect)
{
    return {.kind = effect.kind, .red = effect.red, .green = effect.green, .blue = effect.blue,
            .brightness = effect.brightness};
}

pointer::G703DpiValue toOwner(const PointerDpiSetting& setting)
{
    return {.reportRateHz = setting.reportRateHz, .dpiStages = setting.dpiStages, .defaultStage = setting.defaultStage,
            .dpiShiftStage = setting.dpiShiftStage};
}

PointerDpiSetting fromOwner(const pointer::G703DpiValue& value)
{
    return {.reportRateHz = value.reportRateHz, .dpiStages = value.dpiStages, .defaultStage = value.defaultStage,
            .dpiShiftStage = value.dpiShiftStage};
}

} // namespace

std::optional<int> HeadsetSettingsAdapter::lastSidetone() const
{
    return settings_->lastSidetone();
}

std::vector<HeadsetLightSetting> HeadsetSettingsAdapter::lastLights() const
{
    std::vector<HeadsetLightSetting> lights;
    for (const headset::LightZoneValue& light : settings_->lastLights()) {
        lights.push_back(fromOwner(light));
    }
    return lights;
}

shared_kernel::Result<int> HeadsetSettingsAdapter::applySidetone(int level)
{
    return settings_->applySidetone(level);
}

shared_kernel::Result<void> HeadsetSettingsAdapter::applyLight(const HeadsetLightSetting& light)
{
    auto applied = settings_->applyLight(toOwner(light));
    if (!applied) {
        return std::unexpected(applied.error());
    }
    return {};
}

std::optional<KeyboardEffectSetting> KeyboardEffectAdapter::last() const
{
    const auto effect = settings_->last();
    if (!effect.has_value()) {
        return std::nullopt;
    }
    return KeyboardEffectSetting{.kind = effect->kind, .red = effect->red, .green = effect->green,
                                 .blue = effect->blue, .brightness = effect->brightness};
}

shared_kernel::Result<void> KeyboardEffectAdapter::apply(const KeyboardEffectSetting& effect)
{
    auto applied = settings_->apply(toOwner(effect));
    if (!applied) {
        return std::unexpected(applied.error());
    }
    return {};
}

shared_kernel::Result<PointerDpiSetting> PointerSettingsAdapter::readCurrent()
{
    auto current = settings_->readCurrent();
    if (!current) {
        return std::unexpected(current.error());
    }
    return fromOwner(*current);
}

shared_kernel::Result<PointerDpiApplied> PointerSettingsAdapter::apply(const PointerDpiSetting& setting)
{
    auto applied = settings_->apply(toOwner(setting));
    if (!applied) {
        return std::unexpected(applied.error());
    }
    return PointerDpiApplied{.setting = fromOwner(applied->value), .wrote = applied->wrote};
}

} // namespace ghub::profiles
