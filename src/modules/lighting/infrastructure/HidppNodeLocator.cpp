#include "modules/lighting/infrastructure/HidppNodeLocator.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ghub::lighting::infrastructure {

namespace {

constexpr std::uint16_t logitechVendor = 0x046D;
constexpr std::uint16_t receiverProduct = 0xC547;
constexpr std::uint16_t g915Product = 0x407C;
constexpr std::uint16_t g733Product = 0x0AFE;
constexpr std::uint8_t headsetDeviceIndex = 0xFF;

struct HidrawEntry {
    std::string name; // hidrawN
    std::uint16_t vendor;
    std::uint16_t product;
    std::string phys;
};

template <typename T> std::optional<T> parseNumber(std::string_view text, int base)
{
    T value{};
    const auto* end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, value, base);
    if (error != std::errc{} || ptr != end || text.empty()) {
        return std::nullopt;
    }
    return value;
}

// HID_ID=BBBB:VVVVVVVV:PPPPPPPP
bool parseHidId(std::string_view text, std::uint16_t& vendor, std::uint16_t& product)
{
    const auto first = text.find(':');
    const auto second = first == std::string_view::npos ? first : text.find(':', first + 1);
    if (second == std::string_view::npos) {
        return false;
    }
    const auto vendorValue = parseNumber<std::uint32_t>(text.substr(first + 1, second - first - 1), 16);
    const auto productValue = parseNumber<std::uint32_t>(text.substr(second + 1), 16);
    if (!vendorValue || !productValue || *vendorValue > 0xFFFF || *productValue > 0xFFFF) {
        return false;
    }
    vendor = static_cast<std::uint16_t>(*vendorValue);
    product = static_cast<std::uint16_t>(*productValue);
    return true;
}

std::optional<HidrawEntry> readEntry(const std::filesystem::path& hidrawDir)
{
    std::ifstream uevent{hidrawDir / "device" / "uevent"};
    if (!uevent) {
        return std::nullopt;
    }
    HidrawEntry entry{.name = hidrawDir.filename().string(), .vendor = 0, .product = 0, .phys = {}};
    bool hasId = false;
    std::string line;
    while (std::getline(uevent, line)) {
        if (line.starts_with("HID_ID=")) {
            hasId = parseHidId(std::string_view{line}.substr(7), entry.vendor, entry.product);
        } else if (line.starts_with("HID_PHYS=")) {
            entry.phys = line.substr(9);
        }
    }
    if (!hasId || entry.phys.empty()) {
        return std::nullopt;
    }
    return entry;
}

bool endsWith(const std::string& text, std::string_view suffix)
{
    return text.ends_with(suffix);
}

} // namespace

namespace {

shared_kernel::Result<std::vector<HidrawEntry>> scan(const std::filesystem::path& sysRoot)
{
    const auto classDir = sysRoot / "class" / "hidraw";
    std::error_code error;
    std::vector<HidrawEntry> entries;
    // increment(error): operator++ lança e esta leitura roda fora de qualquer tratamento de exceção.
    for (std::filesystem::directory_iterator it(classDir, error); !error && it != std::filesystem::directory_iterator{};
         it.increment(error)) {
        if (auto entry = readEntry(it->path()); entry.has_value()) {
            entries.push_back(std::move(*entry));
        }
    }
    if (error) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   classDir.string() + ": " + error.message());
    }
    std::ranges::sort(entries, {}, &HidrawEntry::name);
    return entries;
}

} // namespace

shared_kernel::Result<HidppNode> HidppNodeLocator::locateG915() const
{
    const auto entries = scan(sysRoot_);
    if (!entries) {
        return std::unexpected(entries.error());
    }
    constexpr std::string_view marker = "/input2:";
    for (const auto& keyboard : *entries) {
        if (keyboard.vendor != logitechVendor || keyboard.product != g915Product) {
            continue;
        }
        const auto at = keyboard.phys.rfind(marker);
        if (at == std::string::npos) {
            continue;
        }
        const auto index = parseNumber<unsigned>(std::string_view{keyboard.phys}.substr(at + marker.size()), 10);
        if (!index || *index < 1 || *index > 6) {
            continue;
        }
        const std::string receiverPhys = keyboard.phys.substr(0, at) + "/input2";
        for (const auto& receiver : *entries) {
            if (receiver.vendor == logitechVendor && receiver.product == receiverProduct &&
                receiver.phys == receiverPhys) {
                return HidppNode{.devicePath = devRoot_ / receiver.name,
                                 .deviceIndex = static_cast<std::uint8_t>(*index)};
            }
        }
    }
    return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                               "G915 (046d:407c) com receptor c547 interface 2 não encontrado");
}

shared_kernel::Result<HidppNode> HidppNodeLocator::locateG733() const
{
    const auto entries = scan(sysRoot_);
    if (!entries) {
        return std::unexpected(entries.error());
    }
    for (const auto& entry : *entries) {
        if (entry.vendor == logitechVendor && entry.product == g733Product && endsWith(entry.phys, "/input3")) {
            return HidppNode{.devicePath = devRoot_ / entry.name, .deviceIndex = headsetDeviceIndex};
        }
    }
    return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                               "G733 (046d:0afe) interface 3 não encontrado");
}

} // namespace ghub::lighting::infrastructure
