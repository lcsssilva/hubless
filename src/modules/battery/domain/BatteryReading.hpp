#pragma once

#include "modules/battery/domain/BatteryLevel.hpp"
#include "modules/battery/domain/ChargeState.hpp"

#include <optional>

namespace ghub::battery {

// Leitura instantânea de bateria. Com estado de carga desconhecido o nível não é exposto como
// atual: o hid-logitech-hidpp mantém o último `capacity` após perder o link (ADR-0004).
class BatteryReading {
public:
    BatteryReading(ChargeState state, std::optional<BatteryLevel> level) : state_(state), level_(level) {}

    [[nodiscard]] ChargeState chargeState() const noexcept { return state_; }
    [[nodiscard]] std::optional<BatteryLevel> currentLevel() const noexcept;

    friend bool operator==(const BatteryReading&, const BatteryReading&) = default;

private:
    ChargeState state_;
    std::optional<BatteryLevel> level_;
};

} // namespace ghub::battery
