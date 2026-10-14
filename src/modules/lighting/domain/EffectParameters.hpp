#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>

namespace ghub::lighting::domain {

// Período de um efeito de hardware do teclado, em milissegundos. Objeto de valor imutável.
// Faixa 1000..20000 ms, derivada do OpenRGB release_1.0:
//  - LogitechG915Controller.h:70-72 (velocidade SLOWEST=0xC8=200, FASTEST=0x0A=10);
//  - LogitechG915Controller.cpp:396-399 (exceto Ripple, o valor enviado é 100 * velocidade).
// Origem OpenRGB, não confirmada em hardware.
class EffectPeriod {
public:
    static constexpr std::uint32_t kMinMilliseconds = 1000;
    static constexpr std::uint32_t kMaxMilliseconds = 20000;

    [[nodiscard]] static shared_kernel::Result<EffectPeriod> create(std::uint32_t milliseconds);

    [[nodiscard]] std::uint32_t milliseconds() const noexcept { return milliseconds_; }

    friend bool operator==(const EffectPeriod&, const EffectPeriod&) = default;

private:
    explicit EffectPeriod(std::uint32_t milliseconds) noexcept : milliseconds_(milliseconds) {}

    std::uint32_t milliseconds_;
};

// Brilho de um efeito, em porcento. Faixa 1..100, derivada do OpenRGB release_1.0:
// RGBController_LogitechG915.cpp:329-330 (Breathing), :345-346 (Cycle), :355-356 (Wave).
class EffectBrightness {
public:
    static constexpr std::uint32_t kMinPercent = 1;
    static constexpr std::uint32_t kMaxPercent = 100;

    [[nodiscard]] static shared_kernel::Result<EffectBrightness> create(std::uint32_t percent);

    [[nodiscard]] std::uint32_t percent() const noexcept { return percent_; }

    friend bool operator==(const EffectBrightness&, const EffectBrightness&) = default;

private:
    explicit EffectBrightness(std::uint32_t percent) noexcept : percent_(percent) {}

    std::uint32_t percent_;
};

// Velocidade do efeito Ripple. Faixa 2..200 (LogitechG915Controller.h:78-80: RIPPLE_FAST=2,
// RIPPLE_NORMAL=20, RIPPLE_SLOW=200), enviada sem multiplicar (LogitechG915Controller.cpp:427-430,
// byte 0x0B). Unidade NÃO confirmada: o comentário do OpenRGB diz "ms", o que não foi verificado.
class RippleSpeed {
public:
    static constexpr std::uint32_t kMin = 2;
    static constexpr std::uint32_t kMax = 200;

    [[nodiscard]] static shared_kernel::Result<RippleSpeed> create(std::uint32_t value);

    [[nodiscard]] std::uint32_t value() const noexcept { return value_; }

    friend bool operator==(const RippleSpeed&, const RippleSpeed&) = default;

private:
    explicit RippleSpeed(std::uint32_t value) noexcept : value_(value) {}

    std::uint32_t value_;
};

} // namespace ghub::lighting::domain
