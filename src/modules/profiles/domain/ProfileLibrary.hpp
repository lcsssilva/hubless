#pragma once

#include "modules/profiles/domain/Profile.hpp"
#include "modules/profiles/domain/ProfileSection.hpp"
#include "shared_kernel/Result.hpp"

#include <array>
#include <cstddef>
#include <optional>
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

    // Copia as seções de `source` para um perfil novo `copyName`, sem aplicativos associados (INC-017 R4).
    // Origem inexistente, nome já existente ou 51º perfil -> InvalidArgument; em falha a biblioteca não muda.
    [[nodiscard]] shared_kernel::Result<void> duplicate(const ProfileName& source, ProfileName copyName);

    // Perfil padrão da troca automática (INC-017 R6): aplicado quando o aplicativo em foco não casa com nenhum
    // perfil. nullopt = nenhum. Perfil inexistente -> InvalidArgument. Remover o perfil limpa o padrão.
    [[nodiscard]] const std::optional<ProfileName>& defaultProfile() const noexcept { return defaultProfile_; }
    [[nodiscard]] shared_kernel::Result<void> setDefaultProfile(std::optional<ProfileName> name);

    // Seções "iguais em todos os perfis" (INC-017 R7). `propagatePersistent` copia as seções persistentes
    // NÃO vazias de `source` para todos os outros perfis (chamado ao salvar).
    [[nodiscard]] bool isPersistent(ProfileSection section) const noexcept;
    void setPersistent(ProfileSection section, bool persistent) noexcept;
    [[nodiscard]] SectionSet persistentSections() const noexcept;
    void propagatePersistent(const Profile& source);

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
    std::optional<ProfileName> defaultProfile_;
    std::array<bool, profileSectionCount> persistent_{};
};

} // namespace ghub::profiles
