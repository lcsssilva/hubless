#include "modules/profiles/domain/Profile.hpp"

#include <algorithm>

namespace ghub::profiles {

shared_kernel::Result<Profile> Profile::create(ProfileName name, std::vector<CameraControlSetting> camera,
                                               std::optional<EqualizerGains> equalizer)
{
    return create(std::move(name), std::move(camera), equalizer, {});
}

shared_kernel::Result<Profile> Profile::create(ProfileName name, std::vector<CameraControlSetting> camera,
                                               std::optional<EqualizerGains> equalizer,
                                               std::vector<ApplicationId> applications)
{
    return create(std::move(name), std::move(camera), equalizer, std::move(applications), HardwareSections{});
}

namespace {
constexpr std::size_t maximumHeadsetZones = 2;
constexpr std::size_t maximumEffectKindBytes = 16;
constexpr std::size_t maximumDpiStages = 5;

shared_kernel::Result<void> checkHardware(HardwareSections& hardware)
{
    using shared_kernel::ErrorCode;
    if (hardware.headsetLights.size() > maximumHeadsetZones) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "mais de 2 zonas de luz do headset no perfil");
    }
    std::ranges::sort(hardware.headsetLights, {}, &HeadsetLightSetting::zone);
    if (std::ranges::adjacent_find(hardware.headsetLights, {}, &HeadsetLightSetting::zone) !=
        hardware.headsetLights.end()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "zona de luz do headset repetida");
    }
    if (hardware.keyboardEffect.has_value() &&
        (hardware.keyboardEffect->kind.empty() || hardware.keyboardEffect->kind.size() > maximumEffectKindBytes)) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "tipo de efeito do teclado inválido");
    }
    if (hardware.pointerDpi.has_value()) {
        const PointerDpiSetting& dpi = *hardware.pointerDpi;
        const auto stages = static_cast<long long>(dpi.dpiStages.size());
        if (dpi.dpiStages.empty() || dpi.dpiStages.size() > maximumDpiStages) {
            return shared_kernel::fail(ErrorCode::InvalidArgument, "o DPI do mouse precisa de 1 a 5 estágios");
        }
        if (dpi.defaultStage < 0 || dpi.defaultStage >= stages) {
            return shared_kernel::fail(ErrorCode::InvalidArgument, "estágio padrão do DPI inexistente");
        }
        if (dpi.dpiShiftStage.has_value() && (*dpi.dpiShiftStage < 0 || *dpi.dpiShiftStage >= stages)) {
            return shared_kernel::fail(ErrorCode::InvalidArgument, "estágio do DPI shift inexistente");
        }
    }
    return {};
}
} // namespace

shared_kernel::Result<Profile> Profile::create(ProfileName name, std::vector<CameraControlSetting> camera,
                                               std::optional<EqualizerGains> equalizer,
                                               std::vector<ApplicationId> applications, HardwareSections hardware)
{
    using shared_kernel::ErrorCode;
    if (camera.empty() && !equalizer.has_value() && hardware.empty()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "o perfil precisa de ao menos uma seção");
    }
    if (camera.size() > maximumCameraControls) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "mais de 64 controles de câmera no perfil");
    }
    if (std::ranges::any_of(camera,
                            [](const CameraControlSetting& c) { return c.name.size() > maximumControlNameBytes; })) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "nome de controle com mais de 64 bytes");
    }
    std::ranges::sort(camera, {}, &CameraControlSetting::id);
    if (std::ranges::adjacent_find(camera, {}, &CameraControlSetting::id) != camera.end()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "id de controle de câmera repetido");
    }
    if (applications.size() > maximumApplications) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "mais de 32 aplicativos no perfil");
    }
    std::ranges::sort(applications);
    if (std::ranges::adjacent_find(applications) != applications.end()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "aplicativo repetido no perfil");
    }
    if (auto checked = checkHardware(hardware); !checked) {
        return std::unexpected(checked.error());
    }
    return Profile{std::move(name), std::move(camera), equalizer, std::move(applications), std::move(hardware)};
}

shared_kernel::Result<Profile> Profile::withApplications(std::vector<ApplicationId> applications) const
{
    return create(name_, camera_, equalizer_, std::move(applications), hardware_);
}

shared_kernel::Result<Profile> Profile::withoutApplications() const
{
    return withApplications({});
}

shared_kernel::Result<Profile> Profile::copiedAs(ProfileName name) const
{
    return create(std::move(name), camera_, equalizer_, {}, hardware_);
}

} // namespace ghub::profiles
