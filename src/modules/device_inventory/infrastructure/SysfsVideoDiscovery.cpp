#include "modules/device_inventory/infrastructure/SysfsVideoDiscovery.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <string_view>

namespace ghub::device_inventory {

namespace {

constexpr std::size_t maxModaliasBytes = 256;

// Lê no máximo maxModaliasBytes; arquivo ausente ou ilegível → texto vazio.
std::string readModalias(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    std::array<char, maxModaliasBytes> buffer{};
    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return {buffer.data(), static_cast<std::size_t>(stream.gcount())};
}

} // namespace

shared_kernel::Result<std::vector<UsbId>> SysfsVideoDiscovery::listDevices()
{
    namespace fs = std::filesystem;

    // increment(error) em vez de range-for: operator++ lança, e a chamada roda em slot de QTimer.
    std::error_code error;
    std::vector<UsbId> ids;
    for (fs::directory_iterator it(root_, error); !error && it != fs::directory_iterator{}; it.increment(error)) {
        const std::string modalias = readModalias(it->path() / "device" / "modalias");
        if (const auto id = parseUsbModalias(modalias); id.has_value()) {
            ids.push_back(*id);
        }
    }
    if (error) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   root_.string() + ": " + error.message());
    }
    std::ranges::sort(ids);
    const auto duplicates = std::ranges::unique(ids);
    ids.erase(duplicates.begin(), duplicates.end());
    return ids;
}

} // namespace ghub::device_inventory
