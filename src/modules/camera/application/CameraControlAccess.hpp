#pragma once

#include "modules/camera/domain/CameraControl.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <memory>

namespace ghub::camera {

// Sessão de acesso aos controles da câmera. Destruir a sessão fecha o nó (RAII).
// Uma sessão não é segura para uso concorrente; quem a abre a usa de uma thread só.
class CameraControlSession {
public:
    virtual ~CameraControlSession() = default;

    // Relê o controle direto no dispositivo (nunca de cache). Erro InvalidArgument = controle inexistente.
    [[nodiscard]] virtual shared_kernel::Result<CameraControl> readControl(std::uint32_t id) = 0;

    // Grava o valor exatamente como pedido (a validação é de quem chama). Erros: DeviceBusy (câmera ocupada),
    // InvalidArgument (o dispositivo recusou o valor ou o controle), BackendUnavailable (câmera removida, falha).
    [[nodiscard]] virtual shared_kernel::Result<void> writeControl(std::uint32_t id, std::int64_t value) = 0;

protected:
    CameraControlSession() = default;
    CameraControlSession(const CameraControlSession&) = default;
    CameraControlSession& operator=(const CameraControlSession&) = default;
    CameraControlSession(CameraControlSession&&) = default;
    CameraControlSession& operator=(CameraControlSession&&) = default;
};

// Port: abre uma sessão de controle da câmera. Abrir não grava nada.
// Erro BackendUnavailable com "câmera não detectada" = a câmera não está presente.
class CameraControlAccess {
public:
    virtual ~CameraControlAccess() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::unique_ptr<CameraControlSession>> open() = 0;

protected:
    CameraControlAccess() = default;
    CameraControlAccess(const CameraControlAccess&) = default;
    CameraControlAccess& operator=(const CameraControlAccess&) = default;
    CameraControlAccess(CameraControlAccess&&) = default;
    CameraControlAccess& operator=(CameraControlAccess&&) = default;
};

} // namespace ghub::camera
