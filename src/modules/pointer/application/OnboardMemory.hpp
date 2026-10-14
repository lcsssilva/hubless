#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace ghub::pointer {

struct OnboardLayout {
    std::uint16_t sectorSize;
    std::uint8_t profileCount;
};

// Port: memória onboard do mouse (HID++ 0x8100) já aberta. Existe só durante uma operação; o caso de uso
// garante o DeviceLease enquanto o objeto vive.
class OnboardMemory {
public:
    virtual ~OnboardMemory() = default;

    [[nodiscard]] virtual shared_kernel::Result<OnboardLayout> describe() = 0;
    // Modo de operação do mouse (1 = onboard).
    [[nodiscard]] virtual shared_kernel::Result<std::uint8_t> onboardMode() = 0;
    // Setor do perfil ativo, contado a partir de 1.
    [[nodiscard]] virtual shared_kernel::Result<std::uint16_t> currentProfileSector() = 0;
    [[nodiscard]] virtual shared_kernel::Result<std::vector<std::uint8_t>> readSector(std::uint16_t sector,
                                                                                      std::uint16_t size) = 0;
    [[nodiscard]] virtual shared_kernel::Result<void> writeSector(std::uint16_t sector,
                                                                  std::span<const std::uint8_t> bytes) = 0;

protected:
    OnboardMemory() = default;
    OnboardMemory(const OnboardMemory&) = default;
    OnboardMemory& operator=(const OnboardMemory&) = default;
    OnboardMemory(OnboardMemory&&) = default;
    OnboardMemory& operator=(OnboardMemory&&) = default;
};

// Port: abre o nó do dispositivo só durante a operação (fechado ao destruir o OnboardMemory).
class OnboardMemoryConnector {
public:
    virtual ~OnboardMemoryConnector() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::unique_ptr<OnboardMemory>> open() = 0;

protected:
    OnboardMemoryConnector() = default;
    OnboardMemoryConnector(const OnboardMemoryConnector&) = default;
    OnboardMemoryConnector& operator=(const OnboardMemoryConnector&) = default;
    OnboardMemoryConnector(OnboardMemoryConnector&&) = default;
    OnboardMemoryConnector& operator=(OnboardMemoryConnector&&) = default;
};

} // namespace ghub::pointer
