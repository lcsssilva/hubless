#include "modules/audio_processing/application/HostNoiseSuppression.hpp"

#include "modules/audio_processing/application/DefaultPreference.hpp"

namespace ghub::audio_processing {

HostNoiseSuppression::HostNoiseSuppression(NoiseSuppressionGraph& graph) noexcept : graph_(graph) {}

shared_kernel::Result<void> HostNoiseSuppression::enable()
{
    if (enabled_) {
        return {};
    }
    const auto source = graph_.findHeadsetSource();
    if (!source) {
        return std::unexpected(source.error());
    }
    if (auto created = graph_.createNoiseSuppression(threshold_, *source); !created) {
        return created;
    }
    const auto current = graph_.configuredDefaultSource();
    if (!current) {
        static_cast<void>(graph_.removeNoiseSuppression());
        return std::unexpected(current.error());
    }
    if (auto written = graph_.setConfiguredDefaultSource(graph_.noiseSuppressionSourceName()); !written) {
        static_cast<void>(graph_.removeNoiseSuppression());
        return written;
    }
    savedDefault_ = *current;
    enabled_ = true;
    return {};
}

shared_kernel::Result<void> HostNoiseSuppression::changeThreshold(VoiceThreshold threshold)
{
    if (enabled_) {
        if (auto updated = graph_.updateNoiseSuppression(threshold); !updated) {
            return updated;
        }
    }
    threshold_ = threshold;
    return {};
}

shared_kernel::Result<void> HostNoiseSuppression::disable()
{
    if (!enabled_) {
        return {};
    }
    enabled_ = false;
    auto restored = restoreDefaultIfStillOurs(
        [this] { return graph_.configuredDefaultSource(); },
        [this](const std::optional<std::string>& value) { return graph_.setConfiguredDefaultSource(value); },
        graph_.noiseSuppressionSourceName(), savedDefault_);
    auto removed = graph_.removeNoiseSuppression();
    if (!restored) {
        return restored;
    }
    return removed;
}

void HostNoiseSuppression::noiseSuppressionLost()
{
    static_cast<void>(disable());
}

} // namespace ghub::audio_processing
