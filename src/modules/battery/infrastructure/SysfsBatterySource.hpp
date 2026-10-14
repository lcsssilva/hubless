#pragma once

#include "modules/battery/application/BatterySource.hpp"

#include <filesystem>
#include <utility>

namespace ghub::battery {

// Lê baterias do kernel em /sys/class/power_supply (docs/integrations/KERNEL_POWER_SUPPLY.md).
// Somente leitura. O dispositivo é identificado pelo VID:PID do diretório HID apontado por
// `device`; o nome `hidpp_battery_<n>` não é estável e é ignorado.
class SysfsBatterySource final : public BatterySource {
public:
    explicit SysfsBatterySource(std::filesystem::path classRoot = "/sys/class/power_supply")
        : classRoot_(std::move(classRoot))
    {
    }

    [[nodiscard]] shared_kernel::Result<std::vector<SourcedBatteryReading>> readAll() override;

private:
    std::filesystem::path classRoot_;
};

} // namespace ghub::battery
