#include "modules/camera/api/CameraSettings.hpp"

#include <algorithm>
#include <utility>

namespace ghub::camera {

CameraSettings::CameraSettings(CameraBackend& backend, const SetCameraControl& setControl) noexcept
    : backend_(&backend), setControl_(&setControl)
{
}

shared_kernel::Result<std::vector<CameraControlValue>> CameraSettings::currentValues() const
{
    auto camera = backend_->readCamera();
    if (!camera.has_value()) {
        return std::unexpected(std::move(camera.error()));
    }
    if (!camera->has_value()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "câmera não detectada");
    }
    std::vector<CameraControlValue> values;
    for (const auto& control : (*camera)->controls) {
        if (control.readOnly() || control.inactive() || control.firmwareManaged() ||
            !control.currentValue().has_value()) {
            continue;
        }
        values.push_back({.id = control.id(), .name = control.name(), .value = *control.currentValue()});
    }
    std::ranges::sort(values, {}, &CameraControlValue::id);
    return values;
}

shared_kernel::Result<std::int64_t> CameraSettings::apply(std::uint32_t id, std::int64_t value) const
{
    const auto written = setControl_->setValue(id, value);
    if (!written.has_value()) {
        return std::unexpected(written.error());
    }
    if (!written->currentValue().has_value()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "controle sem valor após a escrita");
    }
    return *written->currentValue();
}

} // namespace ghub::camera
