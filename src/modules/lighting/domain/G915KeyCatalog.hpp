#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ghub::lighting::domain {

class G915KeyCatalog;

// Identidade de uma tecla do G915 = posição no catálogo. Só o catálogo cria KeyId, então todo
// KeyId existente é válido. Objeto de valor imutável, ordenado pela posição no catálogo.
class KeyId {
public:
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

    friend bool operator==(const KeyId&, const KeyId&) = default;
    friend auto operator<=>(const KeyId&, const KeyId&) = default;

private:
    friend class G915KeyCatalog;
    explicit KeyId(std::size_t position) noexcept : position_(position) {}

    std::size_t position_;
};

// Entrada do catálogo: nome legível, zona e índice usados pelo OpenRGB. Dados puros, sem protocolo.
struct KeyDefinition {
    KeyId id;
    std::string_view name;
    std::uint8_t zone;
    std::uint8_t index;
};

// Catálogo das teclas do G915, derivado de led_names[] do OpenRGB release_1.0
// (RGBController_LogitechG915.cpp). Ordem do catálogo = ordem de led_names[].
class G915KeyCatalog {
public:
    // full_led_count e tkl_led_count do OpenRGB; led_names[] tem 117 entradas e as 95 primeiras
    // são as do G915 TKL (a 96ª, posição 95, é Num Lock, comentada "First Non-TKL Key").
    static constexpr std::size_t kFullLayoutKeyCount = 117;
    static constexpr std::size_t kTklKeyCount = 95;

    [[nodiscard]] static std::span<const KeyDefinition> keys() noexcept;
    [[nodiscard]] static const KeyDefinition& definition(KeyId id) noexcept;
    [[nodiscard]] static std::optional<KeyId> findByName(std::string_view name) noexcept;
    [[nodiscard]] static std::optional<KeyId> findByAddress(std::uint8_t zone, std::uint8_t index) noexcept;
    [[nodiscard]] static bool isInTklLayout(KeyId id) noexcept { return id.position() < kTklKeyCount; }
};

} // namespace ghub::lighting::domain
