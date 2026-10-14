#pragma once

#include <cstdint>

namespace ghub::battery {

// Valores de /sys/class/power_supply/<n>/status (ABI sysfs-class-power, kernel v7.2.9).
enum class ChargeState : std::uint8_t {
    Unknown,
    Charging,
    Discharging,
    NotCharging,
    Full,
};

} // namespace ghub::battery
