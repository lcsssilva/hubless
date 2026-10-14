#pragma once

#include "modules/lighting/domain/KeyboardEffect.hpp"

#include <array>
#include <cstdint>

namespace ghub::lighting::infrastructure {

// Efeito de hardware do G915 codificado para a feature 0x8071 fn1: ID do efeito (como o GetInfo o anuncia) e os
// dez parâmetros p0..p9 que seguem o índice do efeito no quadro.
struct G915EffectEncoding {
    std::uint16_t effectId;
    std::array<std::uint8_t, 10> params;

    friend bool operator==(const G915EffectEncoding&, const G915EffectEncoding&) = default;
};

// Codificação pura (sem I/O). Layout derivado do OpenRGB release_1.0; não confirmado em hardware.
[[nodiscard]] G915EffectEncoding encodeG915Effect(const domain::KeyboardEffect& effect);

} // namespace ghub::lighting::infrastructure
