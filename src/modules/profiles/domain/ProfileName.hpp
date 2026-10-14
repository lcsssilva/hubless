#pragma once

#include "shared_kernel/Result.hpp"

#include <string>
#include <string_view>

namespace ghub::profiles {

// Nome de um perfil: objeto de valor imutável (ADR-0020, R1). Depois do trim de espaços ASCII tem de 1 a 64
// pontos de código em UTF-8 válido, sem caracteres de controle.
class ProfileName {
public:
    static constexpr std::size_t maximumLength = 64;

    // Texto vazio, longo demais, UTF-8 inválido ou com controle -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<ProfileName> create(std::string_view raw);

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend bool operator==(const ProfileName&, const ProfileName&) = default;

private:
    explicit ProfileName(std::string value) noexcept : value_(std::move(value)) {}

    std::string value_;
};

} // namespace ghub::profiles
