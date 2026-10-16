#include "modules/audio_processing/domain/BandMeter.hpp"
#include "modules/audio_processing/domain/Fft.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

using ghub::audio_processing::BandMeter;
using ghub::audio_processing::fftInPlace;
using ghub::shared_kernel::ErrorCode;

namespace {

constexpr double kRate = 48000.0;

std::vector<float> sine(double frequencyHz, double amplitude, std::size_t count)
{
    std::vector<float> out(count);
    for (std::size_t i = 0; i < count; ++i) {
        out[i] = static_cast<float>(amplitude *
                                    std::sin(2.0 * std::numbers::pi * frequencyHz * static_cast<double>(i) / kRate));
    }
    return out;
}

std::size_t loudestBand(const std::array<float, 10>& levels)
{
    return static_cast<std::size_t>(std::ranges::max_element(levels) - levels.begin());
}

} // namespace

// ---- FFT ----

TEST(FftTest, RejectsSizesThatAreNotAPowerOfTwo)
{
    std::vector<std::complex<double>> data(12);
    const auto result = fftInPlace(data);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);

    std::vector<std::complex<double>> empty;
    EXPECT_FALSE(fftInPlace(empty).has_value());
}

TEST(FftTest, ImpulseHasFlatSpectrum)
{
    std::vector<std::complex<double>> data(8);
    data[0] = 1.0;
    ASSERT_TRUE(fftInPlace(data).has_value());
    for (const auto& bin : data) {
        EXPECT_NEAR(bin.real(), 1.0, 1e-12);
        EXPECT_NEAR(bin.imag(), 0.0, 1e-12);
    }
}

TEST(FftTest, ConstantSignalLandsOnDc)
{
    std::vector<std::complex<double>> data(16, 1.0);
    ASSERT_TRUE(fftInPlace(data).has_value());
    EXPECT_NEAR(data[0].real(), 16.0, 1e-9);
    for (std::size_t i = 1; i < data.size(); ++i) {
        EXPECT_NEAR(std::abs(data[i]), 0.0, 1e-9) << i;
    }
}

TEST(FftTest, CosineOnBinKSplitsIntoKAndMinusK)
{
    constexpr std::size_t n = 64;
    constexpr std::size_t k = 5;
    std::vector<std::complex<double>> data(n);
    for (std::size_t i = 0; i < n; ++i) {
        data[i] = std::cos(2.0 * std::numbers::pi * static_cast<double>(k * i) / static_cast<double>(n));
    }
    ASSERT_TRUE(fftInPlace(data).has_value());
    EXPECT_NEAR(std::abs(data[k]), n / 2.0, 1e-9);
    EXPECT_NEAR(std::abs(data[n - k]), n / 2.0, 1e-9);
    EXPECT_NEAR(std::abs(data[k + 1]), 0.0, 1e-9);
}

TEST(FftTest, SingleSampleIsItself)
{
    std::vector<std::complex<double>> data{{3.0, 0.0}};
    ASSERT_TRUE(fftInPlace(data).has_value());
    EXPECT_NEAR(data[0].real(), 3.0, 1e-12);
}

// ---- BandMeter ----

TEST(BandMeterTest, SilenceGivesExactlyZeroInEveryBand)
{
    BandMeter meter{kRate};
    meter.push(std::vector<float>(8192, 0.0F));

    for (const float level : meter.levels()) {
        EXPECT_EQ(level, 0.0F);
    }
}

TEST(BandMeterTest, NothingPushedGivesZero)
{
    const BandMeter meter{kRate};
    for (const float level : meter.levels()) {
        EXPECT_EQ(level, 0.0F);
    }
}

TEST(BandMeterTest, SineAtEachBandCenterLandsInThatBand)
{
    constexpr std::array<double, 10> centers{32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (std::size_t band = 0; band < centers.size(); ++band) {
        BandMeter meter{kRate};
        meter.push(sine(centers[band], 1.0, 8192));

        const auto levels = meter.levels();

        EXPECT_EQ(loudestBand(levels), band) << centers[band] << " Hz";
        if (band >= 2) {
            EXPECT_GT(levels[band], 0.9F) << centers[band] << " Hz";
        }
        for (std::size_t other = 0; other < levels.size(); ++other) {
            if (other + 1 < band || other > band + 1) { // longe da banda: vazamento desprezível
                EXPECT_LT(levels[other], 0.2F) << centers[band] << " Hz, banda " << other;
            }
        }
    }
}

TEST(BandMeterTest, LouderSignalGivesHigherLevelInDecibelScale)
{
    BandMeter quiet{kRate};
    BandMeter loud{kRate};
    quiet.push(sine(1000.0, 0.01, 8192));
    loud.push(sine(1000.0, 0.5, 8192));

    EXPECT_LT(quiet.levels()[5], loud.levels()[5]);
    EXPECT_GT(quiet.levels()[5], 0.0F);
    EXPECT_LE(loud.levels()[5], 1.0F);
}

TEST(BandMeterTest, UsesOnlyTheMostRecentWindow)
{
    BandMeter meter{kRate};
    meter.push(sine(8000.0, 1.0, 8192));
    meter.push(sine(250.0, 1.0, 8192)); // preenche a janela inteira

    EXPECT_EQ(loudestBand(meter.levels()), 3U);
    EXPECT_LT(meter.levels()[8], 0.2F);
}

TEST(BandMeterTest, ShortPushesAccumulateLikeOneLongPush)
{
    const auto tone = sine(2000.0, 1.0, 8192);
    BandMeter chunked{kRate};
    BandMeter whole{kRate};
    whole.push(tone);
    for (std::size_t offset = 0; offset < tone.size(); offset += 480) {
        const auto count = std::min<std::size_t>(480, tone.size() - offset);
        chunked.push(std::span<const float>{tone}.subspan(offset, count));
    }

    EXPECT_EQ(chunked.levels(), whole.levels());
}

TEST(BandMeterTest, NonFiniteSamplesAreTreatedAsSilence)
{
    BandMeter meter{kRate};
    std::vector<float> bad(8192, std::numeric_limits<float>::quiet_NaN());
    bad[10] = std::numeric_limits<float>::infinity();
    meter.push(bad);

    for (const float level : meter.levels()) {
        EXPECT_EQ(level, 0.0F);
    }
}

TEST(BandMeterTest, ClearForgetsEverything)
{
    BandMeter meter{kRate};
    meter.push(sine(1000.0, 1.0, 8192));
    meter.clear();

    for (const float level : meter.levels()) {
        EXPECT_EQ(level, 0.0F);
    }
}
