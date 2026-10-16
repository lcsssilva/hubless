#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ghub::lighting::domain {

// Slot de perfil onboard do G915 (M1, M2, M3 = 1..3; o G915 tem 3 perfis, `profile_count` do GetProfilesDescr na
// evidência de 2026-10-14). O número é o setor do perfil na memória onboard. Objeto de valor imutável.
class OnboardSlot {
public:
    static constexpr std::uint8_t kMin = 1;
    static constexpr std::uint8_t kMax = 3;

    [[nodiscard]] static shared_kernel::Result<OnboardSlot> create(std::uint8_t number);

    [[nodiscard]] std::uint8_t number() const noexcept { return number_; }

    friend bool operator==(const OnboardSlot&, const OnboardSlot&) = default;

private:
    explicit OnboardSlot(std::uint8_t number) noexcept : number_(number) {}

    std::uint8_t number_;
};

// Slots habilitados no diretório da memória onboard e o slot ativo (quando informado pelo teclado).
struct OnboardSlots {
    std::vector<OnboardSlot> enabled;
    std::optional<OnboardSlot> active;
};

} // namespace ghub::lighting::domain
