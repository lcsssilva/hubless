#pragma once

#include "modules/battery/domain/BatteryReading.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <span>

namespace ghub::battery {

// Medição de bateria da feature HID++ 0x1f20 (ADC measurement): bytes 0-1 = tensão em mV (big-endian), byte 2 =
// sinalizadores. A conversão mV -> % usa a curva de descarga e o algoritmo do G HUB (ADR-0033; evidência em
// docs/device-support/evidence/2026-10-16-g733-curva-bateria-ghub.md), não a tabela do kernel. Carregando, o G HUB
// usa outra curva, que não está implementada: o valor só é confiável descarregando.
//
// Nível em %: round((E_total - E) / E_total * 100), em float, com E a energia da linha da curva escolhida (menor
// tensão >= à medida; a primeira entre repetidas) e E_total a da menor tensão (3,460 V). Fora da faixa da curva,
// satura em 0 % / 100 %; tensão 0 dá 0 %.
[[nodiscard]] int capacityFromMillivolts(int millivolts) noexcept;

// Sinalizadores: 0x01 descarregando, 0x03 carregando, 0x07 cheia, 0x0F e qualquer outro desconhecido (nesse caso o
// nível não é exposto, como em BatteryReading). Menos de 3 bytes -> MalformedData.
[[nodiscard]] shared_kernel::Result<BatteryReading> readingFromAdc(std::span<const std::uint8_t> response);

} // namespace ghub::battery
