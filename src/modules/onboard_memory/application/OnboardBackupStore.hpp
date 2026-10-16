#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ghub::onboard_memory {

// Port: cópia de segurança do setor onboard original (ADR-0017 §8). Nunca sobrescreve.
class OnboardBackupStore {
public:
    virtual ~OnboardBackupStore() = default;

    [[nodiscard]] virtual shared_kernel::Result<std::optional<std::vector<std::uint8_t>>>
    load(std::uint16_t sector) = 0;
    // Guarda os bytes se ainda não houver cópia do setor; se já houver, mantém a antiga e também é sucesso.
    [[nodiscard]] virtual shared_kernel::Result<void> saveIfAbsent(std::uint16_t sector,
                                                                   std::span<const std::uint8_t> bytes) = 0;

protected:
    OnboardBackupStore() = default;
    OnboardBackupStore(const OnboardBackupStore&) = default;
    OnboardBackupStore& operator=(const OnboardBackupStore&) = default;
    OnboardBackupStore(OnboardBackupStore&&) = default;
    OnboardBackupStore& operator=(OnboardBackupStore&&) = default;
};

} // namespace ghub::onboard_memory
