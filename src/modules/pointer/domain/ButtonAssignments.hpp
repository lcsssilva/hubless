#pragma once

#include "modules/pointer/domain/ButtonMapping.hpp"
#include "shared_kernel/Result.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Tecla HID aceita nas atribuições do setor onboard (lista fechada: letras, dígitos, F1-F12, Enter, Esc, Backspace,
// Tab, Espaço, setas, Home/End/PgUp/PgDn, Insert/Delete). `code` é o uso HID (página Keyboard/Keypad).
struct OnboardKey {
    std::uint8_t code;
    std::string_view label;
};

[[nodiscard]] std::span<const OnboardKey> onboardKeyCatalog() noexcept;

// O que gravar numa entrada de botão de 4 bytes (INC-022 R2/R4). Formatos com evidência própria: `80 01 00 NN`
// (botão do mouse, NN = máscara de 1 bit) e `80 02 MM KK` (tecla KK com modificadores MM; Ctrl+C = 80 02 01 06).
// Keep = não toca a entrada (preserva também formatos não interpretados); FactoryDefault = copia a entrada da cópia
// original. Objeto de valor imutável; só existe válido.
class ButtonAssignment {
public:
    enum class Kind : std::uint8_t { Keep, FactoryDefault, MouseButton, Key };
    using Entry = std::array<std::uint8_t, 4>;

    [[nodiscard]] static ButtonAssignment keep() noexcept { return {Kind::Keep, {}}; }
    [[nodiscard]] static ButtonAssignment factoryDefault() noexcept { return {Kind::FactoryDefault, {}}; }
    // Botão 1..5.
    [[nodiscard]] static shared_kernel::Result<ButtonAssignment> mouseButton(std::uint32_t number);
    // Modificadores: bitmap HID dos quatro da esquerda (01 Ctrl, 02 Shift, 04 Alt, 08 Super); tecla do catálogo.
    [[nodiscard]] static shared_kernel::Result<ButtonAssignment> key(std::uint32_t modifiers, std::uint32_t keycode);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    // Os 4 bytes a gravar; só significativo para MouseButton e Key (zeros nos demais).
    [[nodiscard]] Entry entry() const noexcept { return entry_; }

    friend bool operator==(const ButtonAssignment&, const ButtonAssignment&) = default;

private:
    ButtonAssignment(Kind kind, Entry entry) noexcept : kind_(kind), entry_(entry) {}

    Kind kind_;
    Entry entry_;
};

// Atribuições dos 6 botões do G703 (offset 0x20, 4 bytes cada). O botão 1 só muda para algo diferente de "botão do
// mouse 1"/padrão/manter com confirmação explícita (R3). Objeto de valor imutável; só existe válido.
class ButtonAssignments {
public:
    static constexpr std::size_t kCount = 6;

    [[nodiscard]] static ButtonAssignments keepAll();
    [[nodiscard]] static shared_kernel::Result<ButtonAssignments> make(std::vector<ButtonAssignment> assignments,
                                                                       bool confirmPrimaryChange);

    // Os botões atuais do setor (as 6 entradas devem estar em uso).
    [[nodiscard]] static shared_kernel::Result<std::vector<ButtonAction>> read(std::span<const std::uint8_t> sector);

    [[nodiscard]] bool needsReference() const noexcept;
    [[nodiscard]] const std::vector<ButtonAssignment>& assignments() const noexcept { return assignments_; }

    // Cópia de `original` com só as entradas editadas trocadas; "padrão" copia a entrada de `reference` (a cópia
    // original do setor). Os demais bytes ficam como estavam (o CRC é do chamador).
    [[nodiscard]] shared_kernel::Result<std::vector<std::uint8_t>>
    applyTo(std::span<const std::uint8_t> original, std::span<const std::uint8_t> reference) const;

    friend bool operator==(const ButtonAssignments&, const ButtonAssignments&) = default;

private:
    explicit ButtonAssignments(std::vector<ButtonAssignment> assignments) : assignments_(std::move(assignments)) {}

    std::vector<ButtonAssignment> assignments_;
};

} // namespace ghub::pointer
