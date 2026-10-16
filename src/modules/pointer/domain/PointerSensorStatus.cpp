#include "modules/pointer/domain/PointerSensorStatus.hpp"

namespace ghub::pointer {

namespace {

constexpr unsigned stepMarker = 0xE000;
constexpr unsigned stepMask = 0xE000;
constexpr unsigned stepValueMask = 0x1FFF;

std::uint16_t bigEndian(std::uint8_t high, std::uint8_t low)
{
    return static_cast<std::uint16_t>((static_cast<unsigned>(high) << 8U) | low);
}

shared_kernel::DomainError malformed(const char* what)
{
    return {.code = shared_kernel::ErrorCode::MalformedData, .detail = what};
}

struct Token {
    bool isStep;
    std::uint16_t value;
};

} // namespace

DpiList DpiList::parse(std::span<const std::uint8_t> response)
{
    DpiList list;
    list.raw_.assign(response.begin(), response.end());
    if (response.empty() || response[0] != 0x00) {
        return list;
    }
    std::vector<Token> tokens;
    for (std::size_t offset = 1; offset + 1 < response.size(); offset += 2) {
        const unsigned word = bigEndian(response[offset], response[offset + 1]);
        if (word == 0) {
            break;
        }
        if (tokens.size() >= maxValues) {
            return list;
        }
        if ((word & stepMask) == stepMarker) {
            tokens.push_back({.isStep = true, .value = static_cast<std::uint16_t>(word & stepValueMask)});
        } else {
            tokens.push_back({.isStep = false, .value = static_cast<std::uint16_t>(word)});
        }
    }
    std::vector<DpiSegment> segments;
    bool previousEndsARange = false;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (!tokens[i].isStep) {
            segments.push_back({.minimum = tokens[i].value, .maximum = tokens[i].value, .step = 0});
            previousEndsARange = false;
            continue;
        }
        // O passo liga o valor anterior (já guardado como solto) ao seguinte, que é consumido aqui. Um valor que já
        // fecha uma faixa não abre outra: esse encadeamento nunca foi capturado, então a lista vira inválida.
        const bool hasPrevious = i > 0 && !tokens[i - 1].isStep && !previousEndsARange;
        const bool hasNext = i + 1 < tokens.size() && !tokens[i + 1].isStep;
        if (!hasPrevious || !hasNext || tokens[i].value == 0 || tokens[i - 1].value > tokens[i + 1].value) {
            return list;
        }
        segments.back() = {.minimum = tokens[i - 1].value, .maximum = tokens[i + 1].value, .step = tokens[i].value};
        previousEndsARange = true;
        ++i;
    }
    list.segments_ = std::move(segments);
    list.valid_ = true;
    return list;
}

shared_kernel::Result<DpiSetting> parseDpiSetting(std::span<const std::uint8_t> response)
{
    if (response.size() < 5) {
        return std::unexpected(malformed("resposta curta do DPI atual"));
    }
    if (response[0] != 0x00) {
        return std::unexpected(malformed("resposta do DPI de outro sensor"));
    }
    return DpiSetting{.current = bigEndian(response[1], response[2]),
                      .defaultDpi = bigEndian(response[3], response[4])};
}

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

} // namespace ghub::pointer
