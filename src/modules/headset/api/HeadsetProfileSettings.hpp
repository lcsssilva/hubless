#pragma once

#include "modules/headset/application/ChangeHeadsetLighting.hpp"
#include "modules/headset/application/ChangeHeadsetSetting.hpp"
#include "shared_kernel/Result.hpp"

#include <mutex>
#include <optional>
#include <vector>

namespace ghub::headset {

// Cor/efeito de uma zona do G733 como o módulo `profiles` o guarda (valores simples; a validação é daqui).
// zone: 0 inferior, 1 superior; effect: 0 desligado, 1 fixo, 2 respiração, 3 ciclo.
struct LightZoneValue {
    int zone;
    int effect;
    int red;
    int green;
    int blue;
    int periodMs;
    int brightness;

    friend bool operator==(const LightZoneValue&, const LightZoneValue&) = default;
};

// API pública do módulo `headset` para as seções de perfil (INC-017 parte 2, R10; ADR-0028). Fachada fina sobre
// ChangeHeadsetSetting e ChangeHeadsetLighting: toda escrita passa pelos casos de uso existentes (sonda de
// processos externos, DeviceLease, uma tentativa). Bloqueante: chamar de uma thread de trabalho. Os casos de uso
// precisam sobreviver à fachada.
//
// O G733 não informa o sidetone nem as luzes atuais; por isso a fachada guarda o "último valor aplicado" nesta
// sessão (por ela ou registrado por quem aplicou, `remember*`). É o que "Salvar estado atual" pode capturar.
// Seguro entre threads.
class HeadsetProfileSettings {
public:
    HeadsetProfileSettings(const ChangeHeadsetSetting& changeSetting, const ChangeHeadsetLighting& changeLighting)
        noexcept
        : changeSetting_(&changeSetting), changeLighting_(&changeLighting)
    {
    }

    HeadsetProfileSettings(const HeadsetProfileSettings&) = delete;
    HeadsetProfileSettings& operator=(const HeadsetProfileSettings&) = delete;
    HeadsetProfileSettings(HeadsetProfileSettings&&) = delete;
    HeadsetProfileSettings& operator=(HeadsetProfileSettings&&) = delete;
    ~HeadsetProfileSettings() = default;

    // Fora de 0-100 -> InvalidArgument sem escrever. Sucesso: devolve o nível aplicado e o guarda.
    [[nodiscard]] shared_kernel::Result<int> applySidetone(int level) const;

    // Zona, efeito, cor 0-255, período e brilho fora das faixas -> InvalidArgument sem escrever.
    // Sucesso: devolve o valor aplicado e o guarda como último da zona.
    [[nodiscard]] shared_kernel::Result<LightZoneValue> applyLight(const LightZoneValue& value) const;

    [[nodiscard]] std::optional<int> lastSidetone() const;
    // Últimos valores por zona, em ordem de zona.
    [[nodiscard]] std::vector<LightZoneValue> lastLights() const;

    // Registra o que a janela aplicou por fora da fachada; não toca o dispositivo.
    void rememberSidetone(int level) const;
    void rememberLight(const LightZoneValue& value) const;

private:
    const ChangeHeadsetSetting* changeSetting_;   // não nulo
    const ChangeHeadsetLighting* changeLighting_; // não nulo
    mutable std::mutex mutex_;
    mutable std::optional<int> lastSidetone_;
    mutable std::vector<LightZoneValue> lastLights_;
};

} // namespace ghub::headset
