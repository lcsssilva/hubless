#pragma once

#include "modules/onboard_memory/domain/OnboardSector.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <vector>

namespace ghub::onboard_memory {

// Diretório de perfis onboard (setor 0x0000 da feature 0x8100): entradas de 4 bytes
// [setor BE16][habilitado][reservado], terminando em setor 0xFFFF (ou no fim, antes do CRC).
// Objeto de valor imutável; só existe a partir de um setor com CRC válido.
class OnboardDirectory {
public:
    struct Entry {
        std::uint16_t sector;
        bool enabled;

        friend bool operator==(const Entry&, const Entry&) = default;
    };

    [[nodiscard]] static shared_kernel::Result<OnboardDirectory> parse(const OnboardSector& sector);

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }

    // Setor ausente do diretório não está habilitado.
    [[nodiscard]] bool isEnabled(std::uint16_t sector) const noexcept;

private:
    explicit OnboardDirectory(std::vector<Entry> entries) : entries_(std::move(entries)) {}

    std::vector<Entry> entries_;
};

} // namespace ghub::onboard_memory
