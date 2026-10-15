#include "modules/device_inventory/domain/FirmwareVersion.hpp"

#include <cstddef>

namespace ghub::device_inventory {

namespace {

constexpr char hexDigit(unsigned nibble)
{
    return "0123456789ABCDEF"[nibble & 0x0FU]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

std::string hexByte(std::uint8_t value)
{
    return {hexDigit(value >> 4U), hexDigit(value)};
}

std::string hexWord(std::uint16_t value)
{
    return {hexDigit(value >> 12U), hexDigit(value >> 8U), hexDigit(value >> 4U), hexDigit(value)};
}

std::string prefixText(const std::array<std::uint8_t, 3>& prefix)
{
    std::string text;
    for (const auto byte : prefix) {
        text.push_back(byte >= 0x20 && byte <= 0x7E ? static_cast<char>(byte) : '?');
    }
    // Preenchimento do firmware: espaços ou NUL à direita não fazem parte do prefixo.
    std::size_t end = prefix.size();
    while (end > 0 && (prefix.at(end - 1) == ' ' || prefix.at(end - 1) == 0x00)) {
        --end;
    }
    text.resize(end);
    return text;
}

} // namespace

FirmwareVersion::FirmwareVersion(std::uint8_t kind, std::array<std::uint8_t, 3> prefix, std::uint8_t number,
                                 std::uint8_t revision, std::uint16_t build, bool active)
    : kind_(kind), active_(active)
{
    const std::string prefixPart = prefixText(prefix);
    versionText_ = (prefixPart.empty() ? "" : prefixPart + " ") + hexByte(number) + "." + hexByte(revision) + " b" +
                   hexWord(build);
}

std::string FirmwareVersion::kindText() const
{
    switch (kind_) {
    case 0:
        return "aplicação";
    case 1:
        return "bootloader";
    case 2:
        return "hardware";
    default:
        return "outro (" + std::to_string(kind_) + ")";
    }
}

std::string FirmwareVersion::describe() const
{
    return versionText_ + " — " + kindText() + (active_ ? " (ativo)" : "");
}

} // namespace ghub::device_inventory
