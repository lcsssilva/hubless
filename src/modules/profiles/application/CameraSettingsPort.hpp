#pragma once

#include "modules/profiles/domain/CameraControlSetting.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <vector>

namespace ghub::profiles {

// Acesso à câmera para o módulo profiles. Bloqueante: só chamar de uma thread de trabalho (nunca da
// thread da UI). O adaptador liga isto à API pública do módulo camera (ADR-0020).
class CameraSettingsPort {
public:
    virtual ~CameraSettingsPort() = default;

    // Controles atuais já filtrados pelo dono: só os graváveis e ativos que têm valor.
    // Câmera ausente ou indisponível -> BackendUnavailable.
    [[nodiscard]] virtual shared_kernel::Result<std::vector<CameraControlSetting>> readCurrent() = 0;

    // Grava um controle (lease, validação e releitura ficam com o dono) e devolve o valor relido.
    [[nodiscard]] virtual shared_kernel::Result<std::int64_t> apply(std::uint32_t id, std::int64_t value) = 0;
};

} // namespace ghub::profiles
