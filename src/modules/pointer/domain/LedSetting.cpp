#include "modules/pointer/domain/LedSetting.hpp"

#include <algorithm>
#include <string>

namespace ghub::pointer {

namespace {

constexpr std::uint8_t kFixedId = 0x01;
constexpr std::uint8_t kFixedMarker = 0x02;
constexpr std::uint8_t kCycleId = 0x03;
constexpr std::uint32_t kMaxPeriodMs = 65535;
constexpr std::uint32_t kMaxBrightness = 100;

bool allZero(std::span<const std::uint8_t> bytes)
{
    return std::ranges::all_of(bytes, [](std::uint8_t value) { return value == 0; });
}

} // namespace

LedSetting LedSetting::fixed(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
{
    return LedSetting{Kind::Fixed, {kFixedId, red, green, blue, kFixedMarker, 0, 0, 0, 0, 0, 0}};
}

shared_kernel::Result<LedSetting> LedSetting::cycle(std::uint32_t periodMs, std::uint32_t brightness)
{
    if (periodMs < 1 || periodMs > kMaxPeriodMs) {
        return shared_kernel::fail(
            shared_kernel::ErrorCode::InvalidArgument,
            "período do ciclo deve estar entre 1 e 65535 ms (recebido: " + std::to_string(periodMs) + ")");
    }
    if (brightness > kMaxBrightness) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "brilho deve estar entre 0 e 100 % (recebido: " + std::to_string(brightness) + ")");
    }
    return LedSetting{Kind::Cycle,
                      {kCycleId, 0, 0, 0, 0, 0, static_cast<std::uint8_t>(periodMs >> 8U),
                       static_cast<std::uint8_t>(periodMs & 0xFFU), static_cast<std::uint8_t>(brightness), 0, 0}};
}

LedSetting LedSetting::decode(std::span<const std::uint8_t, kEntrySize> entry) noexcept
{
    std::array<std::uint8_t, kEntrySize> bytes{};
    std::ranges::copy(entry, bytes.begin());
    const std::span<const std::uint8_t> view{bytes};
    if (allZero(view)) {
        return off();
    }
    if (bytes[0] == kFixedId && bytes[4] == kFixedMarker && allZero(view.subspan(5))) {
        return LedSetting{Kind::Fixed, bytes};
    }
    if (bytes[0] == kCycleId && allZero(view.subspan(1, 5)) && allZero(view.subspan(9)) &&
        (bytes[6] != 0 || bytes[7] != 0) && bytes[8] <= kMaxBrightness) {
        return LedSetting{Kind::Cycle, bytes};
    }
    return LedSetting{Kind::Raw, bytes};
}

} // namespace ghub::pointer
