#include "modules/camera/application/GetCameraStatus.hpp"

namespace ghub::camera {

CameraStatus GetCameraStatus::execute() const
{
    auto result = backend_->readCamera();
    if (!result.has_value()) {
        return CameraStatus::unavailable(std::move(result.error()));
    }
    if (!result->has_value()) {
        return CameraStatus::notDetected();
    }
    return CameraStatus::ready(std::move(**result));
}

} // namespace ghub::camera
