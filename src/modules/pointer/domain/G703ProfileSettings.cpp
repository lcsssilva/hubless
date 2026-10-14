#include "modules/pointer/domain/G703ProfileSettings.hpp"

#include <algorithm>
#include <string>

namespace ghub::pointer {

namespace {

// Layout do setor (libratbag hidpp20.c).
constexpr std::size_t kReportRateByte = 0;
constexpr std::size_t kDefaultStageByte = 1;
constexpr std::size_t kShiftStageByte = 2;
constexpr std::size_t kDpiFirstByte = 3;
constexpr std::size_t kMaxStages = 5;
constexpr std::uint16_t kMinDpi = 100;
constexpr std::uint16_t kMaxDpi = 25500;
constexpr std::uint16_t kDpiStep = 50;

} // namespace

shared_kernel::Result<G703ProfileSettings> G703ProfileSettings::read(const OnboardSector& sector)
{
    using shared_kernel::ErrorCode;
    if (!sector.hasValidCrc()) {
        return shared_kernel::fail(ErrorCode::MalformedData, "CRC do setor onboard inválido");
    }
    const auto& bytes = sector.bytes();
    const std::uint8_t intervalMs = bytes.at(kReportRateByte);
    if (intervalMs != 1 && intervalMs != 2 && intervalMs != 4 && intervalMs != 8) {
        return shared_kernel::fail(ErrorCode::MalformedData,
                                   "intervalo de relatório inválido: " + std::to_string(intervalMs) + " ms");
    }
    std::vector<std::uint16_t> dpis;
    for (std::size_t i = 0; i < kMaxStages; ++i) {
        const std::size_t at = kDpiFirstByte + (2 * i);
        const auto dpi = static_cast<std::uint16_t>(bytes.at(at) | (static_cast<unsigned>(bytes.at(at + 1)) << 8U));
        if (dpi == 0) {
            break;
        }
        dpis.push_back(dpi);
    }
    if (dpis.empty()) {
        return shared_kernel::fail(ErrorCode::MalformedData, "perfil sem nenhum estágio de DPI");
    }
    const std::uint8_t defaultStage = bytes.at(kDefaultStageByte);
    if (defaultStage >= dpis.size()) {
        return shared_kernel::fail(ErrorCode::MalformedData, "estágio padrão aponta para estágio inexistente");
    }
    return G703ProfileSettings{1000U / intervalMs, std::move(dpis), defaultStage};
}

shared_kernel::Result<G703ProfileSettings>
G703ProfileSettings::make(std::uint32_t reportRateHz, std::vector<std::uint16_t> dpiStages, std::uint32_t defaultStage)
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
                "DPI deve estar entre 100 e 25500, em múltiplos de 50 (recebido: " + std::to_string(dpi) + ")");
        }
    }
    if (defaultStage >= dpiStages.size()) {
        return shared_kernel::fail(ErrorCode::InvalidArgument,
                                   "estágio padrão " + std::to_string(defaultStage) + " não existe");
    }
    return G703ProfileSettings{reportRateHz, std::move(dpiStages), defaultStage};
}

shared_kernel::Result<OnboardSector> G703ProfileSettings::applyTo(const OnboardSector& original) const
{
    if (!original.hasValidCrc()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "setor original com CRC inválido; nada foi alterado");
    }
    std::vector<std::uint8_t> raw = original.bytes();
    if (raw.at(kShiftStageByte) >= dpiStages_.size()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "o DPI do shift aponta para um estágio removido");
    }
    raw.at(kReportRateByte) = static_cast<std::uint8_t>(1000U / reportRateHz_);
    raw.at(kDefaultStageByte) = static_cast<std::uint8_t>(defaultStage_);
    // O byte 2 (estágio de DPI shift, já validado acima) e todos os demais bytes ficam como estavam.
    for (std::size_t i = 0; i < kMaxStages; ++i) {
        const std::uint16_t dpi = i < dpiStages_.size() ? dpiStages_.at(i) : std::uint16_t{0};
        raw.at(kDpiFirstByte + (2 * i)) = static_cast<std::uint8_t>(dpi & 0xFFU);
        raw.at(kDpiFirstByte + (2 * i) + 1) = static_cast<std::uint8_t>(dpi >> 8U);
    }
    auto edited = OnboardSector::fromBytes(std::move(raw));
    if (!edited) {
        return std::unexpected(edited.error());
    }
    return edited->withCrc();
}

} // namespace ghub::pointer
