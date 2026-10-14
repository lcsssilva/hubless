#pragma once

#include "shared_kernel/Result.hpp"

#include <compare>

namespace ghub::battery {

// Percentual de carga reportado pela fonte (ABI power_supply: "0 - 100 (percent)").
// Objeto de valor imutável; só existe dentro da faixa.
class BatteryLevel {
public:
    [[nodiscard]] static shared_kernel::Result<BatteryLevel> fromPercent(int percent);

    [[nodiscard]] int percent() const noexcept { return percent_; }

    friend auto operator<=>(const BatteryLevel&, const BatteryLevel&) = default;

private:
    explicit BatteryLevel(int percent) : percent_(percent) {}

    int percent_;
};

} // namespace ghub::battery
