#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Tipos de ação de botão do ratbagd (doc/dbus.rst). Unknown é o tipo 1000 (ação que a libratbag não conhece).
enum class ButtonActionKind : std::uint8_t { None, Button, Special, Key, Macro, Unknown };

// Ação atribuída a um botão. `value` depende do tipo: Button = nº do botão (a partir de 1), Special = código
// bruto do ratbagd, Key = keycode, Macro = nº de passos, None/Unknown = 0. Objeto de valor imutável.
class ButtonAction {
public:
    [[nodiscard]] static ButtonAction none() noexcept { return {ButtonActionKind::None, 0}; }
    [[nodiscard]] static ButtonAction button(std::uint32_t number) noexcept
    {
        return {ButtonActionKind::Button, number};
    }
    [[nodiscard]] static ButtonAction special(std::uint32_t code) noexcept { return {ButtonActionKind::Special, code}; }
    [[nodiscard]] static ButtonAction key(std::uint32_t keycode) noexcept { return {ButtonActionKind::Key, keycode}; }
    [[nodiscard]] static ButtonAction macro(std::uint32_t steps) noexcept { return {ButtonActionKind::Macro, steps}; }
    [[nodiscard]] static ButtonAction unknown() noexcept { return {ButtonActionKind::Unknown, 0}; }

    [[nodiscard]] ButtonActionKind kind() const noexcept { return kind_; }
    [[nodiscard]] std::uint32_t value() const noexcept { return value_; }

    friend bool operator==(const ButtonAction&, const ButtonAction&) = default;

private:
    ButtonAction(ButtonActionKind kind, std::uint32_t value) noexcept : kind_(kind), value_(value) {}

    ButtonActionKind kind_;
    std::uint32_t value_;
};

// Dados brutos de um botão, ainda não validados.
struct ButtonMappingSpec {
    std::uint32_t index;
    ButtonAction action;
    std::vector<ButtonActionKind> supportedKinds; // sem repetição; Unknown não é tipo suportado

    friend bool operator==(const ButtonMappingSpec&, const ButtonMappingSpec&) = default;
};

// Botão do mouse e a ação atribuída, como o dispositivo a relata. Objeto de valor imutável; só existe válido.
class ButtonMapping {
public:
    [[nodiscard]] static shared_kernel::Result<ButtonMapping> create(ButtonMappingSpec spec);

    [[nodiscard]] std::uint32_t index() const noexcept { return spec_.index; }
    [[nodiscard]] const ButtonAction& action() const noexcept { return spec_.action; }
    [[nodiscard]] const std::vector<ButtonActionKind>& supportedKinds() const noexcept { return spec_.supportedKinds; }

    friend bool operator==(const ButtonMapping&, const ButtonMapping&) = default;

private:
    explicit ButtonMapping(ButtonMappingSpec spec) : spec_(std::move(spec)) {}

    ButtonMappingSpec spec_;
};

} // namespace ghub::pointer
