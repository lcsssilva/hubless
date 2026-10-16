#pragma once

#include "shared_kernel/Result.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace ghub::device_inventory {

// Apelido de um dispositivo, só dentro do Hubless (INC-017 R9): nada é gravado no dispositivo. Depois do trim de
// espaços ASCII tem de 1 a 32 pontos de código imprimíveis em UTF-8 válido (sem controles C0/C1). Imutável.
class DeviceNickname {
public:
    static constexpr std::size_t maximumLength = 32;

    // Vazio, longo demais, UTF-8 inválido ou com caractere de controle -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<DeviceNickname> create(std::string_view raw);

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend bool operator==(const DeviceNickname&, const DeviceNickname&) = default;

private:
    explicit DeviceNickname(std::string value) noexcept : value_(std::move(value)) {}

    std::string value_;
};

} // namespace ghub::device_inventory
