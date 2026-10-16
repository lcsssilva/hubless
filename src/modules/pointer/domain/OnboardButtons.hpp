#pragma once

#include "modules/pointer/domain/ButtonMapping.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace ghub::pointer {

// Decodifica uma entrada de botão de 4 bytes do setor onboard (offset 0x20 em diante). Só os formatos com evidência
// própria são interpretados:
//   80 01 00 NN  botão do mouse; NN é a máscara de um único botão (01, 02, 04, 08, 10 = botões 1 a 5);
//   80 02 MM KK  tecla HID KK com os modificadores MM (valor = MM << 8 | KK);
//   90 00 00 00  sem ação.
// Qualquer outra coisa vira ButtonAction::unrecognized com os 4 bytes (big-endian).
[[nodiscard]] ButtonAction decodeOnboardButton(std::span<const std::uint8_t, 4> entry) noexcept;

// Lê as entradas a partir do offset 0x20 até a primeira entrada não usada (4 bytes 0xFF) ou 16 entradas.
[[nodiscard]] std::vector<ButtonAction> decodeOnboardButtons(std::span<const std::uint8_t> sector);

} // namespace ghub::pointer
