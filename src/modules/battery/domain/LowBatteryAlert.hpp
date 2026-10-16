#pragma once

#include "modules/battery/domain/BatteryLevel.hpp"
#include "modules/battery/domain/ChargeState.hpp"
#include "shared_kernel/Result.hpp"

#include <optional>

namespace ghub::battery {

// Limiar do aviso de bateria fraca: 5 a 50 % (INC-017 R1). Objeto de valor imutável.
class LowBatteryThreshold {
public:
    static constexpr int minimumPercent = 5;
    static constexpr int maximumPercent = 50;
    static constexpr int defaultPercent = 15;

    // Fora de 5..50 -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<LowBatteryThreshold> create(int percent);
    [[nodiscard]] static LowBatteryThreshold standard() noexcept { return LowBatteryThreshold{defaultPercent}; }

    [[nodiscard]] int percent() const noexcept { return percent_; }

    friend bool operator==(const LowBatteryThreshold&, const LowBatteryThreshold&) = default;

private:
    explicit LowBatteryThreshold(int percent) noexcept : percent_(percent) {}

    int percent_;
};

// Regra do aviso de bateria fraca de UM dispositivo (INC-017 R1): desligada por padrão; avisa uma vez por
// descarga ao chegar ao limiar; rearma quando o dispositivo carrega (ou completa) ou quando o nível sobe a
// `rearmMargin` pontos acima do limiar. Pura: sem Qt, sem relógio, sem I/O.
class LowBatteryAlert {
public:
    static constexpr int rearmMargin = 5;

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }

    [[nodiscard]] LowBatteryThreshold threshold() const noexcept { return threshold_; }
    void setThreshold(LowBatteryThreshold threshold) noexcept { threshold_ = threshold; }

    // Registra uma leitura; true quando é hora de avisar (no máximo uma vez por descarga).
    [[nodiscard]] bool observe(ChargeState state, std::optional<BatteryLevel> level) noexcept;

private:
    bool enabled_ = false;
    LowBatteryThreshold threshold_ = LowBatteryThreshold::standard();
    bool notified_ = false;
};

} // namespace ghub::battery
