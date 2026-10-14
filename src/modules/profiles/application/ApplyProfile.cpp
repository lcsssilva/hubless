#include "modules/profiles/application/ApplyProfile.hpp"

namespace ghub::profiles {

ApplyProfile::ApplyProfile(CameraSettingsPort& camera, EqualizerSettingsPort& equalizer) noexcept
    : camera_(camera), equalizer_(equalizer)
{
}

std::vector<ApplyProfile::CameraItemResult> ApplyProfile::applyCamera(const Profile& profile) const
{
    std::vector<CameraItemResult> results;
    results.reserve(profile.camera().size());
    // Profile::camera() já vem em ordem crescente de id (o automático antes do dependente).
    for (const CameraControlSetting& setting : profile.camera()) {
        results.push_back({.id = setting.id,
                           .name = setting.name,
                           .requested = setting.value,
                           .outcome = camera_.apply(setting.id, setting.value)});
    }
    return results;
}

std::optional<shared_kernel::Result<void>> ApplyProfile::applyEqualizer(const Profile& profile) const
{
    if (!profile.equalizer().has_value()) {
        return std::nullopt;
    }
    return equalizer_.requestGains(profile.equalizer()->values());
}

} // namespace ghub::profiles
