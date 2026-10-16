#include "modules/lighting/domain/KeyboardStatus.hpp"

namespace ghub::lighting {

namespace {

std::uint16_t bigEndian(std::uint8_t high, std::uint8_t low)
{
    return static_cast<std::uint16_t>((static_cast<unsigned>(high) << 8U) | low);
}

shared_kernel::DomainError malformed(const char* what)
{
    return {.code = shared_kernel::ErrorCode::MalformedData, .detail = what};
}

} // namespace

shared_kernel::Result<ReportRateSupport> ReportRateSupport::fromBitmap(std::uint8_t bitmap)
{
    if (bitmap == 0) {
        return std::unexpected(malformed("o dispositivo não informou nenhum período de report rate"));
    }
    return ReportRateSupport{bitmap};
}

std::vector<std::uint8_t> ReportRateSupport::periodsMs() const
{
    std::vector<std::uint8_t> periods;
    for (unsigned bit = 0; bit < 8; ++bit) {
        if ((bitmap_ & (1U << bit)) != 0) {
            periods.push_back(static_cast<std::uint8_t>(bit + 1U));
        }
    }
    return periods;
}

bool ReportRateSupport::supports(std::uint8_t periodMs) const noexcept
{
    return periodMs >= 1 && periodMs <= 8 && (bitmap_ & (1U << (periodMs - 1U))) != 0;
}

shared_kernel::Result<BrightnessLimits> parseBrightnessLimits(std::span<const std::uint8_t> response)
{
    if (response.size() < 4) {
        return std::unexpected(malformed("resposta curta do brilho máximo"));
    }
    return BrightnessLimits{.maximum = bigEndian(response[0], response[1]), .unexplained = {response[2], response[3]}};
}

shared_kernel::Result<std::uint16_t> parseCurrentBrightness(std::span<const std::uint8_t> response)
{
    if (response.size() < 2) {
        return std::unexpected(malformed("resposta curta do brilho atual"));
    }
    return bigEndian(response[0], response[1]);
}

shared_kernel::Result<ReportRateSupport> parseReportRateSupport(std::span<const std::uint8_t> response)
{
    if (response.empty()) {
        return std::unexpected(malformed("resposta vazia dos períodos de report rate"));
    }
    return ReportRateSupport::fromBitmap(response[0]);
}

shared_kernel::Result<std::uint8_t> parseReportPeriodMs(std::span<const std::uint8_t> response)
{
    if (response.empty() || response[0] == 0) {
        return std::unexpected(malformed("período de report rate ausente ou zero"));
    }
    return response[0];
}

} // namespace ghub::lighting
