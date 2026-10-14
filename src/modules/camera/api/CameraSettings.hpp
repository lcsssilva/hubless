#pragma once

#include "modules/camera/application/CameraBackend.hpp"
#include "modules/camera/application/SetCameraControl.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ghub::camera {

// Valor de um controle da câmera, como o módulo `profiles` o guarda.
struct CameraControlValue {
    std::uint32_t id;
    std::string name;
    std::int64_t value;

    friend bool operator==(const CameraControlValue&, const CameraControlValue&) = default;
};

// API pública do módulo `camera` (ADR-0020). Bloqueante: chamar de uma thread de trabalho.
// O backend e o SetCameraControl precisam sobreviver a este objeto.
class CameraSettings {
public:
    CameraSettings(CameraBackend& backend, const SetCameraControl& setControl) noexcept;

    // Lê direto do backend (sem cache). Câmera ausente -> BackendUnavailable; erro do backend é repassado.
    // Só controles graváveis, ativos e com valor atual; ordem crescente de id.
    [[nodiscard]] shared_kernel::Result<std::vector<CameraControlValue>> currentValues() const;

    // Grava por SetCameraControl::setValue (lease, validação e releitura; ADR-0010) e devolve o valor relido.
    // Releitura sem valor -> MalformedData.
    [[nodiscard]] shared_kernel::Result<std::int64_t> apply(std::uint32_t id, std::int64_t value) const;

private:
    CameraBackend* backend_;             // não nulo
    const SetCameraControl* setControl_; // não nulo
};

} // namespace ghub::camera
