#include "modules/lighting/domain/AnimationFrames.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <vector>

using namespace ghub::lighting::domain;
using ghub::shared_kernel::ErrorCode;

namespace {

KeyId keyAt(std::size_t position)
{
    return G915KeyCatalog::keys()[position].id;
}

constexpr RgbColor kBlack{0, 0, 0};
constexpr RgbColor kRed{255, 0, 0};
constexpr RgbColor kBlue{0, 0, 255};

std::size_t keyCount()
{
    return G915KeyCatalog::keys().size();
}

} // namespace

// ---- ciclo de cor ----

TEST(ColorCycleFrameTest, PaintsEveryCatalogKeyWithTheSameHue)
{
    const auto frame = colorCycleFrame(0, {.periodMs = 6000});

    EXPECT_EQ(frame.size(), keyCount());
    for (const auto& [key, color] : frame) {
        EXPECT_EQ(color, kRed) << key.position();
    }
}

TEST(ColorCycleFrameTest, WalksTheHueWheelAtExactPoints)
{
    constexpr ColorCycleParams params{.periodMs = 6000};
    EXPECT_EQ(colorCycleFrame(1000, params).colorOf(keyAt(0)), (RgbColor{255, 255, 0}));
    EXPECT_EQ(colorCycleFrame(2000, params).colorOf(keyAt(0)), (RgbColor{0, 255, 0}));
    EXPECT_EQ(colorCycleFrame(3000, params).colorOf(keyAt(0)), (RgbColor{0, 255, 255}));
    EXPECT_EQ(colorCycleFrame(4000, params).colorOf(keyAt(0)), (RgbColor{0, 0, 255}));
    EXPECT_EQ(colorCycleFrame(5000, params).colorOf(keyAt(0)), (RgbColor{255, 0, 255}));
    EXPECT_EQ(colorCycleFrame(500, params).colorOf(keyAt(0)), (RgbColor{255, 128, 0}));
}

TEST(ColorCycleFrameTest, IsPeriodic)
{
    constexpr ColorCycleParams params{.periodMs = 6000};
    for (const std::int64_t t : {0, 123, 2999, 4001}) {
        EXPECT_EQ(colorCycleFrame(t, params), colorCycleFrame(t + 6000, params)) << t;
        EXPECT_EQ(colorCycleFrame(t, params), colorCycleFrame(t + 12000, params)) << t;
    }
}

TEST(ColorCycleFrameTest, ZeroPeriodDoesNotDivideByZero)
{
    EXPECT_EQ(colorCycleFrame(5000, {.periodMs = 0}).size(), keyCount());
}

TEST(ColorCycleFrameTest, NegativeTimeStaysInsideTheWheel)
{
    constexpr ColorCycleParams params{.periodMs = 6000};
    EXPECT_EQ(colorCycleFrame(-1000, params), colorCycleFrame(5000, params));
}

// ---- onda ----

TEST(WaveFrameTest, KeyZeroStartsRed)
{
    constexpr WaveParams params{.periodMs = 6000, .direction = WaveDirection::LeftToRight};
    const auto frame = waveFrame(0, params);

    EXPECT_EQ(frame.size(), keyCount());
    EXPECT_EQ(frame.colorOf(keyAt(0)), kRed);
}

TEST(WaveFrameTest, DirectionMirrorsTheTravel)
{
    // A tecla 39 fica a 1/3 do catálogo (117 teclas): deslocamento de fase exato de 512/1536.
    constexpr std::size_t kThird = 39;
    ASSERT_EQ(keyCount(), 117U);
    const auto leftToRight = waveFrame(0, {.periodMs = 6000, .direction = WaveDirection::LeftToRight});
    const auto rightToLeft = waveFrame(0, {.periodMs = 6000, .direction = WaveDirection::RightToLeft});

    EXPECT_EQ(leftToRight.colorOf(keyAt(kThird)), kBlue);
    EXPECT_EQ(rightToLeft.colorOf(keyAt(kThird)), (RgbColor{0, 255, 0}));
}

TEST(WaveFrameTest, PatternAdvancesWithTimeInTheChosenDirection)
{
    // Em LeftToRight, a cor da tecla 39 em t = 2000 ms (1/3 do período) é a que a tecla 0 tinha em t = 0.
    constexpr WaveParams params{.periodMs = 6000, .direction = WaveDirection::LeftToRight};
    EXPECT_EQ(waveFrame(2000, params).colorOf(keyAt(39)), waveFrame(0, params).colorOf(keyAt(0)));
}

TEST(WaveFrameTest, SpeedFollowsThePeriod)
{
    constexpr WaveParams slow{.periodMs = 6000, .direction = WaveDirection::LeftToRight};
    constexpr WaveParams fast{.periodMs = 3000, .direction = WaveDirection::LeftToRight};
    EXPECT_EQ(waveFrame(1500, fast), waveFrame(3000, slow));
}

TEST(WaveFrameTest, IsPeriodic)
{
    constexpr WaveParams params{.periodMs = 4000, .direction = WaveDirection::RightToLeft};
    for (const std::int64_t t : {0, 777, 3999}) {
        EXPECT_EQ(waveFrame(t, params), waveFrame(t + 4000, params)) << t;
    }
}

// ---- estrela ----

TEST(StarFrameTest, ZeroDensityKeepsEveryKeyBlack)
{
    for (const std::int64_t t : {0, 500, 1500}) {
        const auto frame = starFrame(t, {.densityPercent = 0, .color = kRed, .seed = 7});
        EXPECT_EQ(frame.size(), keyCount());
        for (const auto& entry : frame) {
            EXPECT_EQ(entry.second, kBlack);
        }
    }
}

TEST(StarFrameTest, FullDensityMakesEveryKeyTwinkle)
{
    constexpr StarParams params{.densityPercent = 100, .color = {200, 200, 200}, .seed = 1};
    for (std::size_t position = 0; position < keyCount(); ++position) {
        std::set<int> seen;
        for (std::int64_t t = 0; t < 2000; t += 100) {
            seen.insert(starFrame(t, params).colorOf(keyAt(position))->red());
        }
        EXPECT_TRUE(seen.contains(0)) << position;
        EXPECT_TRUE(seen.contains(200)) << position;
    }
}

TEST(StarFrameTest, EachStarFollowsATriangleBrightnessCurve)
{
    // Triângulo b(t) + b(t + 1000) = 1000: com cor 200 e passos de 100 ms a soma dos canais é exatamente 200.
    constexpr StarParams params{.densityPercent = 100, .color = {200, 200, 200}, .seed = 3};
    for (const std::int64_t t : {0, 300, 700, 1100}) {
        const auto a = starFrame(t, params);
        const auto b = starFrame(t + 1000, params);
        for (std::size_t position = 0; position < keyCount(); ++position) {
            EXPECT_EQ(a.colorOf(keyAt(position))->red() + b.colorOf(keyAt(position))->red(), 200)
                << position << " t=" << t;
        }
    }
}

TEST(StarFrameTest, ColorIsScaledNotReplaced)
{
    constexpr StarParams params{.densityPercent = 100, .color = {200, 100, 0}, .seed = 5};
    for (const auto& entry : starFrame(300, params)) {
        EXPECT_EQ(entry.second.green() * 2, entry.second.red());
        EXPECT_EQ(entry.second.blue(), 0);
    }
}

TEST(StarFrameTest, SameSeedGivesSameFrameAndOtherSeedDiffers)
{
    constexpr StarParams a{.densityPercent = 50, .color = kRed, .seed = 42};
    constexpr StarParams b{.densityPercent = 50, .color = kRed, .seed = 43};
    EXPECT_EQ(starFrame(250, a), starFrame(250, a));
    EXPECT_NE(starFrame(250, a), starFrame(250, b));
}

TEST(StarFrameTest, DensityControlsHowManyKeysAreStars)
{
    const auto starsAt = [](std::uint8_t density) {
        std::size_t stars = 0;
        for (std::size_t position = 0; position < keyCount(); ++position) {
            bool lit = false;
            for (std::int64_t t = 0; t < 2000; t += 100) {
                const auto frame = starFrame(t, {.densityPercent = density, .color = kRed, .seed = 9});
                lit = lit || frame.colorOf(keyAt(position)) != kBlack;
            }
            stars += lit ? 1U : 0U;
        }
        return stars;
    };
    const auto few = starsAt(10);
    const auto half = starsAt(50);
    EXPECT_GT(few, 0U);
    EXPECT_LT(few, half);
    EXPECT_GT(half, keyCount() / 4);
    EXPECT_LT(half, keyCount() * 3 / 4);
    EXPECT_EQ(starsAt(100), keyCount());
}

TEST(StarFrameTest, DensityAbove100IsClamped)
{
    EXPECT_EQ(starFrame(300, {.densityPercent = 250, .color = kRed, .seed = 1}),
              starFrame(300, {.densityPercent = 100, .color = kRed, .seed = 1}));
}

TEST(StarFrameTest, IsPeriodicEvery2000Ms)
{
    constexpr StarParams params{.densityPercent = 60, .color = kBlue, .seed = 11};
    EXPECT_EQ(starFrame(123, params), starFrame(2123, params));
}

// ---- keyframes ----

TEST(KeyframeTrackTest, RejectsEmptyAndNonIncreasingTimes)
{
    EXPECT_EQ(KeyframeTrack::create({}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(KeyframeTrack::create({{0, kRed}, {0, kBlue}}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(KeyframeTrack::create({{100, kRed}, {50, kBlue}}).error().code, ErrorCode::InvalidArgument);
}

TEST(KeyframeTrackTest, InterpolatesLinearlyAndLoops)
{
    const auto track = KeyframeTrack::create({{0, kRed}, {1000, kBlue}, {2000, kRed}});
    ASSERT_TRUE(track.has_value());

    EXPECT_EQ(keyframeFrame(0, *track).colorOf(keyAt(0)), kRed);
    EXPECT_EQ(keyframeFrame(500, *track).colorOf(keyAt(0)), (RgbColor{128, 0, 128}));
    EXPECT_EQ(keyframeFrame(1000, *track).colorOf(keyAt(0)), kBlue);
    EXPECT_EQ(keyframeFrame(1500, *track).colorOf(keyAt(0)), (RgbColor{128, 0, 128}));
    EXPECT_EQ(keyframeFrame(2000, *track).colorOf(keyAt(0)), kRed);
    EXPECT_EQ(keyframeFrame(2500, *track), keyframeFrame(500, *track));
}

TEST(KeyframeTrackTest, PaintsEveryKeyAndSingleKeyframeIsConstant)
{
    const auto track = KeyframeTrack::create({{0, kBlue}});
    ASSERT_TRUE(track.has_value());
    const auto frame = keyframeFrame(12345, *track);
    EXPECT_EQ(frame.size(), keyCount());
    EXPECT_EQ(frame.colorOf(keyAt(keyCount() - 1)), kBlue);
}

TEST(KeyframeTrackTest, BeforeTheFirstKeyframeHoldsTheFirstColor)
{
    const auto track = KeyframeTrack::create({{500, kRed}, {1500, kBlue}});
    ASSERT_TRUE(track.has_value());
    EXPECT_EQ(keyframeFrame(100, *track).colorOf(keyAt(0)), kRed);
}

// ---- barras de áudio ----

TEST(AudioBarsFrameTest, SilenceIsAllBlack)
{
    const auto frame = audioBarsFrame({}, kRed);
    EXPECT_EQ(frame.size(), keyCount());
    for (const auto& entry : frame) {
        EXPECT_EQ(entry.second, kBlack);
    }
}

TEST(AudioBarsFrameTest, FullLevelLightsEveryKeyOfThatBandOnly)
{
    std::array<float, 10> levels{};
    levels[3] = 1.0F;
    const auto frame = audioBarsFrame(levels, kRed);

    std::size_t lit = 0;
    for (const auto& [key, color] : frame) {
        const auto band = key.position() * 10 / keyCount();
        EXPECT_EQ(color, band == 3 ? kRed : kBlack) << key.position();
        lit += color == kRed ? 1U : 0U;
    }
    EXPECT_GT(lit, 0U);
}

TEST(AudioBarsFrameTest, PartialLevelLightsAProportionalPrefixOfTheBand)
{
    std::array<float, 10> levels{};
    levels[0] = 0.5F;
    const auto frame = audioBarsFrame(levels, kRed);

    std::vector<std::size_t> bandKeys;
    for (std::size_t position = 0; position < keyCount(); ++position) {
        if (position * 10 / keyCount() == 0) {
            bandKeys.push_back(position);
        }
    }
    const auto expectedLit = (bandKeys.size() + 1) / 2; // ceil(0,5 * n)
    for (std::size_t i = 0; i < bandKeys.size(); ++i) {
        EXPECT_EQ(frame.colorOf(keyAt(bandKeys[i])), i < expectedLit ? kRed : kBlack) << i;
    }
}

TEST(AudioBarsFrameTest, OutOfRangeAndNanLevelsAreClamped)
{
    std::array<float, 10> levels{};
    levels[0] = 7.0F;
    levels[1] = -2.0F;
    levels[2] = std::numeric_limits<float>::quiet_NaN();
    const auto frame = audioBarsFrame(levels, kRed);

    std::array<float, 10> expected{};
    expected[0] = 1.0F;
    EXPECT_EQ(frame, audioBarsFrame(expected, kRed));
}

// ---- despacho por especificação ----

TEST(AnimationSpecTest, DispatchesToTheMatchingFrameFunction)
{
    constexpr ColorCycleParams cycle{.periodMs = 6000};
    constexpr WaveParams wave{.periodMs = 4000, .direction = WaveDirection::RightToLeft};
    constexpr StarParams stars{.densityPercent = 40, .color = kBlue, .seed = 8};
    const auto track = KeyframeTrack::create({{0, kRed}, {1000, kBlue}});
    ASSERT_TRUE(track.has_value());

    EXPECT_EQ(animationFrame(700, AnimationSpec{cycle}), colorCycleFrame(700, cycle));
    EXPECT_EQ(animationFrame(700, AnimationSpec{wave}), waveFrame(700, wave));
    EXPECT_EQ(animationFrame(700, AnimationSpec{stars}), starFrame(700, stars));
    EXPECT_EQ(animationFrame(700, AnimationSpec{*track}), keyframeFrame(700, *track));
}
