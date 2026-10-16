#pragma once

#include "shared_kernel/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace ghub::onboard_memory {

// Setor da memória onboard do mouse (HID++ 0x8100) como bytes brutos; os 2 últimos bytes são o CRC-16/CCITT
// (big-endian) dos anteriores. Objeto de valor imutável; só existe com tamanho plausível.
class OnboardSector {
public:
    static constexpr std::size_t kMinSize = 16;
    static constexpr std::size_t kMaxSize = 4096;

    [[nodiscard]] static shared_kernel::Result<OnboardSector> fromBytes(std::vector<std::uint8_t> bytes);

    // CRC-16/CCITT-FALSE: semente 0xFFFF, polinômio 0x1021, sem reflexão, sem xor final (libratbag hidpp-generic.c).
    [[nodiscard]] static std::uint16_t crc16Ccitt(std::span<const std::uint8_t> data) noexcept;

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

    [[nodiscard]] std::uint16_t storedCrc() const;
    [[nodiscard]] std::uint16_t computedCrc() const;
    [[nodiscard]] bool hasValidCrc() const { return storedCrc() == computedCrc(); }

    // Cópia com os 2 últimos bytes substituídos pelo CRC calculado.
    [[nodiscard]] OnboardSector withCrc() const;

    friend bool operator==(const OnboardSector&, const OnboardSector&) = default;

private:
    explicit OnboardSector(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}

    std::vector<std::uint8_t> bytes_;
};

} // namespace ghub::onboard_memory
