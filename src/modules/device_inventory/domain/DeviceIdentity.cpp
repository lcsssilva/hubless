#include "modules/device_inventory/domain/DeviceIdentity.hpp"

#include <cstddef>
#include <utility>

namespace ghub::device_inventory {

namespace {

template <typename T> bool cutAbove(std::vector<T>& values, std::size_t limit)
{
    if (values.size() <= limit) {
        return false;
    }
    values.erase(values.begin() + static_cast<std::ptrdiff_t>(limit), values.end());
    return true;
}

} // namespace

DeviceIdentity::DeviceIdentity(DeviceIdentityParts parts) : parts_(std::move(parts))
{
    if (cutAbove(parts_.firmwares, maxFirmwareEntries)) {
        parts_.firmwareTruncated = true;
    }
    if (cutAbove(parts_.featureIds, maxFeatures)) {
        parts_.featuresTruncated = true;
    }
}

std::optional<std::string> deviceNameFromBytes(std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() > DeviceIdentity::maxNameLength) {
        return std::nullopt;
    }
    std::string name;
    name.reserve(bytes.size());
    for (const auto byte : bytes) {
        name.push_back(byte >= 0x20 && byte <= 0x7E ? static_cast<char>(byte) : '?');
    }
    return name;
}

} // namespace ghub::device_inventory
