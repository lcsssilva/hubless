#include "modules/pointer/domain/PointerConfiguration.hpp"

#include <algorithm>

namespace ghub::pointer {

shared_kernel::Result<PointerConfiguration> PointerConfiguration::create(std::string name,
                                                                         std::vector<PointerProfile> profiles)
{
    if (name.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "configuração sem nome");
    }
    if (profiles.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "configuração sem perfis");
    }
    const bool repeated = std::ranges::any_of(profiles, [&profiles](const PointerProfile& profile) {
        return std::ranges::count(profiles, profile.index(), &PointerProfile::index) > 1;
    });
    if (repeated) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "índice de perfil repetido");
    }
    return PointerConfiguration{std::move(name), std::move(profiles)};
}

const PointerProfile* PointerConfiguration::activeProfile() const noexcept
{
    const auto active = std::ranges::find_if(profiles_, &PointerProfile::isActive);
    return active == profiles_.end() ? nullptr : &*active;
}

std::vector<PointerWarning> PointerConfiguration::warnings() const
{
    std::vector<PointerWarning> found;
    if (activeProfile() == nullptr) {
        found.push_back(
            {.kind = PointerWarningKind::NoActiveProfile, .profileIndex = std::nullopt, .stageIndex = std::nullopt});
    }
    for (const auto& profile : profiles_) {
        if (profile.isActive() && !profile.isEnabled()) {
            found.push_back({
                .kind = PointerWarningKind::ActiveProfileDisabled,
                .profileIndex = profile.index(),
                .stageIndex = std::nullopt,
            });
        }
        for (const auto& stage : profile.stages()) {
            if (stage.isActive() && !stage.supportedDpis().empty() && !stage.isDpiListed()) {
                found.push_back({
                    .kind = PointerWarningKind::ActiveDpiNotListed,
                    .profileIndex = profile.index(),
                    .stageIndex = stage.index(),
                });
            }
        }
    }
    return found;
}

} // namespace ghub::pointer
