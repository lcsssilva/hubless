#include "modules/audio_processing/infrastructure/PipeWireFilterChainArgs.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>

namespace ghub::audio_processing {
namespace {

constexpr double bandQ = 1.41; // uma oitava (ADR-0016)

[[nodiscard]] double linearGain(int db) noexcept
{
    return std::pow(10.0, static_cast<double>(db) / 20.0);
}

[[nodiscard]] bool isSafeInsideQuotes(const std::string& text) noexcept
{
    return !text.empty() && std::ranges::none_of(text, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return c == '"' || c == '\\' || u < 0x20 || u == 0x7f;
    });
}

} // namespace

shared_kernel::Result<std::string> buildFilterChainArgs(const EqualizerCurve& curve, const std::string& targetSink,
                                                        const std::string& remoteName)
{
    if (!isSafeInsideQuotes(targetSink)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "nome do sink vazio ou com aspas, barra invertida ou caractere de controle");
    }
    if (!remoteName.empty() && !isSafeInsideQuotes(remoteName)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "remote.name com aspas, barra invertida ou caractere de controle");
    }

    std::string args = "{ node.description = \"G733 + EQ (G HUB Linux)\" audio.channels = 2 audio.position = [ FL FR ] "
                       "filter.graph = { nodes = [ ";
    args += std::format("{{ name = pre type = builtin label = mixer control = {{ \"Gain 1\" = {:.6f} }} }} ",
                        linearGain(curve.preampDb()));
    for (std::size_t band = 0; band < EqualizerCurve::bandCount; ++band) {
        args += std::format("{{ name = eq_{} type = builtin label = bq_peaking control = {{ \"Freq\" = {:.6f} \"Q\" = "
                            "{:.6f} \"Gain\" = {:.6f} }} }} ",
                            band + 1, static_cast<double>(EqualizerCurve::bandFrequenciesHz[band]), bandQ,
                            static_cast<double>(curve.gainsDb()[band]));
    }
    args += "] links = [ { output = \"pre:Out\" input = \"eq_1:In\" } ";
    for (std::size_t band = 1; band < EqualizerCurve::bandCount; ++band) {
        args += std::format("{{ output = \"eq_{}:Out\" input = \"eq_{}:In\" }} ", band, band + 1);
    }
    args += "] inputs = [ \"pre:In 1\" ] outputs = [ \"eq_10:Out\" ] } ";
    args += "capture.props = { node.name = \"ghub.g733.eq\" media.class = Audio/Sink } ";
    args += std::format("playback.props = {{ node.name = \"ghub.g733.eq.output\" target.object = \"{}\" "
                        "node.dont-fallback = true node.dont-move = true node.passive = true }}",
                        targetSink);
    args += remoteName.empty() ? std::string{" "} : std::format(" remote.name = \"{}\" ", remoteName);
    args += "}";
    return args;
}

std::vector<FilterChainControl> buildControlUpdates(const EqualizerCurve& curve)
{
    std::vector<FilterChainControl> updates;
    updates.reserve(EqualizerCurve::bandCount + 1);
    updates.push_back({"pre:Gain 1", static_cast<float>(linearGain(curve.preampDb()))});
    for (std::size_t band = 0; band < EqualizerCurve::bandCount; ++band) {
        updates.push_back({std::format("eq_{}:Gain", band + 1), static_cast<float>(curve.gainsDb()[band])});
    }
    return updates;
}

shared_kernel::Result<std::string> buildNoiseSuppressionArgs(VoiceThreshold threshold, const std::string& source,
                                                             const std::string& remoteName)
{
    if (!isSafeInsideQuotes(source)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "nome da fonte vazio ou com aspas, barra invertida ou caractere de controle");
    }
    if (!remoteName.empty() && !isSafeInsideQuotes(remoteName)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "remote.name com aspas, barra invertida ou caractere de controle");
    }
    std::string args = "{ node.description = \"G733 + Redução de ruído\" audio.rate = 48000 audio.channels = 1 "
                       "audio.position = [ MONO ] ";
    args += std::format("filter.graph = {{ nodes = [ {{ type = ladspa name = rnnoise plugin = "
                        "\"librnnoise_ladspa\" label = noise_suppressor_mono control = {{ \"VAD Threshold (%)\" "
                        "{:.6f} }} }} ] }} ",
                        static_cast<double>(threshold.percent()));
    args += std::format("capture.props = {{ node.name = \"ghub.g733.mic.input\" target.object = \"{}\" "
                        "node.dont-fallback = true node.dont-move = true node.passive = true }} ",
                        source);
    args += "playback.props = { node.name = \"ghub.g733.mic\" media.class = Audio/Source }";
    args += remoteName.empty() ? std::string{" "} : std::format(" remote.name = \"{}\" ", remoteName);
    args += '}';
    return args;
}

std::vector<FilterChainControl> buildNoiseSuppressionControlUpdates(VoiceThreshold threshold)
{
    return {{.name = "rnnoise:VAD Threshold (%)", .value = static_cast<float>(threshold.percent())}};
}

} // namespace ghub::audio_processing
