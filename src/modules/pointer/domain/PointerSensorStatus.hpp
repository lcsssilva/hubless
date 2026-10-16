#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ghub::pointer {

// Estado do sensor do G703 lido por HID++ (INC-015 parte 2, ADR-0025). Só o que as capturas de 2026-10-15 sustentam.

// Faixa de DPI sem expandir os valores: {mín, máx, passo}; valor solto = mín == máx e passo 0.
struct DpiSegment {
    std::uint16_t minimum = 0;
    std::uint16_t maximum = 0;
    std::uint16_t step = 0;

    friend bool operator==(const DpiSegment&, const DpiSegment&) = default;
};

// 0x2201 fn1 (sensor 0): byte 0 = sensor; depois valores de 16 bits (MSB primeiro) até 0x0000 ou o fim. Um valor com os
// 3 bits altos em 111 é o passo (& 0x1FFF) entre o valor anterior e o seguinte. No máximo 16 valores; passo 0, faixa
// invertida, passo sem vizinho ou sensor diferente de 0 tornam a lista inválida (com os bytes brutos guardados).
class DpiList {
public:
    static constexpr std::size_t maxValues = 16;

    [[nodiscard]] static DpiList parse(std::span<const std::uint8_t> response);

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] const std::vector<DpiSegment>& segments() const noexcept { return segments_; } // vazio se inválida
    [[nodiscard]] const std::vector<std::uint8_t>& rawBytes() const noexcept { return raw_; }

private:
    DpiList() = default;

    bool valid_ = false;
    std::vector<DpiSegment> segments_;
    std::vector<std::uint8_t> raw_;
};

// 0x2201 fn2 (sensor 0): sensor, DPI atual (BE), DPI padrão (BE).
struct DpiSetting {
    std::uint16_t current = 0;
    std::uint16_t defaultDpi = 0;
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
struct PointerSensorStatus {
    std::optional<DpiList> dpiList;
    std::optional<DpiSetting> dpiSetting;
    std::optional<ReportRateSupport> reportRateSupport;
    std::optional<std::uint8_t> currentPeriodMs;
};

[[nodiscard]] shared_kernel::Result<DpiSetting> parseDpiSetting(std::span<const std::uint8_t> response);
[[nodiscard]] shared_kernel::Result<ReportRateSupport> parseReportRateSupport(std::span<const std::uint8_t> response);
[[nodiscard]] shared_kernel::Result<std::uint8_t> parseReportPeriodMs(std::span<const std::uint8_t> response);

} // namespace ghub::pointer
