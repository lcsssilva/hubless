#pragma once

#include "modules/battery/domain/BatteryReading.hpp"
#include "modules/device_inventory/api/DeviceInventory.hpp"
#include "shared_kernel/Result.hpp"

#include <vector>

namespace ghub::battery {

// Leitura de uma entrada da fonte; a falha é por entrada para não esconder os demais dispositivos.
struct SourcedBatteryReading {
    device_inventory::UsbId device;
    shared_kernel::Result<BatteryReading> reading;
};

// Port: fonte de leituras de bateria (ex.: kernel power_supply). A falha global indica que a
// fonte inteira está indisponível.
class BatterySource {
public:
    virtual ~BatterySource() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::vector<SourcedBatteryReading>> readAll() = 0;

protected:
    BatterySource() = default;
    BatterySource(const BatterySource&) = default;
    BatterySource& operator=(const BatterySource&) = default;
    BatterySource(BatterySource&&) = default;
    BatterySource& operator=(BatterySource&&) = default;
};

} // namespace ghub::battery
