#pragma once

#include "modules/camera/domain/CameraControl.hpp"
#include "modules/camera/domain/VideoMode.hpp"

#include <string>
#include <vector>

namespace ghub::camera {

// O que se leu da câmera: nome, modos de captura e controles.
struct CameraDescription {
    std::string name;
    std::vector<VideoMode> modes;
    std::vector<CameraControl> controls;

    friend bool operator==(const CameraDescription&, const CameraDescription&) = default;
};

} // namespace ghub::camera
