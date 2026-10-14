#pragma once

#include "shared_kernel/Result.hpp"

namespace ghub::headset {

// Nível de sidetone (retorno da própria voz) do headset: 0 a 100, a faixa que o dispositivo realmente recebe
// (ADR-0015 decisão 2). Objeto de valor imutável; só existe válido.
class SidetoneLevel {
public:
    static constexpr int minimum = 0;
    static constexpr int maximum = 100;

    // Fora de 0-100 -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<SidetoneLevel> create(int value);

    [[nodiscard]] int value() const noexcept { return value_; }

    friend bool operator==(const SidetoneLevel&, const SidetoneLevel&) = default;

private:
    explicit SidetoneLevel(int value) noexcept : value_(value) {}

    int value_;
};

} // namespace ghub::headset
