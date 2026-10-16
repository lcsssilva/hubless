#pragma once

#include "modules/profiles/application/CameraSettingsPort.hpp"
#include "modules/profiles/application/EqualizerSettingsPort.hpp"
#include "modules/profiles/application/HardwareSettingsPorts.hpp"
#include "modules/profiles/domain/Profile.hpp"
#include "modules/profiles/domain/ProfileSection.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ghub::profiles {

// Aplica as seções de um perfil nos módulos donos (R5, R6). As duas partes rodam em threads diferentes
// porque o EQ só aceita pedidos pela thread dona (ADR-0020). Sem estado: os métodos são const.
class ApplyProfile {
public:
    struct CameraItemResult {
        std::uint32_t id;
        std::string name;
        std::int64_t requested;
        shared_kernel::Result<std::int64_t> outcome; // valor relido, ou o erro do dono (inclui DeviceBusy)
    };

    // Resultado de uma seção de hardware (INC-017 parte 2, R11): `outcome` traz o que foi aplicado (texto curto) ou
    // o erro do dono.
    struct SectionResult {
        ProfileSection section;
        std::string label;
        shared_kernel::Result<std::string> outcome;
    };

    // `hardware` é opcional: porta nula = módulo ausente (a seção falha com BackendUnavailable).
    ApplyProfile(CameraSettingsPort& camera, EqualizerSettingsPort& equalizer, HardwarePorts hardware = {}) noexcept;

    // THREAD DE TRABALHO (bloqueante). Grava os controles em ordem crescente de id (o controle automático
    // vem antes do que depende dele). Uma falha não interrompe os demais e não há retentativa. Perfil sem
    // seção de câmera -> lista vazia.
    [[nodiscard]] std::vector<CameraItemResult> applyCamera(const Profile& profile) const;

    // THREAD DONA DO EQ. nullopt se o perfil não tem seção de EQ; senão o resultado de requestGains (ganho
    // fora da faixa -> erro do dono, nada aplicado). Não liga nem desliga o EQ.
    [[nodiscard]] std::optional<shared_kernel::Result<void>> applyEqualizer(const Profile& profile) const;

    // THREAD DE TRABALHO (bloqueante). Sidetone, luzes do G733 (por zona) e DPI do G703 do perfil, nesta ordem,
    // exceto as seções em `skip` (persistentes). Uma falha não interrompe as demais e não há retentativa.
    [[nodiscard]] std::vector<SectionResult> applyHardware(const Profile& profile, const SectionSet& skip) const;

    // THREAD DONA DA SESSÃO DO TECLADO. Efeito do G915 do perfil (lista vazia se não há ou se está em `skip`).
    [[nodiscard]] std::vector<SectionResult> applyKeyboardEffect(const Profile& profile,
                                                                 const SectionSet& skip) const;

private:
    CameraSettingsPort& camera_;
    EqualizerSettingsPort& equalizer_;
    HardwarePorts hardware_;
};

} // namespace ghub::profiles
