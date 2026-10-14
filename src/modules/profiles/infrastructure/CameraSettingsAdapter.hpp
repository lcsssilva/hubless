#pragma once

#include "modules/camera/api/CameraSettings.hpp"
#include "modules/profiles/application/CameraSettingsPort.hpp"

namespace ghub::profiles {

// Liga o port de câmera do módulo profiles à API pública do módulo camera (ADR-0020). Bloqueante.
// A fachada precisa sobreviver a este objeto.
class CameraSettingsAdapter final : public CameraSettingsPort {
public:
    explicit CameraSettingsAdapter(const camera::CameraSettings& settings) noexcept : settings_(&settings) {}

    [[nodiscard]] shared_kernel::Result<std::vector<CameraControlSetting>> readCurrent() override;
    [[nodiscard]] shared_kernel::Result<std::int64_t> apply(std::uint32_t id, std::int64_t value) override;

private:
    const camera::CameraSettings* settings_; // não nulo
};

} // namespace ghub::profiles
