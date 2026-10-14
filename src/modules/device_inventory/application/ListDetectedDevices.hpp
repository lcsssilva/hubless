#pragma once

#include "modules/device_inventory/application/DeviceDiscovery.hpp"
#include "modules/device_inventory/domain/DeviceCatalog.hpp"
#include "shared_kernel/Result.hpp"

#include <utility>
#include <vector>

namespace ghub::device_inventory {

// Resultado por modelo. `paths` vazio = não detectado; sem repetição e na ordem de ConnectionPath.
// Erro = nenhum caminho do modelo foi achado e alguma descoberta falhou (detecção indisponível).
struct DeviceDetection {
    DeviceModel model;
    shared_kernel::Result<std::vector<ConnectionPath>> paths;
};

// Caso de uso: uma DeviceDetection por modelo conhecido, na ordem de kDeviceModels.
class ListDetectedDevices {
public:
    // Ponteiros não nulos e não possuídos; devem sobreviver ao caso de uso.
    explicit ListDetectedDevices(std::vector<DeviceDiscovery*> discoveries) : discoveries_(std::move(discoveries)) {}

    [[nodiscard]] std::vector<DeviceDetection> execute() const;

private:
    std::vector<DeviceDiscovery*> discoveries_;
};

} // namespace ghub::device_inventory
