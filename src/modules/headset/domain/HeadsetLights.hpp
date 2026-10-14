#pragma once

#include <cstdint>

namespace ghub::headset {

// Estado das luzes do headset que o produto sabe pedir: ligar (efeito respiração do HeadsetControl) ou desligar.
enum class HeadsetLights : std::uint8_t { On, Off };

} // namespace ghub::headset
