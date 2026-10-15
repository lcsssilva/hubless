#pragma once

#include <cstdint>
#include <string>

namespace ghub::device_inventory {

// Tipo de dispositivo informado pela feature HID++ 0x0005 fn2 (spec HID++ 2.0 de 2012, p. 8): 0 teclado,
// 1 controle remoto, 2 teclado numérico, 3 mouse, 4 touchpad, 5 trackball, 6 apresentador, 7 receptor.
// Qualquer outro valor (o G733 informa 8) vira "outro (N)".
[[nodiscard]] std::string deviceTypeText(std::uint8_t type);

} // namespace ghub::device_inventory
