#pragma once

#include "modules/battery/domain/BatteryReading.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <span>

namespace ghub::battery {

// Medição de bateria da feature HID++ 0x1f20 (ADC measurement): bytes 0-1 = tensão em mV (big-endian), byte 2 =
// sinalizadores. Conversão e sinalizadores conforme o kernel Linux, drivers/hid/hid-logitech-hidpp.c linhas
// 1862-1932 (hidpp20_map_adc_measurement_1f20_capacity: tabela e busca; hidpp20_map_adc_measurement_1f20:
// sinalizadores e tensão big-endian), master
// 2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e, verificado em 2026-10-15. A curva é uma estimativa (o kernel avisa que
// pode não servir a todo dispositivo) e pode diferir do que o G HUB mostra.
//
// Nível em % pela tabela de 100 degraus do kernel: o primeiro degrau cuja tensão é <= à medida dá 100 - índice;
// abaixo do último (3399 mV) ou com tensão 0, 0.
[[nodiscard]] int capacityFromMillivolts(int millivolts) noexcept;

// Sinalizadores: 0x01 descarregando, 0x03 carregando, 0x07 cheia, 0x0F e qualquer outro desconhecido (nesse caso o
// nível não é exposto, como em BatteryReading). Menos de 3 bytes -> MalformedData.
[[nodiscard]] shared_kernel::Result<BatteryReading> readingFromAdc(std::span<const std::uint8_t> response);

} // namespace ghub::battery
