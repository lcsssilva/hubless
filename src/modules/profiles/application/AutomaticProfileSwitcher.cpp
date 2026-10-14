#include "modules/profiles/application/AutomaticProfileSwitcher.hpp"

namespace ghub::profiles {

std::optional<ProfileName> AutomaticProfileSwitcher::decide(const ProfileLibrary& library,
                                                            const ApplicationId& application) const
{
    if (!library.automaticSwitching() || application == ApplicationId::ownApplication()) {
        return std::nullopt;
    }
    const Profile* target = library.profileFor(application);
    if (target == nullptr || (current_ && *current_ == target->name())) {
        return std::nullopt;
    }
    return target->name();
}

void AutomaticProfileSwitcher::noteApplied(const ProfileName& name)
{
    current_ = name;
}

} // namespace ghub::profiles
