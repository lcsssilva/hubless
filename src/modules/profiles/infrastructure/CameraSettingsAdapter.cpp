#include "modules/profiles/infrastructure/CameraSettingsAdapter.hpp"

#include <utility>

namespace ghub::profiles {

shared_kernel::Result<std::vector<CameraControlSetting>> CameraSettingsAdapter::readCurrent()
{
    auto values = settings_->currentValues();
    if (!values) {
        return std::unexpected(values.error());
    }
    std::vector<CameraControlSetting> settings;
    settings.reserve(values->size());
    for (auto& value : *values) {
        settings.push_back({.id = value.id, .name = std::move(value.name), .value = value.value});
    }
    return settings;
}

shared_kernel::Result<std::int64_t> CameraSettingsAdapter::apply(std::uint32_t id, std::int64_t value)
{
    return settings_->apply(id, value);
}

} // namespace ghub::profiles
