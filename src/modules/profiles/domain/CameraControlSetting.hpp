#pragma once

#include <cstdint>
#include <string>

namespace ghub::profiles {

// Valor de um controle da câmera dentro de um perfil. O nome serve só para exibição; a faixa do valor é
// validada pelo módulo dono (camera) ao aplicar (ADR-0020).
struct CameraControlSetting {
    std::uint32_t id;
    std::string name;
    std::int64_t value;

    friend bool operator==(const CameraControlSetting&, const CameraControlSetting&) = default;
};

} // namespace ghub::profiles
