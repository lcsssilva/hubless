#pragma once

#include "modules/lighting/domain/KeyColorMap.hpp"

#include <cstdint>
#include <vector>

namespace ghub::lighting::infrastructure {

// Quadro da feature 0x8081 (per-key lighting) do G915: função e parâmetros (até 16 bytes).
struct PerKeyFrame {
    std::uint8_t function;
    std::vector<std::uint8_t> params;

    friend bool operator==(const PerKeyFrame&, const PerKeyFrame&) = default;
};

// Código de tecla usado nos quadros 0x8081 a partir da zona/índice do catálogo (derivado do OpenRGB).
[[nodiscard]] std::uint8_t g915FrameKeyCode(std::uint8_t zone, std::uint8_t index) noexcept;

// Quadros fn6 (uma cor, até 13 teclas) e fn1 (até 4 pares tecla/cor), como o OpenRGB release_1.0 os monta.
// Não inclui o commit (fn7). Mapa vazio -> nenhum quadro.
[[nodiscard]] std::vector<PerKeyFrame> buildG915PerKeyFrames(const domain::KeyColorMap& colors);

} // namespace ghub::lighting::infrastructure
