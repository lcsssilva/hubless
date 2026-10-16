#include "modules/lighting/domain/StoredEffect.hpp"

#include "fakes/G915SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

// Entradas de 11 bytes do bloco de iluminação (0xD0) do perfil onboard do G915, conferidas com cópias reais da
// memória lidas em 2026-10-15 (docs/device-support/evidence/2026-10-15-c04-c06-g915-captura-inc015.md).
namespace {

using namespace ghub::lighting::domain;
using ghub::shared_kernel::ErrorCode;
namespace fx = ghub::fixtures::g915;

constexpr std::size_t kBlock = 0xD0;

StoredEffectBytes entry(const std::array<std::uint8_t, 255>& sector, std::size_t index)
{
    StoredEffectBytes bytes{};
    std::copy_n(sector.begin() + static_cast<std::ptrdiff_t>(kBlock + index * kStoredEffectSize), kStoredEffectSize,
                bytes.begin());
    return bytes;
}

EffectPeriod period(std::uint32_t ms)
{
    return EffectPeriod::create(ms).value();
}
EffectBrightness brightness(std::uint32_t percent)
{
    return EffectBrightness::create(percent).value();
}
RippleSpeed speed(std::uint32_t value)
{
    return RippleSpeed::create(value).value();
}
constexpr RgbColor black{0, 0, 0};

} // namespace

TEST(StoredEffect, DecodesTheStaticBlackAndRippleOfTheOriginalM1)
{
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M1, 0)), KeyboardEffect::staticColor(black));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M1, 1)), KeyboardEffect::ripple(black, speed(20)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M1, 2)), KeyboardEffect::staticColor(black));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M1, 3)), KeyboardEffect::ripple(black, speed(20)));
}

TEST(StoredEffect, DecodesTheRippleSpeedTheUserChangedInGHub)
{
    EXPECT_EQ(decodeStoredEffect(entry(fx::kFinalM1, 1)), KeyboardEffect::ripple(black, speed(2)));
}

TEST(StoredEffect, DecodesBreathingWithColorPeriodAndBrightnessOfM2)
{
    const RgbColor cyan{0x00, 0xdc, 0xff};
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M2, 0)), KeyboardEffect::breathing(cyan, period(3000), brightness(100)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M2, 1)), KeyboardEffect::breathing(cyan, period(3000), brightness(100)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M2, 2)), KeyboardEffect::breathing(cyan, period(3000), brightness(50)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M2, 3)), KeyboardEffect::breathing(cyan, period(3000), brightness(50)));
}

TEST(StoredEffect, DecodesCycleOfM3AndOfTheGHubProfile)
{
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M3, 0)), KeyboardEffect::cycle(period(5000), brightness(100)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kBeforeC04M3, 2)), KeyboardEffect::cycle(period(5000), brightness(50)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kAfterC06M1, 0)), KeyboardEffect::cycle(period(5000), brightness(100)));
}

TEST(StoredEffect, DecodesWaveOfTheGHubProfileAndOfTheFactoryRom)
{
    EXPECT_EQ(decodeStoredEffect(entry(fx::kAfterC06M1, 1)), KeyboardEffect::wave(period(5000), brightness(100)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kAfterC06M1, 3)), KeyboardEffect::wave(period(5000), brightness(50)));
    // Cópia da ROM (Restaurar o perfil padrão): onda 2100 ms (0x0834 = período alto 08, baixo 34).
    EXPECT_EQ(decodeStoredEffect(entry(fx::kAfterRestoreM1, 1)), KeyboardEffect::wave(period(2100), brightness(100)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kAfterRestoreM1, 3)), KeyboardEffect::wave(period(2100), brightness(50)));
    EXPECT_EQ(decodeStoredEffect(entry(fx::kAfterRestoreM1, 0)), KeyboardEffect::cycle(period(2100), brightness(100)));
}

TEST(StoredEffect, EveryRealEntryEncodesBackToTheIdenticalBytes)
{
    const std::array<const std::array<std::uint8_t, 255>*, 6> sectors{&fx::kBeforeC04M1,  &fx::kBeforeC04M2,
                                                                      &fx::kBeforeC04M3,  &fx::kAfterC06M1,
                                                                      &fx::kAfterRestoreM1, &fx::kFinalM1};
    for (const auto* sector : sectors) {
        for (std::size_t index = 0; index < 4; ++index) {
            const auto original = entry(*sector, index);
            const auto decoded = decodeStoredEffect(original);
            ASSERT_TRUE(decoded.has_value()) << "entrada " << index;
            const auto encoded = encodeStoredEffect(*decoded, original);
            ASSERT_TRUE(encoded.has_value());
            EXPECT_EQ(*encoded, original) << "entrada " << index;
        }
    }
}

TEST(StoredEffect, UnconfirmedFormatsStayRaw)
{
    auto cycle = entry(fx::kBeforeC04M3, 0);
    cycle[0] = 0x02; // efeito sem evidência de formato
    EXPECT_FALSE(decodeStoredEffect(cycle).has_value());
    cycle[0] = 0x05;
    EXPECT_FALSE(decodeStoredEffect(cycle).has_value());

    auto staticEffect = entry(fx::kBeforeC04M1, 0);
    staticEffect[4] = 0x00; // sem o marcador 02
    EXPECT_FALSE(decodeStoredEffect(staticEffect).has_value());

    auto breathing = entry(fx::kBeforeC04M2, 0);
    breathing[6] = 0x02; // p5 diferente de 01
    EXPECT_FALSE(decodeStoredEffect(breathing).has_value());

    auto wave = entry(fx::kAfterC06M1, 1);
    wave[8] = 0x02; // direção diferente de 01
    EXPECT_FALSE(decodeStoredEffect(wave).has_value());

    auto invalidPeriod = entry(fx::kBeforeC04M3, 0);
    invalidPeriod[6] = 0x00;
    invalidPeriod[7] = 0x10; // 16 ms
    EXPECT_FALSE(decodeStoredEffect(invalidPeriod).has_value());

    auto invalidBrightness = entry(fx::kBeforeC04M3, 0);
    invalidBrightness[8] = 0x00;
    EXPECT_FALSE(decodeStoredEffect(invalidBrightness).has_value());
}

TEST(StoredEffect, ChangingTheKindBuildsTheExactBytesTheGHubWrote)
{
    // M1 original (Static/Ripple) -> perfil do G HUB (Cycle/Wave 5000 ms, brilho 100).
    EXPECT_EQ(encodeStoredEffect(KeyboardEffect::cycle(period(5000), brightness(100)), entry(fx::kBeforeC04M1, 0)).value(),
              entry(fx::kAfterC06M1, 0));
    EXPECT_EQ(encodeStoredEffect(KeyboardEffect::wave(period(5000), brightness(100)), entry(fx::kBeforeC04M1, 1)).value(),
              entry(fx::kAfterC06M1, 1));
    // E de volta.
    EXPECT_EQ(encodeStoredEffect(KeyboardEffect::staticColor(black), entry(fx::kAfterC06M1, 0)).value(),
              entry(fx::kBeforeC04M1, 0));
    EXPECT_EQ(encodeStoredEffect(KeyboardEffect::ripple(black, speed(20)), entry(fx::kAfterC06M1, 1)).value(),
              entry(fx::kBeforeC04M1, 1));
    // Respiração ciano do M2.
    EXPECT_EQ(encodeStoredEffect(KeyboardEffect::breathing(RgbColor{0x00, 0xdc, 0xff}, period(3000), brightness(100)),
                                 entry(fx::kBeforeC04M1, 0))
                  .value(),
              entry(fx::kBeforeC04M2, 0));
    EXPECT_EQ(encodeStoredEffect(KeyboardEffect::cycle(period(5000), brightness(50)), entry(fx::kBeforeC04M2, 2)).value(),
              entry(fx::kBeforeC04M3, 2));
}

TEST(StoredEffect, SameKindEditKeepsTheBytesOutsideTheEditedFields)
{
    // Entrada 2 da cópia da ROM: ciclo 2100 ms, brilho 50, com um byte (p4 = 0x88) que o formato conhecido não explica.
    const auto romDimmed = entry(fx::kAfterRestoreM1, 2);
    ASSERT_EQ(romDimmed[5], 0x88);

    const auto edited = encodeStoredEffect(KeyboardEffect::cycle(period(3000), brightness(50)), romDimmed);

    ASSERT_TRUE(edited.has_value());
    auto expected = romDimmed;
    expected[6] = 0x0b;
    expected[7] = 0xb8;
    EXPECT_EQ(*edited, expected);
}

TEST(StoredEffect, RippleEditChangesOnlyTheSpeedByteOfTheOriginalEntry)
{
    const auto edited = encodeStoredEffect(KeyboardEffect::ripple(black, speed(2)), entry(fx::kBeforeC04M1, 1));

    ASSERT_TRUE(edited.has_value());
    EXPECT_EQ(*edited, entry(fx::kFinalM1, 1));
}

TEST(StoredEffect, OffHasNoStoredFormatAndIsRefused)
{
    const auto encoded = encodeStoredEffect(KeyboardEffect::off(), entry(fx::kBeforeC04M1, 0));

    ASSERT_FALSE(encoded.has_value());
    EXPECT_EQ(encoded.error().code, ErrorCode::InvalidArgument);
}

TEST(StoredEffect, EditingARawEntryReplacesItByTheKnownTemplate)
{
    auto raw = entry(fx::kBeforeC04M3, 0);
    raw[0] = 0x05;
    ASSERT_FALSE(decodeStoredEffect(raw).has_value());

    EXPECT_EQ(encodeStoredEffect(KeyboardEffect::cycle(period(5000), brightness(100)), raw).value(),
              entry(fx::kBeforeC04M3, 0));
}

TEST(StoredEffect, DimmedVariantFollowsThePrimaryLikeTheGHubWroteIt)
{
    // Static/Ripple -> Cycle/Wave: as variantes ganham brilho 50.
    EXPECT_EQ(deriveDimmedVariant(entry(fx::kBeforeC04M1, 0), entry(fx::kBeforeC04M1, 2), entry(fx::kAfterC06M1, 0)),
              entry(fx::kAfterC06M1, 2));
    EXPECT_EQ(deriveDimmedVariant(entry(fx::kBeforeC04M1, 1), entry(fx::kBeforeC04M1, 3), entry(fx::kAfterC06M1, 1)),
              entry(fx::kAfterC06M1, 3));
    // Cycle/Wave -> Static/Ripple: sem campo de brilho, a variante é igual à primária.
    EXPECT_EQ(deriveDimmedVariant(entry(fx::kAfterC06M1, 0), entry(fx::kAfterC06M1, 2), entry(fx::kBeforeC04M1, 0)),
              entry(fx::kBeforeC04M1, 2));
    // Mesmo tipo: a variante muda o campo editado e mantém o próprio brilho (Ripple 20 -> 2).
    EXPECT_EQ(deriveDimmedVariant(entry(fx::kBeforeC04M1, 1), entry(fx::kBeforeC04M1, 3), entry(fx::kFinalM1, 1)),
              entry(fx::kFinalM1, 3));
}

TEST(StoredEffect, SameKindVariantKeepsItsOwnBrightnessWhenOnlyThePeriodChanges)
{
    const auto newPrimary = encodeStoredEffect(KeyboardEffect::cycle(period(3000), brightness(100)),
                                               entry(fx::kBeforeC04M3, 0))
                                .value();

    const auto variant = deriveDimmedVariant(entry(fx::kBeforeC04M3, 0), entry(fx::kBeforeC04M3, 2), newPrimary);

    const auto decoded = decodeStoredEffect(variant);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, KeyboardEffect::cycle(period(3000), brightness(50)));
}

TEST(StoredEffect, UnchangedPrimaryLeavesTheVariantUntouched)
{
    EXPECT_EQ(deriveDimmedVariant(entry(fx::kAfterRestoreM1, 0), entry(fx::kAfterRestoreM1, 2),
                                  entry(fx::kAfterRestoreM1, 0)),
              entry(fx::kAfterRestoreM1, 2));
}
