#include "modules/battery/domain/HeadsetBroadcast.hpp"

namespace ghub::battery {

namespace {
constexpr std::uint8_t longReportId = 0x11;
constexpr std::uint8_t receiverDeviceIndex = 0xff;
constexpr std::uint8_t rootFeatureIndex = 0x00;
constexpr std::uint8_t errorFeatureIndex = 0xff;
constexpr std::uint8_t eventFunctionAndSoftwareId = 0x00;
} // namespace

bool isHidppBroadcastFromHeadset(std::span<const std::uint8_t> report) noexcept
{
    return report.size() >= 4 && report[0] == longReportId && report[1] == receiverDeviceIndex &&
           report[2] != rootFeatureIndex && report[2] != errorFeatureIndex && report[3] == eventFunctionAndSoftwareId;
}

} // namespace ghub::battery
