#pragma once

#include "modules/battery/application/BatterySource.hpp"
#include "modules/battery/domain/BatteryReading.hpp"
#include "modules/device_inventory/api/DeviceInventory.hpp"
#include "shared_kernel/DomainError.hpp"

#include <array>
#include <utility>
#include <variant>
#include <vector>

namespace ghub::battery {

// Modelos cuja bateria o produto lê, na ordem de exibição. G733 entra via HeadsetControl (ADR-0007).
inline constexpr std::array kBatteryModels{
    device_inventory::DeviceModel::G915,
    device_inventory::DeviceModel::G703,
    device_inventory::DeviceModel::G733,
};

struct NotDetected {
    friend bool operator==(const NotDetected&, const NotDetected&) = default;
};

// Estado de bateria de um dispositivo suportado: leitura, ausente, ou leitura indisponível.
struct DeviceBatteryStatus {
    device_inventory::DeviceModel model;
    std::variant<BatteryReading, NotDetected, shared_kernel::DomainError> state;
};

// Caso de uso: um DeviceBatteryStatus por modelo de kBatteryModels, na mesma ordem.
class GetBatteryStatus {
public:
    // `sources`: ponteiros não nulos, que precisam sobreviver ao caso de uso.
    explicit GetBatteryStatus(std::vector<BatterySource*> sources) : sources_(std::move(sources)) {}

    [[nodiscard]] std::vector<DeviceBatteryStatus> execute() const;

private:
    std::vector<BatterySource*> sources_;
};

} // namespace ghub::battery
