#include "modules/profiles/application/AutomaticProfileSwitcher.hpp"

namespace ghub::profiles {

std::optional<ProfileName> AutomaticProfileSwitcher::decide(const ProfileLibrary& library,
                                                            const ApplicationId& application) const
{
    if (!library.automaticSwitching() || application == ApplicationId::ownApplication()) {
        return std::nullopt;
    }
    const Profile* target = library.profileFor(application);
    // Sem regra para o aplicativo: vale o perfil padrão, se houver (INC-017 R6).
    const ProfileName* wanted = target != nullptr                      ? &target->name()
                                : library.defaultProfile().has_value() ? &*library.defaultProfile()
                                                                       : nullptr;
    if (wanted == nullptr || (current_ && *current_ == *wanted)) {
        return std::nullopt;
    }
    return *wanted;
}

void AutomaticProfileSwitcher::noteApplied(const ProfileName& name)
{
    current_ = name;
}

} // namespace ghub::profiles
