#pragma once

#include "modules/profiles/domain/ApplicationId.hpp"
#include "modules/profiles/domain/CameraControlSetting.hpp"
#include "modules/profiles/domain/EqualizerGains.hpp"
#include "modules/profiles/domain/ProfileName.hpp"
#include "shared_kernel/Result.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace ghub::profiles {

// Perfil: nome e ao menos uma seção (câmera e/ou EQ). Entidade identificada pelo nome (ADR-0020, R2).
class Profile {
public:
    static constexpr std::size_t maximumCameraControls = 64;
    static constexpr std::size_t maximumControlNameBytes = 64;
    static constexpr std::size_t maximumApplications = 32;

    // Câmera vazia = sem seção de câmera. Sem nenhuma seção, mais de 64 controles, ids repetidos ou nome de
    // controle com mais de 64 bytes -> InvalidArgument. A seção de câmera é guardada em ordem crescente de id.
    [[nodiscard]] static shared_kernel::Result<Profile>
    create(ProfileName name, std::vector<CameraControlSetting> camera, std::optional<EqualizerGains> equalizer);

    // Com aplicativos associados (ADR-0022, R2): 0 a 32, sem repetição; guardados em ordem crescente.
    // Mais de 32 ou repetido -> InvalidArgument. As demais regras são as da sobrecarga acima.
    [[nodiscard]] static shared_kernel::Result<Profile> create(ProfileName name,
                                                               std::vector<CameraControlSetting> camera,
                                                               std::optional<EqualizerGains> equalizer,
                                                               std::vector<ApplicationId> applications);

    [[nodiscard]] const ProfileName& name() const noexcept { return name_; }
    [[nodiscard]] const std::vector<CameraControlSetting>& camera() const noexcept { return camera_; }
    [[nodiscard]] const std::optional<EqualizerGains>& equalizer() const noexcept { return equalizer_; }

    [[nodiscard]] const std::vector<ApplicationId>& applications() const noexcept { return applications_; }

    friend bool operator==(const Profile&, const Profile&) = default;

private:
    Profile(ProfileName name, std::vector<CameraControlSetting> camera, std::optional<EqualizerGains> equalizer,
            std::vector<ApplicationId> applications)
        : name_(std::move(name)), camera_(std::move(camera)), equalizer_(equalizer),
          applications_(std::move(applications))
    {
    }

    ProfileName name_;
    std::vector<CameraControlSetting> camera_;
    std::optional<EqualizerGains> equalizer_;
    std::vector<ApplicationId> applications_;
};

} // namespace ghub::profiles
