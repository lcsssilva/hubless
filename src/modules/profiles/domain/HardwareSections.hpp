#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ghub::profiles {

// Seções de hardware do perfil (INC-017 parte 2, R10). Valores simples: a faixa de cada campo é validada pelo
// módulo dono ao aplicar (como a curva do EQ, ADR-0020), para a regra não ficar duplicada. O perfil só garante a
// forma (tamanhos, zonas distintas, índices existentes).

// Uma zona de luz do G733 (headset): zona 0 inferior / 1 superior; efeito 0 desligado, 1 fixo, 2 respiração, 3 ciclo.
struct HeadsetLightSetting {
    int zone;
    int effect;
    int red;
    int green;
    int blue;
    int periodMs;
    int brightness;

    friend bool operator==(const HeadsetLightSetting&, const HeadsetLightSetting&) = default;
};

// Efeito de prévia do G915 (wave, cycle, breathing, ripple, static), com cor e brilho. Volátil.
struct KeyboardEffectSetting {
    std::string kind;
    int red;
    int green;
    int blue;
    int brightness;

    friend bool operator==(const KeyboardEffectSetting&, const KeyboardEffectSetting&) = default;
};

// DPI, estágios e taxa de relatório do G703 (perfil onboard ativo).
struct PointerDpiSetting {
    int reportRateHz;
    std::vector<int> dpiStages;
    int defaultStage;
    std::optional<int> dpiShiftStage;

    friend bool operator==(const PointerDpiSetting&, const PointerDpiSetting&) = default;
};

struct HardwareSections {
    std::optional<int> sidetone;
    std::vector<HeadsetLightSetting> headsetLights;
    std::optional<KeyboardEffectSetting> keyboardEffect;
    std::optional<PointerDpiSetting> pointerDpi;

    [[nodiscard]] bool empty() const noexcept
    {
        return !sidetone.has_value() && headsetLights.empty() && !keyboardEffect.has_value() && !pointerDpi.has_value();
    }

    friend bool operator==(const HardwareSections&, const HardwareSections&) = default;
};

} // namespace ghub::profiles
