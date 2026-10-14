#include "modules/device_inventory/application/ListDetectedDevices.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace ghub::device_inventory {

std::vector<DeviceDetection> ListDetectedDevices::execute() const
{
    std::vector<UsbId> found;
    std::optional<shared_kernel::DomainError> firstFailure;
    for (DeviceDiscovery* const discovery : discoveries_) {
        auto result = discovery->listDevices();
        if (result.has_value()) {
            found.insert(found.end(), result->begin(), result->end());
        } else if (!firstFailure.has_value()) {
            firstFailure = result.error();
        }
    }

    std::vector<DeviceDetection> detections;
    detections.reserve(kDeviceModels.size());
    for (const DeviceModel model : kDeviceModels) {
        std::vector<ConnectionPath> paths;
        for (const UsbId id : found) {
            if (const auto entry = identifyDevice(id); entry.has_value() && entry->model == model) {
                paths.push_back(entry->path);
            }
        }
        std::ranges::sort(paths);
        const auto duplicates = std::ranges::unique(paths);
        paths.erase(duplicates.begin(), duplicates.end());
        if (paths.empty() && firstFailure.has_value()) {
            detections.push_back({.model = model, .paths = std::unexpected(*firstFailure)});
        } else {
            detections.push_back({.model = model, .paths = std::move(paths)});
        }
    }
    return detections;
}

} // namespace ghub::device_inventory
