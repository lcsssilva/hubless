#pragma once

#include "modules/profiles/domain/ApplicationId.hpp"
#include "modules/profiles/domain/ProfileLibrary.hpp"
#include "modules/profiles/domain/ProfileName.hpp"

#include <optional>

namespace ghub::profiles {

// Decisão pura da troca automática (ADR-0022, R4): sem Qt, sem relógio. A espera de 500 ms e a coalescência
// de eventos ficam na apresentação.
class AutomaticProfileSwitcher {
public:
    // nullopt se a troca está desligada, se o aplicativo é o próprio app, se não há regra nem perfil padrão ou se o
    // perfil decidido (da regra, ou o padrão sem regra) já é o atual.
    [[nodiscard]] std::optional<ProfileName> decide(const ProfileLibrary& library,
                                                    const ApplicationId& application) const;

    // Registra o perfil aplicado, manual ou automaticamente.
    void noteApplied(const ProfileName& name);

    [[nodiscard]] const std::optional<ProfileName>& current() const noexcept { return current_; }

private:
    std::optional<ProfileName> current_;
};

} // namespace ghub::profiles
