#include "modules/audio_processing/domain/BandMeter.hpp"

#include "modules/audio_processing/domain/EqualizerCurve.hpp"
#include "modules/audio_processing/domain/Fft.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace ghub::audio_processing {

namespace {

constexpr double kFloorDb = -60.0;

// Limites de banda em Hz: [lower, upper).
double lowerEdgeHz(std::size_t band)
{
    if (band == 0) {
        return 0.0;
    }
    const auto& f = EqualizerCurve::bandFrequenciesHz;
    return std::sqrt(static_cast<double>(f[band - 1]) * static_cast<double>(f[band])); // NOLINT
}

double upperEdgeHz(std::size_t band, double nyquist)
{
    const auto& f = EqualizerCurve::bandFrequenciesHz;
    if (band + 1 >= BandMeter::kBands) {
        return nyquist + 1.0;
    }
    return std::sqrt(static_cast<double>(f[band]) * static_cast<double>(f[band + 1])); // NOLINT
}

float toLevel(double amplitude)
{
    if (!(amplitude > 1e-9)) {
        return 0.0F;
    }
    const auto db = 20.0 * std::log10(amplitude);
    return static_cast<float>(std::clamp((db - kFloorDb) / -kFloorDb, 0.0, 1.0));
}

} // namespace

BandMeter::BandMeter(double sampleRateHz) : sampleRate_(sampleRateHz), ring_(kWindow, 0.0F) {}

void BandMeter::push(std::span<const float> monoSamples)
{
    for (const float sample : monoSamples) {
        ring_[next_] = std::isfinite(sample)
                           ? sample
                           : 0.0F; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        next_ = (next_ + 1) % kWindow;
        filled_ = std::min(filled_ + 1, kWindow);
    }
}

void BandMeter::clear()
{
    std::ranges::fill(ring_, 0.0F);
    next_ = 0;
    filled_ = 0;
}

std::array<float, BandMeter::kBands> BandMeter::levels() const
{
    std::array<float, kBands> out{};
    if (filled_ == 0) {
        return out;
    }
    // Janela de Hann sobre as últimas kWindow amostras, da mais antiga à mais nova (o que faltar fica em zero).
    std::vector<std::complex<double>> data(kWindow);
    double windowSum = 0.0;
    for (std::size_t i = 0; i < kWindow; ++i) {
        const auto hann =
            0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(kWindow));
        windowSum += hann;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        data[i] = static_cast<double>(ring_[(next_ + i) % kWindow]) * hann;
    }
    if (!fftInPlace(data).has_value()) {
        return out;
    }
    const auto binHz = sampleRate_ / static_cast<double>(kWindow);
    const auto nyquist = sampleRate_ / 2.0;
    for (std::size_t band = 0; band < kBands; ++band) {
        const auto lower = lowerEdgeHz(band);
        const auto upper = upperEdgeHz(band, nyquist);
        double peak = 0.0;
        for (std::size_t bin = 1; bin <= kWindow / 2; ++bin) { // pula o DC
            const auto hz = static_cast<double>(bin) * binHz;
            if (hz >= lower && hz < upper) {
                peak = std::max(peak, 2.0 * std::abs(data[bin]) / windowSum); // NOLINT
            }
        }
        out[band] = toLevel(peak); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
    return out;
}

} // namespace ghub::audio_processing
