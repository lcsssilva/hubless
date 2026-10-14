#include "modules/camera/application/StartCameraPreview.hpp"

#include <utility>

namespace ghub::camera {

std::expected<PreviewMode, PreviewError> StartCameraPreview::execute() const
{
    auto camera = backend_->readCamera();
    if (!camera.has_value()) {
        return std::unexpected(
            PreviewError{.reason = PreviewFailure::CameraUnavailable, .detail = std::move(camera.error().detail)});
    }
    if (!camera->has_value()) {
        return std::unexpected(PreviewError{.reason = PreviewFailure::CameraNotDetected, .detail = {}});
    }
    const auto mode = selectPreviewMode((*camera)->modes);
    if (!mode.has_value()) {
        return std::unexpected(PreviewError{.reason = PreviewFailure::NoCompatibleMode, .detail = {}});
    }
    runner_->start(*mode);
    return *mode;
}

} // namespace ghub::camera
