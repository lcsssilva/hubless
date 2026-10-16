#include "modules/battery/domain/AdcMeasurement.hpp"

#include <algorithm>
#include <array>

namespace ghub::battery {

namespace {

// Tabela do kernel (hid-logitech-hidpp.c@2f2f7eee, hidpp20_map_adc_measurement_1f20_capacity), adaptada pelo
// kernel do HeadsetControl (G633/G933/G935).
constexpr std::array<int, 100> voltages{
    4030, 4024, 4018, 4011, 4003, 3994, 3985, 3975, 3963, 3951, 3937, 3922, 3907, 3893, 3880, 3868, 3857,
    3846, 3837, 3828, 3820, 3812, 3805, 3798, 3791, 3785, 3779, 3773, 3768, 3762, 3757, 3752, 3747, 3742,
    3738, 3733, 3729, 3724, 3720, 3716, 3712, 3708, 3704, 3700, 3696, 3692, 3688, 3685, 3681, 3677, 3674,
    3670, 3667, 3663, 3660, 3657, 3653, 3650, 3646, 3643, 3640, 3637, 3633, 3630, 3627, 3624, 3620, 3617,
    3614, 3611, 3608, 3604, 3601, 3598, 3595, 3592, 3589, 3585, 3582, 3579, 3576, 3573, 3569, 3566, 3563,
    3560, 3556, 3553, 3550, 3546, 3543, 3539, 3536, 3532, 3529, 3525, 3499, 3466, 3433, 3399,
};

constexpr std::size_t responseSize = 3;
constexpr std::uint8_t flagDischarging = 0x01;
constexpr std::uint8_t flagCharging = 0x03;
constexpr std::uint8_t flagFull = 0x07;

ChargeState stateFromFlags(std::uint8_t flags) noexcept
{
    switch (flags) {
    case flagDischarging:
        return ChargeState::Discharging;
    case flagCharging:
        return ChargeState::Charging;
    case flagFull:
        return ChargeState::Full;
    default:
        return ChargeState::Unknown;
    }
}

} // namespace

int capacityFromMillivolts(int millivolts) noexcept
{
    if (millivolts == 0) {
        return 0;
    }
    const auto step = std::ranges::find_if(voltages, [millivolts](int threshold) { return millivolts >= threshold; });
    if (step == voltages.end()) {
        return 0;
    }
    return static_cast<int>(voltages.size()) - static_cast<int>(step - voltages.begin());
}

shared_kernel::Result<BatteryReading> readingFromAdc(std::span<const std::uint8_t> response)
{
    if (response.size() < responseSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "resposta 0x1f20 curta");
    }
    const int millivolts = (response[0] << 8U) | response[1];
    const auto level = BatteryLevel::fromPercent(capacityFromMillivolts(millivolts));
    if (!level.has_value()) {
        return std::unexpected(level.error());
    }
    return BatteryReading{stateFromFlags(response[2]), *level};
}

} // namespace ghub::battery
