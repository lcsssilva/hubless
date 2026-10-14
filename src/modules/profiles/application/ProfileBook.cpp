#include "modules/profiles/application/ProfileBook.hpp"

namespace ghub::profiles {

ProfileBook::ProfileBook(ProfileRepository& repository, CameraSettingsPort& camera) noexcept
    : repository_(repository), camera_(camera)
{
}

shared_kernel::Result<void> ProfileBook::load()
{
    auto loaded = repository_.load();
    if (!loaded) {
        loadError_ = loaded.error();
        return std::unexpected(loaded.error());
    }
    library_ = std::move(*loaded);
    loadError_.reset();
    return {};
}

shared_kernel::Result<ProfileBook::SaveReport>
ProfileBook::saveCurrent(std::string_view name, std::optional<std::array<int, 10>> equalizerGains, bool replace)
{
    using shared_kernel::ErrorCode;
    if (loadError_) {
        return std::unexpected(*loadError_);
    }
    auto profileName = ProfileName::create(name);
    if (!profileName) {
        return std::unexpected(profileName.error());
    }
    // Falha cedo, antes de tocar a câmera, quando o nome já existe e não é para substituir.
    if (!replace && library_.find(*profileName) != nullptr) {
        return shared_kernel::fail(ErrorCode::InvalidArgument,
                                   "já existe um perfil com o nome " + profileName->value());
    }

    SaveReport report;
    std::vector<CameraControlSetting> cameraSection;
    auto current = camera_.readCurrent();
    if (!current) {
        report.cameraOmittedReason = current.error().detail.empty() ? "câmera indisponível" : current.error().detail;
    } else if (current->empty()) {
        report.cameraOmittedReason = "a câmera não tem controles graváveis com valor";
    } else if (current->size() > Profile::maximumCameraControls) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "a câmera tem mais de 64 controles");
    } else {
        cameraSection = std::move(*current);
        report.cameraIncluded = true;
    }

    std::optional<EqualizerGains> equalizer;
    if (equalizerGains) {
        auto gains = EqualizerGains::create(std::vector<int>(equalizerGains->begin(), equalizerGains->end()));
        if (!gains) {
            return std::unexpected(gains.error());
        }
        equalizer = *gains;
        report.equalizerIncluded = true;
    }

    // Sem seção nenhuma o Profile::create recusa com InvalidArgument e nada é gravado.
    // Substituir mantém os aplicativos associados ao perfil (ADR-0022, R2).
    std::vector<ApplicationId> applications;
    if (const Profile* existing = library_.find(*profileName); existing != nullptr) {
        applications = existing->applications();
    }
    auto profile =
        Profile::create(std::move(*profileName), std::move(cameraSection), equalizer, std::move(applications));
    if (!profile) {
        return std::unexpected(profile.error());
    }

    ProfileLibrary candidate = library_;
    if (auto put = candidate.put(std::move(*profile), replace); !put) {
        return std::unexpected(put.error());
    }
    if (auto saved = repository_.save(candidate); !saved) {
        return std::unexpected(saved.error());
    }
    library_ = std::move(candidate);
    return report;
}

shared_kernel::Result<void> ProfileBook::remove(std::string_view name)
{
    if (loadError_) {
        return std::unexpected(*loadError_);
    }
    auto profileName = ProfileName::create(name);
    if (!profileName) {
        return std::unexpected(profileName.error());
    }
    ProfileLibrary candidate = library_;
    if (auto removed = candidate.remove(*profileName); !removed) {
        return std::unexpected(removed.error());
    }
    if (auto saved = repository_.save(candidate); !saved) {
        return std::unexpected(saved.error());
    }
    library_ = std::move(candidate);
    return {};
}

shared_kernel::Result<void> ProfileBook::assignApplication(std::string_view profileName, std::string_view application)
{
    if (loadError_) {
        return std::unexpected(*loadError_);
    }
    auto name = ProfileName::create(profileName);
    if (!name) {
        return std::unexpected(name.error());
    }
    auto app = ApplicationId::create(application);
    if (!app) {
        return std::unexpected(app.error());
    }
    ProfileLibrary candidate = library_;
    if (auto assigned = candidate.assignApplication(*name, std::move(*app)); !assigned) {
        return std::unexpected(assigned.error());
    }
    return commit(std::move(candidate));
}

shared_kernel::Result<void> ProfileBook::unassignApplication(std::string_view application)
{
    if (loadError_) {
        return std::unexpected(*loadError_);
    }
    auto app = ApplicationId::create(application);
    if (!app) {
        return std::unexpected(app.error());
    }
    ProfileLibrary candidate = library_;
    if (auto removed = candidate.unassignApplication(*app); !removed) {
        return std::unexpected(removed.error());
    }
    return commit(std::move(candidate));
}

shared_kernel::Result<void> ProfileBook::setAutomaticSwitching(bool enabled)
{
    if (loadError_) {
        return std::unexpected(*loadError_);
    }
    ProfileLibrary candidate = library_;
    candidate.setAutomaticSwitching(enabled);
    return commit(std::move(candidate));
}

shared_kernel::Result<void> ProfileBook::commit(ProfileLibrary candidate)
{
    if (auto saved = repository_.save(candidate); !saved) {
        return std::unexpected(saved.error());
    }
    library_ = std::move(candidate);
    return {};
}

} // namespace ghub::profiles
