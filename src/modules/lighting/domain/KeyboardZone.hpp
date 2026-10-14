#pragma once

#include <cstdint>

namespace ghub::lighting::domain {

// Zona de iluminação do G915 que recebe um efeito de hardware: o bloco de teclas ou o logo.
// No G915 desta unidade são dois clusters da feature 0x8071 (INC-008 parte D).
enum class KeyboardZone : std::uint8_t { Keys, Logo };

} // namespace ghub::lighting::domain
