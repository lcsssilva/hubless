#pragma once

#include "modules/profiles/domain/Profile.hpp"
#include "shared_kernel/Result.hpp"

#include <cstddef>
#include <vector>

namespace ghub::profiles {

// Raiz do agregado: perfis com nome único, no máximo 50, na ordem de inserção (ADR-0020, R3).
class ProfileLibrary {
public:
    static constexpr std::size_t maximumProfiles = 50;

    ProfileLibrary() = default;

    // Reconstrução a partir de dados persistidos: nome repetido, mais de 50 perfis ou aplicativo associado a mais
    // de um perfil -> MalformedData. A troca automática começa desligada.
    [[nodiscard]] static shared_kernel::Result<ProfileLibrary> create(std::vector<Profile> profiles);

    // Nome existente sem `replace` -> InvalidArgument ("já existe"); com `replace` substitui na mesma posição.
    // O 51º perfil -> InvalidArgument. Em falha a biblioteca não muda.
    [[nodiscard]] shared_kernel::Result<void> put(Profile profile, bool replace);

    // Nome inexistente -> InvalidArgument.
    [[nodiscard]] shared_kernel::Result<void> remove(const ProfileName& name);

    // Associa um aplicativo ao perfil (R2). Perfil inexistente, aplicativo em OUTRO perfil ("já associado ao perfil
    // X") ou 33º aplicativo -> InvalidArgument. Já associado ao mesmo perfil -> sucesso sem efeito.
    [[nodiscard]] shared_kernel::Result<void> assignApplication(const ProfileName& name, ApplicationId application);

    // Aplicativo sem perfil -> InvalidArgument.
    [[nodiscard]] shared_kernel::Result<void> unassignApplication(const ApplicationId& application);

    [[nodiscard]] const Profile* profileFor(const ApplicationId& application) const;

    [[nodiscard]] bool automaticSwitching() const noexcept { return automaticSwitching_; }
    void setAutomaticSwitching(bool enabled) noexcept { automaticSwitching_ = enabled; }

    [[nodiscard]] const Profile* find(const ProfileName& name) const;
    [[nodiscard]] const std::vector<Profile>& profiles() const noexcept { return profiles_; }

private:
    std::vector<Profile> profiles_;
    bool automaticSwitching_ = false;
};

} // namespace ghub::profiles
