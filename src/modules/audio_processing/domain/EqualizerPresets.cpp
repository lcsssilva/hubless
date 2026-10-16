#include "modules/audio_processing/domain/EqualizerPresets.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace ghub::audio_processing {

namespace {

using Gains = std::array<int, EqualizerCurve::bandCount>;

// Pesos em quartos (4 = 1,0; 3 = 0,75; 2 = 0,5).
constexpr std::array<int, 4> bassWeights{4, 4, 3, 2};
constexpr std::array<int, 4> trebleWeights{2, 3, 4, 4};
constexpr std::size_t firstTrebleBand = EqualizerCurve::bandCount - trebleWeights.size();

constexpr int inputGuard = 100;

int scaled(int gainDb, int quarters) noexcept
{
    // Meio-ponto para longe de zero, em inteiros.
    const int product = gainDb * quarters;
    const int magnitude = (std::abs(product) + 2) / 4;
    return product < 0 ? -magnitude : magnitude;
}

int limited(int gainDb) noexcept
{
    return std::clamp(gainDb, EqualizerCurve::minimumGainDb, EqualizerCurve::maximumGainDb);
}

EqualizerCurve curveOf(const Gains& gains)
{
    return EqualizerCurve::create(gains).value(); // valores constantes dentro da faixa
}

const std::array<EqualizerPresets::Preset, 2>& presets()
{
    static const std::array<EqualizerPresets::Preset, 2> list{{
        {.name = "Plano", .curve = EqualizerCurve::flat()},
        {.name = "Aumento de grave", .curve = curveOf({4, 3, 2, 1, 1, 0, 0, 0, 0, 0})},
    }};
    return list;
}

} // namespace

std::span<const EqualizerPresets::Preset> EqualizerPresets::all() noexcept
{
    return presets();
}

EqualizerCurve EqualizerPresets::fromBassTreble(int bassDb, int trebleDb) noexcept
{
    // Só o resultado é limitado a -12..12 (R7); a guarda de entrada evita estouro de int e já satura qualquer banda.
    const int bass = std::clamp(bassDb, -inputGuard, inputGuard);
    const int treble = std::clamp(trebleDb, -inputGuard, inputGuard);
    Gains gains{};
    for (std::size_t i = 0; i < bassWeights.size(); ++i) {
        gains.at(i) = limited(scaled(bass, bassWeights.at(i)));
    }
    for (std::size_t i = 0; i < trebleWeights.size(); ++i) {
        gains.at(firstTrebleBand + i) = limited(scaled(treble, trebleWeights.at(i)));
    }
    return EqualizerCurve::create(gains).value();
}

} // namespace ghub::audio_processing
