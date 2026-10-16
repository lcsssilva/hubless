#pragma once

#include "shared_kernel/Result.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ghub::lighting {

// Estado do G915 lido por HID++ (INC-015 parte 2, ADR-0025). Só valores observados nas capturas de 2026-10-15;
// o que não foi confirmado fica em bytes brutos.

// 0x8040 fn0: bytes 0-1 = brilho máximo (BE); bytes 2-3 NÃO interpretados.
struct BrightnessLimits {
    std::uint16_t maximum = 0;
    std::array<std::uint8_t, 2> unexplained{};
};

// 0x8060 fn0: bitmap em que o bit n ligado significa período de n+1 ms suportado (1..8 ms).
class ReportRateSupport {
public:
    [[nodiscard]] static shared_kernel::Result<ReportRateSupport> fromBitmap(std::uint8_t bitmap);

    [[nodiscard]] std::vector<std::uint8_t> periodsMs() const; // crescente
    [[nodiscard]] bool supports(std::uint8_t periodMs) const noexcept;

private:
    explicit ReportRateSupport(std::uint8_t bitmap) noexcept : bitmap_(bitmap) {}
    std::uint8_t bitmap_;
};

// Cada campo é ausente quando a feature não existe no dispositivo ("não informado").
struct KeyboardStatus {
    std::optional<BrightnessLimits> brightnessLimits;
    std::optional<std::uint16_t> currentBrightness;
    std::optional<ReportRateSupport> reportRateSupport;
    std::optional<std::uint8_t> currentPeriodMs;
};

// Parsers das respostas (payload de 16 bytes ou menos). Resposta curta ou valor impossível -> MalformedData.
[[nodiscard]] shared_kernel::Result<BrightnessLimits> parseBrightnessLimits(std::span<const std::uint8_t> response);
[[nodiscard]] shared_kernel::Result<std::uint16_t> parseCurrentBrightness(std::span<const std::uint8_t> response);
[[nodiscard]] shared_kernel::Result<ReportRateSupport> parseReportRateSupport(std::span<const std::uint8_t> response);
[[nodiscard]] shared_kernel::Result<std::uint8_t> parseReportPeriodMs(std::span<const std::uint8_t> response);

} // namespace ghub::lighting
