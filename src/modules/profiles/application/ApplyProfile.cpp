#include "modules/profiles/application/ApplyProfile.hpp"

#include <string>

namespace ghub::profiles {

namespace {

using shared_kernel::ErrorCode;

constexpr auto missingModule = "módulo indisponível";

bool skipped(const SectionSet& skip, ProfileSection section)
{
    return skip.test(static_cast<std::size_t>(section));
}

std::string zoneLabel(int zone)
{
    return zone == 0 ? "inferior" : zone == 1 ? "superior" : "zona " + std::to_string(zone);
}

std::string stagesText(const std::vector<int>& stages)
{
    std::string text;
    for (const int stage : stages) {
        text += (text.empty() ? "" : "/") + std::to_string(stage);
    }
    return text;
}

} // namespace

ApplyProfile::ApplyProfile(CameraSettingsPort& camera, EqualizerSettingsPort& equalizer,
                           HardwarePorts hardware) noexcept
    : camera_(camera), equalizer_(equalizer), hardware_(hardware)
{
}

std::vector<ApplyProfile::SectionResult> ApplyProfile::applyHardware(const Profile& profile,
                                                                     const SectionSet& skip) const
{
    std::vector<SectionResult> results;
    const HardwareSections& wanted = profile.hardware();
    const auto unavailable = [] { return shared_kernel::fail(ErrorCode::BackendUnavailable, missingModule); };

    if (wanted.sidetone.has_value() && !skipped(skip, ProfileSection::Sidetone)) {
        auto outcome = hardware_.headset == nullptr ? shared_kernel::Result<std::string>{unavailable()}
                                                    : [&]() -> shared_kernel::Result<std::string> {
            auto applied = hardware_.headset->applySidetone(*wanted.sidetone);
            if (!applied) {
                return std::unexpected(applied.error());
            }
            return std::to_string(*applied);
        }();
        results.push_back({ProfileSection::Sidetone, "Sidetone do G733", std::move(outcome)});
    }
    if (!skipped(skip, ProfileSection::HeadsetLights)) {
        for (const HeadsetLightSetting& light : wanted.headsetLights) {
            shared_kernel::Result<std::string> outcome = unavailable();
            if (hardware_.headset != nullptr) {
                auto applied = hardware_.headset->applyLight(light);
                outcome = applied ? shared_kernel::Result<std::string>{"efeito " + std::to_string(light.effect)}
                                  : std::unexpected(applied.error());
            }
            results.push_back(
                {ProfileSection::HeadsetLights, "Luz do G733 (" + zoneLabel(light.zone) + ")", std::move(outcome)});
        }
    }
    if (wanted.pointerDpi.has_value() && !skipped(skip, ProfileSection::PointerDpi)) {
        shared_kernel::Result<std::string> outcome = unavailable();
        if (hardware_.pointer != nullptr) {
            auto applied = hardware_.pointer->apply(*wanted.pointerDpi);
            if (!applied) {
                outcome = std::unexpected(applied.error());
            } else {
                outcome = std::to_string(applied->setting.reportRateHz) + " Hz, " +
                          stagesText(applied->setting.dpiStages) + " DPI" +
                          (applied->wrote ? " (gravado no mouse)" : " (o mouse já tinha estes valores)");
            }
        }
        results.push_back({ProfileSection::PointerDpi, "DPI do G703", std::move(outcome)});
    }
    return results;
}

std::vector<ApplyProfile::SectionResult> ApplyProfile::applyKeyboardEffect(const Profile& profile,
                                                                           const SectionSet& skip) const
{
    std::vector<SectionResult> results;
    const auto& effect = profile.hardware().keyboardEffect;
    if (!effect.has_value() || skipped(skip, ProfileSection::KeyboardEffect)) {
        return results;
    }
    shared_kernel::Result<std::string> outcome = shared_kernel::fail(ErrorCode::BackendUnavailable, missingModule);
    if (hardware_.keyboard != nullptr) {
        auto requested = hardware_.keyboard->apply(*effect);
        outcome = requested ? shared_kernel::Result<std::string>{"efeito " + effect->kind + " pedido ao modo ao vivo"}
                            : std::unexpected(requested.error());
    }
    results.push_back({ProfileSection::KeyboardEffect, "Efeito do G915", std::move(outcome)});
    return results;
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
