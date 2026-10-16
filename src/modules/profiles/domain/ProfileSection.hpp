#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace ghub::profiles {

// Seções de um perfil que podem ser marcadas como "igual em todos os perfis" (INC-017 R7).
enum class ProfileSection : std::uint8_t {
    Camera,
    Equalizer,
    Sidetone,       // INC-017 parte 2: sidetone do G733
    HeadsetLights,  // luzes do G733 por zona
    KeyboardEffect, // efeito de prévia do G915
    PointerDpi,     // DPI/estágios/taxa do G703
};

inline constexpr std::size_t profileSectionCount = 6;

// Conjunto de seções, indexado por static_cast<std::size_t>(ProfileSection).
using SectionSet = std::bitset<profileSectionCount>;

} // namespace ghub::profiles
