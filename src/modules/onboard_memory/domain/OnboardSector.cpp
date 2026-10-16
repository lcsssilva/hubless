#include "modules/onboard_memory/domain/OnboardSector.hpp"

#include <string>

namespace ghub::onboard_memory {

shared_kernel::Result<OnboardSector> OnboardSector::fromBytes(std::vector<std::uint8_t> bytes)
{
    if (bytes.size() < kMinSize || bytes.size() > kMaxSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "setor onboard deve ter entre " + std::to_string(kMinSize) + " e " +
                                       std::to_string(kMaxSize) + " bytes (recebido: " + std::to_string(bytes.size()) +
                                       ")");
    }
    return OnboardSector{std::move(bytes)};
}

std::uint16_t OnboardSector::crc16Ccitt(std::span<const std::uint8_t> data) noexcept
{
    constexpr unsigned kPolynomial = 0x1021U;
    unsigned crc = 0xFFFFU;
    for (const std::uint8_t byte : data) {
        crc ^= static_cast<unsigned>(byte) << 8U;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0 ? ((crc << 1U) ^ kPolynomial) & 0xFFFFU : (crc << 1U) & 0xFFFFU;
        }
    }
    return static_cast<std::uint16_t>(crc);
}

std::uint16_t OnboardSector::storedCrc() const
{
    return static_cast<std::uint16_t>((static_cast<unsigned>(bytes_.at(size() - 2)) << 8U) | bytes_.at(size() - 1));
}

std::uint16_t OnboardSector::computedCrc() const
{
    return crc16Ccitt(std::span{bytes_}.first(size() - 2));
}

OnboardSector OnboardSector::withCrc() const
{
    OnboardSector copy{*this};
    const std::uint16_t crc = computedCrc();
    copy.bytes_.at(size() - 2) = static_cast<std::uint8_t>(crc >> 8U);
    copy.bytes_.at(size() - 1) = static_cast<std::uint8_t>(crc & 0xFFU);
    return copy;
}

} // namespace ghub::onboard_memory
