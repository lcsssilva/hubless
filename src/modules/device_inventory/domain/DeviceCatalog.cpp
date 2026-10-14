#include "modules/device_inventory/domain/DeviceCatalog.hpp"

#include <charconv>
#include <cstddef>
#include <memory>

namespace ghub::device_inventory {

namespace {

constexpr std::uint16_t logitechVendorId = 0x046d;

std::optional<std::uint16_t> parseHex4(std::string_view text) noexcept
{
    constexpr std::size_t fieldWidth = 4;
    if (text.size() != fieldWidth) {
        return std::nullopt;
    }
    // from_chars aceitaria sinal; exigimos somente dígitos hexadecimais.
    for (const char c : text) {
        const bool digit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!digit) {
            return std::nullopt;
        }
    }
    std::uint16_t value{};
    const auto* const first = std::to_address(text.begin());
    const auto* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(first, last, value, 16);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<CatalogEntry> identifyDevice(UsbId id) noexcept
{
    if (id.vendorId != logitechVendorId) {
        return std::nullopt;
    }
    switch (id.productId) {
    case 0x407c:
        return CatalogEntry{.model = DeviceModel::G915, .path = ConnectionPath::LightspeedReceiver};
    case 0xc33e:
        return CatalogEntry{.model = DeviceModel::G915, .path = ConnectionPath::UsbCable};
    case 0x4086:
        return CatalogEntry{.model = DeviceModel::G703, .path = ConnectionPath::LightspeedReceiver};
    case 0xc090:
        return CatalogEntry{.model = DeviceModel::G703, .path = ConnectionPath::UsbCable};
    case 0x0afe:
        return CatalogEntry{.model = DeviceModel::G733, .path = ConnectionPath::UsbDongle};
    case 0x094c:
        return CatalogEntry{.model = DeviceModel::Brio100, .path = ConnectionPath::UsbCable};
    default:
        return std::nullopt;
    }
}

std::string_view modelName(DeviceModel model) noexcept
{
    switch (model) {
    case DeviceModel::G915:
        return "G915";
    case DeviceModel::G703:
        return "G703";
    case DeviceModel::G733:
        return "G733";
    case DeviceModel::Brio100:
        return "Brio 100";
    }
    return {};
}

std::optional<UsbId> parseHidDeviceName(std::string_view name) noexcept
{
    // "BBBB:VVVV:PPPP.IIII"
    constexpr std::size_t length = 19;
    if (name.size() != length || name.at(4) != ':' || name.at(9) != ':' || name.at(14) != '.') {
        return std::nullopt;
    }
    const auto bus = parseHex4(name.substr(0, 4));
    const auto vendor = parseHex4(name.substr(5, 4));
    const auto product = parseHex4(name.substr(10, 4));
    const auto instance = parseHex4(name.substr(15, 4));
    if (!bus || !vendor || !product || !instance) {
        return std::nullopt;
    }
    return UsbId{.vendorId = *vendor, .productId = *product};
}

std::optional<UsbId> parseUsbModalias(std::string_view modalias) noexcept
{
    // "usb:vVVVVpPPPP..."
    constexpr std::string_view prefix = "usb:v";
    constexpr std::size_t productMarker = prefix.size() + 4;
    constexpr std::size_t minLength = productMarker + 1 + 4;
    if (modalias.size() < minLength || !modalias.starts_with(prefix) || modalias.at(productMarker) != 'p') {
        return std::nullopt;
    }
    const auto vendor = parseHex4(modalias.substr(prefix.size(), 4));
    const auto product = parseHex4(modalias.substr(productMarker + 1, 4));
    if (!vendor || !product) {
        return std::nullopt;
    }
    return UsbId{.vendorId = *vendor, .productId = *product};
}

} // namespace ghub::device_inventory
