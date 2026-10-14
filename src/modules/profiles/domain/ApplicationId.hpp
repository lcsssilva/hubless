#pragma once

#include "shared_kernel/Result.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace ghub::profiles {

// Aplicativo que ativa um perfil (ADR-0022, R1): classe da janela (resourceClass/desktopFileName do KWin),
// depois do trim e em minúsculas, com 1 a 128 caracteres em [a-z0-9._-]. Objeto de valor imutável.
// Nunca carrega o título da janela.
class ApplicationId {
public:
    static constexpr std::size_t maximumLength = 128;

    // Vazio, longo demais ou com caractere fora de [A-Za-z0-9._-] -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<ApplicationId> create(std::string_view raw);

    // O próprio app (resourceClass "ghub-linux"): nunca dispara troca de perfil.
    [[nodiscard]] static ApplicationId ownApplication();

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend bool operator==(const ApplicationId&, const ApplicationId&) = default;
    friend auto operator<=>(const ApplicationId&, const ApplicationId&) = default;

private:
    explicit ApplicationId(std::string value) noexcept : value_(std::move(value)) {}

    std::string value_;
};

} // namespace ghub::profiles
