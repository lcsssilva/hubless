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
    using shared_kernel::ErrorCode;
    if (camera.empty() && !equalizer.has_value()) {
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
    return Profile{std::move(name), std::move(camera), equalizer, std::move(applications)};
}

} // namespace ghub::profiles
