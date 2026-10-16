#include "modules/pointer/domain/OnboardButtons.hpp"

#include <bit>

namespace ghub::pointer {

namespace {

constexpr std::size_t kFirstButtonOffset = 0x20;
constexpr std::size_t kEntrySize = 4;
constexpr std::size_t kMaxButtons = 16;
constexpr std::uint8_t kUnusedByte = 0xFF;

std::uint32_t packBigEndian(std::span<const std::uint8_t, 4> entry) noexcept
{
    return (static_cast<std::uint32_t>(entry[0]) << 24U) | (static_cast<std::uint32_t>(entry[1]) << 16U) |
           (static_cast<std::uint32_t>(entry[2]) << 8U) | entry[3];
}

} // namespace

ButtonAction decodeOnboardButton(std::span<const std::uint8_t, 4> entry) noexcept
{
    if (entry[0] == 0x80 && entry[1] == 0x01 && entry[2] == 0x00 && std::has_single_bit(entry[3])) {
        return ButtonAction::button(static_cast<std::uint32_t>(std::countr_zero(entry[3])) + 1U);
    }
    if (entry[0] == 0x80 && entry[1] == 0x02) {
        return ButtonAction::key((static_cast<std::uint32_t>(entry[2]) << 8U) | entry[3]);
    }
    if (entry[0] == 0x90 && entry[1] == 0x00 && entry[2] == 0x00 && entry[3] == 0x00) {
        return ButtonAction::none();
    }
    return ButtonAction::unrecognized(packBigEndian(entry));
}

std::vector<ButtonAction> decodeOnboardButtons(std::span<const std::uint8_t> sector)
{
    std::vector<ButtonAction> buttons;
    for (std::size_t i = 0; i < kMaxButtons; ++i) {
        const std::size_t at = kFirstButtonOffset + (i * kEntrySize);
        if (at + kEntrySize > sector.size()) {
            break;
        }
        const auto entry = sector.subspan(at, kEntrySize).first<kEntrySize>();
        if (entry[0] == kUnusedByte && entry[1] == kUnusedByte && entry[2] == kUnusedByte && entry[3] == kUnusedByte) {
            break;
        }
        buttons.push_back(decodeOnboardButton(entry));
    }
    return buttons;
}

} // namespace ghub::pointer
