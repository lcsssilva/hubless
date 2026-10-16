#pragma once

#include "shared_kernel/Result.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace ghub::headset {

// Estado do G733 lido por HID++ (INC-015 parte 2, ADR-0025). Só o que as capturas sustentam; o resto fica em bytes
// brutos, sem significado atribuído.

// 0x8310 fn0: byte 0 = quantidade de bandas do EQ; bytes 1-2 NÃO interpretados.
struct EqualizerInfo {
    std::uint8_t bandCount = 0;
    std::array<std::uint8_t, 2> unexplained{};
};

// 0x8300 fn0: leitura NÃO interpretada; guarda os 4 primeiros bytes.
struct UninterpretedRead {
    std::array<std::uint8_t, 4> bytes{};
};

// Cada campo é ausente quando a feature não existe no dispositivo ("não informado").
struct HeadsetStatus {
    std::optional<EqualizerInfo> equalizer;
    std::optional<UninterpretedRead> uninterpreted;
};

// Resposta curta ou valor impossível -> MalformedData.
[[nodiscard]] shared_kernel::Result<EqualizerInfo> parseEqualizerInfo(std::span<const std::uint8_t> response);
[[nodiscard]] shared_kernel::Result<UninterpretedRead> parseUninterpretedRead(std::span<const std::uint8_t> response);

} // namespace ghub::headset
