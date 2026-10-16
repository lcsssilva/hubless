#pragma once

#include <cstdint>
#include <span>

namespace ghub::battery {

// Evento espontâneo do G733 (HID++ 2.0, feature 0x1f20): relatório longo (0x11) do índice 0xff, com índice de feature
// diferente de Root (0x00) e do erro (0xff), função 0 e software id 0 (byte 3 == 0x00). Respostas aos pedidos do app
// trazem software id próprio no byte 3. Função pura; não interpreta o payload.
[[nodiscard]] bool isHidppBroadcastFromHeadset(std::span<const std::uint8_t> report) noexcept;

} // namespace ghub::battery
