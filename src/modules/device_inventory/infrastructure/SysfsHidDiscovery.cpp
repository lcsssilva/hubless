#include "modules/device_inventory/infrastructure/SysfsHidDiscovery.hpp"

#include <algorithm>
#include <string>

namespace ghub::device_inventory {

shared_kernel::Result<std::vector<UsbId>> SysfsHidDiscovery::listDevices()
{
    namespace fs = std::filesystem;

    // increment(error) em vez de range-for: operator++ lança, e a chamada roda em slot de QTimer.
    // Só o nome da entrada é lido (R8); as entradas são links simbólicos e não são seguidas.
    std::error_code error;
    std::vector<UsbId> ids;
    for (fs::directory_iterator it(root_, error); !error && it != fs::directory_iterator{}; it.increment(error)) {
        if (const auto id = parseHidDeviceName(it->path().filename().string()); id.has_value()) {
            ids.push_back(*id);
        }
    }
    if (error) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   root_.string() + ": " + error.message());
    }
    std::ranges::sort(ids);
    return ids;
}

} // namespace ghub::device_inventory
