#pragma once

#include "modules/camera/application/CameraControlAccess.hpp"
#include "modules/camera/domain/CameraControl.hpp"
#include "modules/device_access/api/DeviceAccess.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>

namespace ghub::camera {

// Caso de uso: grava o valor de um controle da câmera sob DeviceLease (ADR-0010).
// Fluxo: lease -> abre -> relê o controle -> valida -> confere o lease -> grava -> relê -> devolve o relido.
// Nunca ajusta o valor, nunca repete a escrita. O lease vive só durante a operação.
// Pode ser chamado de uma thread de trabalho (faz I/O bloqueante); não usa o cache do leitor.
class SetCameraControl {
public:
    // `access` e `registry` precisam sobreviver ao caso de uso.
    SetCameraControl(CameraControlAccess& access, device_access::AccessRegistry& registry,
                     device_access::PhysicalInterfaceId interfaceId);

    // Devolve o controle como o dispositivo o informa depois da escrita.
    [[nodiscard]] shared_kernel::Result<CameraControl> setValue(std::uint32_t id, std::int64_t value) const;

    // Grava o valor padrão que o dispositivo informa na releitura, com as mesmas validações.
    [[nodiscard]] shared_kernel::Result<CameraControl> restoreDefault(std::uint32_t id) const;

private:
    enum class Target : std::uint8_t { GivenValue, DefaultValue };

    [[nodiscard]] shared_kernel::Result<CameraControl> write(std::uint32_t id, Target target, std::int64_t value) const;

    CameraControlAccess* access_;             // não nulo
    device_access::AccessRegistry* registry_; // não nulo
    device_access::PhysicalInterfaceId interfaceId_;
};

} // namespace ghub::camera
