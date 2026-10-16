#include "modules/device_inventory/domain/DeviceNickname.hpp"

#include <cstdint>

namespace ghub::device_inventory {

namespace {

bool isAsciiSpace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

int continuationBytes(std::uint8_t lead) noexcept
{
    if (lead < 0x80) {
        return 0;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        return 1;
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        return 2;
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        return 3;
    }
    return -1;
}

// Valida UTF-8 e rejeita controles C0, DEL e C1. Devolve a quantidade de pontos de código, ou -1 se inválido.
long countPrintableCodePoints(std::string_view text) noexcept
{
    long count = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<std::uint8_t>(text[i]);
        const int extra = continuationBytes(lead);
        if (extra < 0 || i + static_cast<std::size_t>(extra) >= text.size()) {
            return -1;
        }
        std::uint32_t code = extra == 0 ? lead : (lead & (0x3Fu >> extra));
        for (int k = 1; k <= extra; ++k) {
            const auto next = static_cast<std::uint8_t>(text[i + static_cast<std::size_t>(k)]);
            if ((next & 0xC0) != 0x80) {
                return -1;
            }
            code = (code << 6) | (next & 0x3Fu);
        }
        constexpr std::uint32_t minimumForLength[] = {0, 0x80, 0x800, 0x10000};
        if (code < minimumForLength[extra] || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
            return -1;
        }
        if (code < 0x20 || (code >= 0x7F && code <= 0x9F)) {
            return -1;
        }
        i += static_cast<std::size_t>(extra) + 1;
        ++count;
    }
    return count;
}

} // namespace

shared_kernel::Result<DeviceNickname> DeviceNickname::create(std::string_view raw)
{
    while (!raw.empty() && isAsciiSpace(raw.front())) {
        raw.remove_prefix(1);
    }
    while (!raw.empty() && isAsciiSpace(raw.back())) {
        raw.remove_suffix(1);
    }
    if (raw.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "apelido vazio");
    }
    const long length = countPrintableCodePoints(raw);
    if (length < 0) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "apelido com caractere de controle ou UTF-8 inválido");
    }
    if (static_cast<std::size_t>(length) > maximumLength) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "apelido com mais de 32 caracteres");
    }
    return DeviceNickname{std::string(raw)};
}

} // namespace ghub::device_inventory
