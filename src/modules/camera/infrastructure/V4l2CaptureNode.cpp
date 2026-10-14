#include "modules/camera/infrastructure/V4l2CaptureNode.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace ghub::camera {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;
using shared_kernel::Result;

constexpr std::size_t maxModaliasBytes = 256;

// Lê no máximo maxModaliasBytes; arquivo ausente ou ilegível → texto vazio.
std::string readModalias(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    std::array<char, maxModaliasBytes> buffer{};
    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return {buffer.data(), static_cast<std::size_t>(stream.gcount())};
}

// Número de "video<dígitos>"; vazio se o nome não tiver essa forma.
std::optional<unsigned long long> videoNodeNumber(std::string_view name)
{
    constexpr std::string_view prefix = "video";
    if (!name.starts_with(prefix) || name.size() == prefix.size()) {
        return std::nullopt;
    }
    const std::string_view digits = name.substr(prefix.size());
    unsigned long long number = 0;
    const auto* const first = std::to_address(digits.begin());
    const auto* const last = std::to_address(digits.end());
    const auto [end, error] = std::from_chars(first, last, number);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return number;
}

} // namespace

Result<std::vector<std::string>> findCandidateNodes(const std::filesystem::path& root, device_inventory::UsbId target)
{
    std::error_code error;
    std::vector<std::pair<unsigned long long, std::string>> found;
    // increment(error) em vez de range-for: operator++ lança, e a chamada roda em thread de trabalho.
    for (std::filesystem::directory_iterator it(root, error); !error && it != std::filesystem::directory_iterator{};
         it.increment(error)) {
        const std::string name = it->path().filename().string();
        const auto number = videoNodeNumber(name);
        if (!number.has_value()) {
            continue;
        }
        const auto id = device_inventory::parseUsbModalias(readModalias(it->path() / "device" / "modalias"));
        if (id.has_value() && *id == target) {
            found.emplace_back(*number, name);
        }
    }
    if (error) {
        return fail(ErrorCode::BackendUnavailable, root.string() + ": " + error.message());
    }
    std::ranges::sort(found);
    std::vector<std::string> names;
    names.reserve(found.size());
    for (auto& entry : found) {
        names.push_back(std::move(entry.second));
    }
    return names;
}

std::uint32_t nodeCapabilities(const v4l2_capability& capability)
{
    return (capability.capabilities & V4L2_CAP_DEVICE_CAPS) != 0 ? capability.device_caps : capability.capabilities;
}

} // namespace ghub::camera
