#include "modules/profiles/domain/ProfileLibrary.hpp"

#include <algorithm>

namespace ghub::profiles {

namespace {
auto sameName(const ProfileName& name)
{
    return [&name](const Profile& profile) { return profile.name() == name; };
}
} // namespace

shared_kernel::Result<ProfileLibrary> ProfileLibrary::create(std::vector<Profile> profiles)
{
    using shared_kernel::ErrorCode;
    if (profiles.size() > maximumProfiles) {
        return shared_kernel::fail(ErrorCode::MalformedData, "mais de 50 perfis");
    }
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        const auto later = profiles.begin() + static_cast<std::ptrdiff_t>(i) + 1;
        if (std::any_of(later, profiles.end(), sameName(profiles[i].name()))) {
            return shared_kernel::fail(ErrorCode::MalformedData,
                                       "nome de perfil repetido: " + profiles[i].name().value());
        }
    }
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        for (std::size_t j = i + 1; j < profiles.size(); ++j) {
            for (const ApplicationId& application : profiles[i].applications()) {
                if (std::ranges::binary_search(profiles[j].applications(), application)) {
                    return shared_kernel::fail(ErrorCode::MalformedData,
                                               "aplicativo associado a mais de um perfil: " + application.value());
                }
            }
        }
    }
    ProfileLibrary library;
    library.profiles_ = std::move(profiles);
    return library;
}

shared_kernel::Result<void> ProfileLibrary::put(Profile profile, bool replace)
{
    using shared_kernel::ErrorCode;
    for (const ApplicationId& application : profile.applications()) {
        if (const Profile* owner = profileFor(application); owner != nullptr && owner->name() != profile.name()) {
            return shared_kernel::fail(ErrorCode::InvalidArgument, "aplicativo " + application.value() +
                                                                       " já associado ao perfil " +
                                                                       owner->name().value());
        }
    }
    const auto existing = std::ranges::find_if(profiles_, sameName(profile.name()));
    if (existing != profiles_.end()) {
        if (!replace) {
            return shared_kernel::fail(ErrorCode::InvalidArgument,
                                       "já existe um perfil com o nome " + profile.name().value());
        }
        *existing = std::move(profile);
        return {};
    }
    if (profiles_.size() >= maximumProfiles) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "limite de 50 perfis atingido");
    }
    profiles_.push_back(std::move(profile));
    return {};
}

shared_kernel::Result<void> ProfileLibrary::remove(const ProfileName& name)
{
    const auto existing = std::ranges::find_if(profiles_, sameName(name));
    if (existing == profiles_.end()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "perfil inexistente: " + name.value());
    }
    profiles_.erase(existing);
    if (defaultProfile_ == name) {
        defaultProfile_.reset();
    }
    return {};
}

shared_kernel::Result<void> ProfileLibrary::duplicate(const ProfileName& source, ProfileName copyName)
{
    using shared_kernel::ErrorCode;
    const Profile* original = find(source);
    if (original == nullptr) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "perfil inexistente: " + source.value());
    }
    auto copy = original->copiedAs(std::move(copyName));
    if (!copy) {
        return std::unexpected(copy.error());
    }
    return put(std::move(*copy), false);
}

shared_kernel::Result<void> ProfileLibrary::setDefaultProfile(std::optional<ProfileName> name)
{
    if (name.has_value() && find(*name) == nullptr) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "perfil inexistente: " + name->value());
    }
    defaultProfile_ = std::move(name);
    return {};
}

bool ProfileLibrary::isPersistent(ProfileSection section) const noexcept
{
    return persistent_[static_cast<std::size_t>(section)];
}

void ProfileLibrary::setPersistent(ProfileSection section, bool persistent) noexcept
{
    persistent_[static_cast<std::size_t>(section)] = persistent;
}

SectionSet ProfileLibrary::persistentSections() const noexcept
{
    SectionSet set;
    for (std::size_t i = 0; i < profileSectionCount; ++i) {
        set.set(i, persistent_[i]);
    }
    return set;
}

void ProfileLibrary::propagatePersistent(const Profile& source)
{
    const auto wanted = [this](ProfileSection section, bool present) { return isPersistent(section) && present; };
    const HardwareSections& from = source.hardware();
    const bool camera = wanted(ProfileSection::Camera, !source.camera().empty());
    const bool equalizer = wanted(ProfileSection::Equalizer, source.equalizer().has_value());
    const bool sidetone = wanted(ProfileSection::Sidetone, from.sidetone.has_value());
    const bool lights = wanted(ProfileSection::HeadsetLights, !from.headsetLights.empty());
    const bool effect = wanted(ProfileSection::KeyboardEffect, from.keyboardEffect.has_value());
    const bool pointer = wanted(ProfileSection::PointerDpi, from.pointerDpi.has_value());
    if (!camera && !equalizer && !sidetone && !lights && !effect && !pointer) {
        return;
    }
    for (Profile& profile : profiles_) {
        if (profile.name() == source.name()) {
            continue;
        }
        HardwareSections hardware = profile.hardware();
        if (sidetone) {
            hardware.sidetone = from.sidetone;
        }
        if (lights) {
            hardware.headsetLights = from.headsetLights;
        }
        if (effect) {
            hardware.keyboardEffect = from.keyboardEffect;
        }
        if (pointer) {
            hardware.pointerDpi = from.pointerDpi;
        }
        auto rebuilt = Profile::create(profile.name(), camera ? source.camera() : profile.camera(),
                                       equalizer ? source.equalizer() : profile.equalizer(), profile.applications(),
                                       std::move(hardware));
        if (rebuilt) {
            profile = std::move(*rebuilt);
        }
    }
}

shared_kernel::Result<void> ProfileLibrary::assignApplication(const ProfileName& name, ApplicationId application)
{
    using shared_kernel::ErrorCode;
    const auto target = std::ranges::find_if(profiles_, sameName(name));
    if (target == profiles_.end()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "perfil inexistente: " + name.value());
    }
    if (const Profile* owner = profileFor(application); owner != nullptr) {
        if (owner->name() == name) {
            return {};
        }
        return shared_kernel::fail(ErrorCode::InvalidArgument,
                                   "aplicativo já associado ao perfil " + owner->name().value());
    }
    std::vector<ApplicationId> applications = target->applications();
    applications.push_back(std::move(application));
    auto rebuilt = target->withApplications(std::move(applications));
    if (!rebuilt) {
        return std::unexpected(rebuilt.error());
    }
    *target = std::move(*rebuilt);
    return {};
}

shared_kernel::Result<void> ProfileLibrary::unassignApplication(const ApplicationId& application)
{
    const Profile* owner = profileFor(application);
    if (owner == nullptr) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "aplicativo sem perfil associado: " + application.value());
    }
    std::vector<ApplicationId> applications = owner->applications();
    std::erase(applications, application);
    auto rebuilt = owner->withApplications(std::move(applications));
    if (!rebuilt) {
        return std::unexpected(rebuilt.error());
    }
    *std::ranges::find_if(profiles_, sameName(owner->name())) = std::move(*rebuilt);
    return {};
}

const Profile* ProfileLibrary::profileFor(const ApplicationId& application) const
{
    for (const Profile& profile : profiles_) {
        if (std::ranges::binary_search(profile.applications(), application)) {
            return &profile;
        }
    }
    return nullptr;
}

const Profile* ProfileLibrary::find(const ProfileName& name) const
{
    const auto existing = std::ranges::find_if(profiles_, sameName(name));
    return existing == profiles_.end() ? nullptr : &*existing;
}

} // namespace ghub::profiles
