#pragma once

#include <cstdint>

namespace ghub::profiles {

// Seções de um perfil que podem ser marcadas como "igual em todos os perfis" (INC-017 R7). Câmera e EQ existem
// hoje; as demais entram na parte 2.
enum class ProfileSection : std::uint8_t {
    Camera,
    Equalizer,
};

} // namespace ghub::profiles
