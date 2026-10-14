#pragma once

#include "modules/profiles/application/CameraSettingsPort.hpp"
#include "modules/profiles/application/EqualizerSettingsPort.hpp"
#include "modules/profiles/domain/Profile.hpp"
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

    ApplyProfile(CameraSettingsPort& camera, EqualizerSettingsPort& equalizer) noexcept;

    // THREAD DE TRABALHO (bloqueante). Grava os controles em ordem crescente de id (o controle automático
    // vem antes do que depende dele). Uma falha não interrompe os demais e não há retentativa. Perfil sem
    // seção de câmera -> lista vazia.
    [[nodiscard]] std::vector<CameraItemResult> applyCamera(const Profile& profile) const;

    // THREAD DONA DO EQ. nullopt se o perfil não tem seção de EQ; senão o resultado de requestGains (ganho
    // fora da faixa -> erro do dono, nada aplicado). Não liga nem desliga o EQ.
    [[nodiscard]] std::optional<shared_kernel::Result<void>> applyEqualizer(const Profile& profile) const;

private:
    CameraSettingsPort& camera_;
    EqualizerSettingsPort& equalizer_;
};

} // namespace ghub::profiles
