#include "modules/pointer/domain/G703ProfileSettings.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace ghub::pointer {

namespace {

// Layout do setor (libratbag hidpp20.c para os bytes 0-12; cópias próprias para o bloco de LED).
constexpr std::size_t kReportRateByte = 0;
constexpr std::size_t kDefaultStageByte = 1;
constexpr std::size_t kShiftStageByte = 2;
constexpr std::size_t kDpiFirstByte = 3;
constexpr std::size_t kMaxStages = 5;
constexpr std::size_t kPrimaryLedOffset = 0xD0;
constexpr std::size_t kLogoLedOffset = kPrimaryLedOffset + LedSetting::kEntrySize;
constexpr std::size_t kMinSectorSize = kLogoLedOffset + LedSetting::kEntrySize;
constexpr std::uint8_t kNoShiftByte = 0xFF;
constexpr std::uint16_t kMinDpi = 100;
constexpr std::uint16_t kMaxDpi = 25600;
constexpr std::uint16_t kDpiStep = 50;

LedSetting ledAt(std::span<const std::uint8_t> sector, std::size_t offset)
{
    return LedSetting::decode(sector.subspan(offset, LedSetting::kEntrySize).first<LedSetting::kEntrySize>());
}

} // namespace

shared_kernel::Result<G703ProfileSettings> G703ProfileSettings::read(std::span<const std::uint8_t> sector)
{
    using shared_kernel::ErrorCode;
    if (sector.size() < kMinSectorSize) {
        return shared_kernel::fail(ErrorCode::MalformedData, "setor onboard curto demais para o perfil (" +
                                                                 std::to_string(sector.size()) + " bytes)");
    }
    const std::uint8_t intervalMs = sector[kReportRateByte];
    if (intervalMs != 1 && intervalMs != 2 && intervalMs != 4 && intervalMs != 8) {
        return shared_kernel::fail(ErrorCode::MalformedData,
                                   "intervalo de relatório inválido: " + std::to_string(intervalMs) + " ms");
    }
    std::vector<std::uint16_t> dpis;
    for (std::size_t i = 0; i < kMaxStages; ++i) {
        const std::size_t at = kDpiFirstByte + (2 * i);
        const auto dpi = static_cast<std::uint16_t>(sector[at] | (static_cast<unsigned>(sector[at + 1]) << 8U));
        if (dpi == 0) {
            break;
        }
        dpis.push_back(dpi);
    }
    if (dpis.empty()) {
        return shared_kernel::fail(ErrorCode::MalformedData, "perfil sem nenhum estágio de DPI");
    }
    const std::uint8_t defaultStage = sector[kDefaultStageByte];
    if (defaultStage >= dpis.size()) {
        return shared_kernel::fail(ErrorCode::MalformedData, "estágio padrão aponta para estágio inexistente");
    }
    std::optional<std::uint32_t> shift;
    if (sector[kShiftStageByte] < dpis.size()) {
        shift = sector[kShiftStageByte];
    }
    return G703ProfileSettings{
        1000U / intervalMs,           std::move(dpis), defaultStage, shift, ledAt(sector, kPrimaryLedOffset),
        ledAt(sector, kLogoLedOffset)};
}

shared_kernel::Result<G703ProfileSettings>
G703ProfileSettings::make(std::uint32_t reportRateHz, std::vector<std::uint16_t> dpiStages, std::uint32_t defaultStage,
                          std::optional<std::uint32_t> dpiShiftStage, LedSetting primaryLed, LedSetting logoLed)
{
    using shared_kernel::ErrorCode;
    if (reportRateHz != 125 && reportRateHz != 250 && reportRateHz != 500 && reportRateHz != 1000) {
        return shared_kernel::fail(
            ErrorCode::InvalidArgument,
            "taxa de relatório deve ser 125, 250, 500 ou 1000 Hz (recebido: " + std::to_string(reportRateHz) + ")");
    }
    if (dpiStages.empty() || dpiStages.size() > kMaxStages) {
        return shared_kernel::fail(
            ErrorCode::InvalidArgument,
            "o perfil deve ter de 1 a 5 estágios de DPI (recebido: " + std::to_string(dpiStages.size()) + ")");
    }
    for (const std::uint16_t dpi : dpiStages) {
        if (dpi < kMinDpi || dpi > kMaxDpi || dpi % kDpiStep != 0) {
            return shared_kernel::fail(
                ErrorCode::InvalidArgument,
                "DPI deve estar entre 100 e 25600, em múltiplos de 50 (recebido: " + std::to_string(dpi) + ")");
        }
    }
    if (defaultStage >= dpiStages.size()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument,
                                   "estágio padrão " + std::to_string(defaultStage) + " não existe");
    }
    if (dpiShiftStage.has_value() && *dpiShiftStage >= dpiStages.size()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument,
                                   "estágio do DPI shift " + std::to_string(*dpiShiftStage) + " não existe");
    }
    return G703ProfileSettings{reportRateHz, std::move(dpiStages), defaultStage, dpiShiftStage, primaryLed, logoLed};
}

shared_kernel::Result<std::vector<std::uint8_t>>
G703ProfileSettings::applyTo(std::span<const std::uint8_t> original) const
{
    if (original.size() < kMinSectorSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "setor onboard curto demais para o perfil (" + std::to_string(original.size()) +
                                       " bytes)");
    }
    std::vector<std::uint8_t> raw{original.begin(), original.end()};
    raw.at(kReportRateByte) = static_cast<std::uint8_t>(1000U / reportRateHz_);
    raw.at(kDefaultStageByte) = static_cast<std::uint8_t>(defaultStage_);
    // Shift: mantém o byte quando ele já vale o pedido; "nenhum" só escreve 0xFF se o byte original passaria a
    // apontar para um estágio da lista nova.
    if (dpiShiftStage_.has_value()) {
        raw.at(kShiftStageByte) = static_cast<std::uint8_t>(*dpiShiftStage_);
    } else if (original[kShiftStageByte] < dpiStages_.size()) {
        raw.at(kShiftStageByte) = kNoShiftByte;
    }
    for (std::size_t i = 0; i < kMaxStages; ++i) {
        const std::uint16_t dpi = i < dpiStages_.size() ? dpiStages_.at(i) : std::uint16_t{0};
        raw.at(kDpiFirstByte + (2 * i)) = static_cast<std::uint8_t>(dpi & 0xFFU);
        raw.at(kDpiFirstByte + (2 * i) + 1) = static_cast<std::uint8_t>(dpi >> 8U);
    }
    // Um LED igual ao já gravado não toca seus bytes (também mantém bytes de formatos desconhecidos).
    const std::array<std::pair<std::size_t, const LedSetting*>, 2> leds{{
        {kPrimaryLedOffset, &primaryLed_},
        {kLogoLedOffset, &logoLed_},
    }};
    for (const auto& [offset, led] : leds) {
        if (ledAt(original, offset) != *led) {
            const auto bytes = led->encode();
            std::ranges::copy(bytes, raw.begin() + static_cast<std::ptrdiff_t>(offset));
        }
    }
    return raw;
}

} // namespace ghub::pointer
