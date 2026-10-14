#include "modules/audio_processing/application/HostEqualizer.hpp"

#include "modules/audio_processing/application/DefaultPreference.hpp"

namespace ghub::audio_processing {

HostEqualizer::HostEqualizer(AudioGraph& graph) noexcept : graph_(graph) {}

shared_kernel::Result<void> HostEqualizer::enable()
{
    if (enabled_) {
        return {};
    }
    const auto sink = graph_.findHeadsetSink();
    if (!sink) {
        return std::unexpected(sink.error());
    }
    if (auto created = graph_.createEqualizer(curve_, *sink); !created) {
        return created;
    }
    const auto current = graph_.configuredDefaultSink();
    if (!current) {
        static_cast<void>(graph_.removeEqualizer());
        return std::unexpected(current.error());
    }
    if (auto written = graph_.setConfiguredDefaultSink(graph_.equalizerSinkName()); !written) {
        static_cast<void>(graph_.removeEqualizer());
        return written;
    }
    savedDefault_ = *current;
    enabled_ = true;
    return {};
}

shared_kernel::Result<void> HostEqualizer::changeCurve(const EqualizerCurve& curve)
{
    if (enabled_) {
        if (auto updated = graph_.updateEqualizer(curve); !updated) {
            return updated;
        }
    }
    curve_ = curve;
    return {};
}

shared_kernel::Result<void> HostEqualizer::disable()
{
    if (!enabled_) {
        return {};
    }
    enabled_ = false;
    auto restored = restoreDefaultIfStillOurs(
        [this] { return graph_.configuredDefaultSink(); },
        [this](const std::optional<std::string>& value) { return graph_.setConfiguredDefaultSink(value); },
        graph_.equalizerSinkName(), savedDefault_);
    auto removed = graph_.removeEqualizer();
    if (!restored) {
        return restored;
    }
    return removed;
}

void HostEqualizer::equalizerLost()
{
    static_cast<void>(disable());
}

} // namespace ghub::audio_processing
