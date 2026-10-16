#pragma once

#include "modules/lighting/domain/G915KeyCatalog.hpp"
#include "shared_kernel/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace ghub::lighting::domain {

// Teclas que o modo de jogo do G915 desativa (INC-019 R2, ADR-0029). Só teclas de teclado do catálogo (zonas 0x01 e
// 0x04, cujo índice é o código de uso HID: F1 = 0x3A na captura C-04); mídia, logo, G-keys e indicadores não entram.
// No máximo 16 códigos (um relatório longo). Lista vazia = todas as teclas reativadas. Objeto de valor imutável.
class GameModeKeys {
public:
    static constexpr std::size_t kMaxKeys = 16;

    [[nodiscard]] static shared_kernel::Result<GameModeKeys> create(std::span<const KeyId> keys);
    // Padrão sugerido, como no G HUB: Windows esquerda, Windows direita e Menu.
    [[nodiscard]] static GameModeKeys suggested();
    // Teclas do catálogo que podem ser escolhidas, na ordem do catálogo.
    [[nodiscard]] static std::span<const KeyId> eligibleKeys();
    // true se o código HID corresponde a uma tecla elegível do catálogo.
    [[nodiscard]] static bool isEligibleCode(std::uint8_t hidCode) noexcept;

    [[nodiscard]] const std::vector<std::uint8_t>& hidCodes() const noexcept { return codes_; }

    friend bool operator==(const GameModeKeys&, const GameModeKeys&) = default;

private:
    explicit GameModeKeys(std::vector<std::uint8_t> codes) : codes_(std::move(codes)) {}

    std::vector<std::uint8_t> codes_;
};

} // namespace ghub::lighting::domain
