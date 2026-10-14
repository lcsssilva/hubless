#include "modules/pointer/domain/OnboardDirectory.hpp"

#include <algorithm>

namespace ghub::pointer {

namespace {
constexpr std::size_t kEntrySize = 4;
constexpr std::uint16_t kEndMarker = 0xFFFF;
} // namespace

shared_kernel::Result<OnboardDirectory> OnboardDirectory::parse(const OnboardSector& sector)
{
    if (!sector.hasValidCrc()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "CRC do diretório onboard inválido");
    }
    const auto& bytes = sector.bytes();
    const std::size_t payload = bytes.size() - 2;
    std::vector<Entry> entries;
    for (std::size_t at = 0; at + kEntrySize <= payload; at += kEntrySize) {
        const auto address = static_cast<std::uint16_t>((static_cast<unsigned>(bytes.at(at)) << 8U) | bytes.at(at + 1));
        if (address == kEndMarker) {
            break;
        }
        entries.push_back({.sector = address, .enabled = bytes.at(at + 2) != 0});
    }
    return OnboardDirectory{std::move(entries)};
}

bool OnboardDirectory::isEnabled(std::uint16_t sector) const noexcept
{
    return std::ranges::any_of(entries_, [sector](const Entry& e) { return e.sector == sector && e.enabled; });
}

} // namespace ghub::pointer
