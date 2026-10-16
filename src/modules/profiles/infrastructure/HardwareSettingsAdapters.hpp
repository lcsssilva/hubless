#pragma once

#include "modules/headset/api/HeadsetProfileSettings.hpp"
#include "modules/lighting/api/KeyboardEffectSettings.hpp"
#include "modules/pointer/api/G703DpiSettings.hpp"
#include "modules/profiles/application/HardwareSettingsPorts.hpp"

namespace ghub::profiles {

// Adaptadores das seções de hardware do perfil (INC-017 parte 2, R10; ADR-0028): traduzem os tipos do perfil e
// chamam somente a `api/` do módulo dono. As fachadas precisam sobreviver aos adaptadores.

// Sidetone e luzes do G733 -> headset::HeadsetProfileSettings. Aplicar bloqueia (thread de trabalho).
class HeadsetSettingsAdapter final : public HeadsetSettingsPort {
public:
    explicit HeadsetSettingsAdapter(headset::HeadsetProfileSettings& settings) noexcept : settings_(&settings) {}

    [[nodiscard]] std::optional<int> lastSidetone() const override;
    [[nodiscard]] std::vector<HeadsetLightSetting> lastLights() const override;
    [[nodiscard]] shared_kernel::Result<int> applySidetone(int level) override;
    [[nodiscard]] shared_kernel::Result<void> applyLight(const HeadsetLightSetting& light) override;

private:
    headset::HeadsetProfileSettings* settings_; // não nulo
};

// Efeito do G915 -> lighting::KeyboardEffectSettings. Só na thread dona da sessão ao vivo.
class KeyboardEffectAdapter final : public KeyboardEffectPort {
public:
    explicit KeyboardEffectAdapter(lighting::KeyboardEffectSettings& settings) noexcept : settings_(&settings) {}

    [[nodiscard]] std::optional<KeyboardEffectSetting> last() const override;
    [[nodiscard]] shared_kernel::Result<void> apply(const KeyboardEffectSetting& effect) override;

private:
    lighting::KeyboardEffectSettings* settings_; // não nulo
};

// DPI/estágios/taxa do G703 -> pointer::G703DpiSettings. Bloqueante (thread de trabalho).
class PointerSettingsAdapter final : public PointerSettingsPort {
public:
    explicit PointerSettingsAdapter(const pointer::G703DpiSettings& settings) noexcept : settings_(&settings) {}

    [[nodiscard]] shared_kernel::Result<PointerDpiSetting> readCurrent() override;
    [[nodiscard]] shared_kernel::Result<PointerDpiApplied> apply(const PointerDpiSetting& setting) override;

private:
    const pointer::G703DpiSettings* settings_; // não nulo
};

} // namespace ghub::profiles
