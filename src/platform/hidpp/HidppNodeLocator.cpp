#include "platform/hidpp/HidppNodeLocator.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ghub::platform::hidpp {

namespace {

constexpr std::uint16_t logitechVendor = 0x046D;
constexpr std::uint16_t receiverProduct = 0xC547;
constexpr std::uint16_t g703ReceiverProduct = 0xC539;
constexpr std::uint16_t g703Product = 0x4086;
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
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-suspicious-stringview-data-usage):
    // from_chars exige o par de ponteiros [data, data+size) do string_view, sem terminador nulo.
    const auto* end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, value, base);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-suspicious-stringview-data-usage)
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

// Dispositivo sem fio (`devicePid`) atrás de um receptor (`receiverPid`): o filho tem HID_PHYS ".../input2:N"
// (N = índice de dispositivo, 1..6) e o nó a abrir é o receptor interface 2 do mesmo hub USB.
std::optional<HidppNode> findBehindReceiver(const std::vector<HidrawEntry>& entries, std::uint16_t devicePid,
                                            std::uint16_t receiverPid, const std::filesystem::path& devRoot)
{
    constexpr std::string_view marker = "/input2:";
    for (const auto& child : entries) {
        if (child.vendor != logitechVendor || child.product != devicePid) {
            continue;
        }
        const auto at = child.phys.rfind(marker);
        if (at == std::string::npos) {
            continue;
        }
        const auto index = parseNumber<unsigned>(std::string_view{child.phys}.substr(at + marker.size()), 10);
        if (!index || *index < 1 || *index > 6) {
            continue;
        }
        const std::string receiverPhys = child.phys.substr(0, at) + "/input2";
        for (const auto& receiver : entries) {
            if (receiver.vendor == logitechVendor && receiver.product == receiverPid && receiver.phys == receiverPhys) {
                return HidppNode{
                    .devicePath = devRoot / receiver.name,
                    .deviceIndex = static_cast<std::uint8_t>(*index),
                };
            }
        }
    }
    return std::nullopt;
}

} // namespace

shared_kernel::Result<HidppNode> HidppNodeLocator::locateG915() const
{
    const auto entries = scan(sysRoot_);
    if (!entries) {
        return std::unexpected(entries.error());
    }
    if (auto node = findBehindReceiver(*entries, g915Product, receiverProduct, devRoot_)) {
        return *node;
    }
    return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                               "G915 (046d:407c) com receptor c547 interface 2 não encontrado");
}

shared_kernel::Result<HidppNode> HidppNodeLocator::locateG703() const
{
    const auto entries = scan(sysRoot_);
    if (!entries) {
        return std::unexpected(entries.error());
    }
    if (auto node = findBehindReceiver(*entries, g703Product, g703ReceiverProduct, devRoot_)) {
        return *node;
    }
    return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                               "G703 (046d:4086) com receptor c539 interface 2 não encontrado");
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

} // namespace ghub::platform::hidpp
