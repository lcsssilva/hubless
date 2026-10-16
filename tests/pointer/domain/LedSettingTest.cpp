#include "modules/pointer/domain/LedSetting.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace ghub::pointer {
namespace {

using shared_kernel::ErrorCode;
using Entry = std::array<std::uint8_t, LedSetting::kEntrySize>;

// Entradas de LED reais: G703 setor 0x0001 (0xD0), G703 setor 0x0002 (0xD0 e 0xFB) e G915 setor 0x0001 (0xD0).
Entry entryAt(const std::array<std::uint8_t, 255>& sector, std::size_t offset)
{
    Entry entry{};
    std::copy_n(sector.begin() + static_cast<std::ptrdiff_t>(offset), entry.size(), entry.begin());
    return entry;
}

TEST(LedSettingDecodeTest, AllZeroEntryOfG703Sector0001IsOff)
{
    const auto led = LedSetting::decode(entryAt(fixtures::kSector0001, 0xD0));
    EXPECT_EQ(led.kind(), LedSetting::Kind::Off);
}

TEST(LedSettingDecodeTest, CycleEntryOfG703Sector0002HasPeriod8000AndBrightness100)
{
    const auto led = LedSetting::decode(entryAt(fixtures::kSector0002, 0xD0));
    EXPECT_EQ(led.kind(), LedSetting::Kind::Cycle);
    EXPECT_EQ(led.periodMs(), 8000U);
    EXPECT_EQ(led.brightness(), 100U);
}

TEST(LedSettingDecodeTest, DimmedCycleEntryKeepsItsBrightness50)
{
    const auto led = LedSetting::decode(entryAt(fixtures::kSector0002, 0xD0 + (2 * 11)));
    EXPECT_EQ(led.kind(), LedSetting::Kind::Cycle);
    EXPECT_EQ(led.brightness(), 50U);
}

// G915 setor 0x0001: Static preto (`01 000000 02 ...`).
TEST(LedSettingDecodeTest, FixedEntryHasRgbAndTheTrailingTwo)
{
    const Entry g915Static{0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(LedSetting::decode(g915Static).kind(), LedSetting::Kind::Fixed);
    const Entry cyan{0x01, 0x00, 0xff, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const auto led = LedSetting::decode(cyan);
    EXPECT_EQ(led.kind(), LedSetting::Kind::Fixed);
    EXPECT_EQ(led.red(), 0U);
    EXPECT_EQ(led.green(), 255U);
    EXPECT_EQ(led.blue(), 255U);
}

TEST(LedSettingDecodeTest, UnknownFormatsStayRawAndRoundTripByteForByte)
{
    // Ripple do G915 (0x0b), respiração do G915 (0x0a), e entradas quase conhecidas com sobra diferente de zero.
    const std::array<Entry, 5> samples{{
        {0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00},
        {0x0a, 0x00, 0xdc, 0xff, 0x0b, 0xb8, 0x01, 0x64, 0x00, 0x00, 0x00},
        {0x01, 0x00, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // fixo sem o `02`
        {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x40, 0x64, 0x00, 0x01}, // ciclo com sobra
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, // desligado com sobra
    }};
    for (const auto& sample : samples) {
        const auto led = LedSetting::decode(sample);
        EXPECT_EQ(led.kind(), LedSetting::Kind::Raw);
        EXPECT_EQ(led.encode(), sample);
    }
}

TEST(LedSettingDecodeTest, CycleWithBrightnessAbove100OrZeroPeriodStaysRaw)
{
    const Entry bright{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x40, 0x65, 0x00, 0x00};
    const Entry noPeriod{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00};
    EXPECT_EQ(LedSetting::decode(bright).kind(), LedSetting::Kind::Raw);
    EXPECT_EQ(LedSetting::decode(noPeriod).kind(), LedSetting::Kind::Raw);
}

TEST(LedSettingDecodeTest, EveryRealEntryRoundTripsByteForByte)
{
    for (const auto* sector : {&fixtures::kSector0001, &fixtures::kSector0002}) {
        for (std::size_t i = 0; i < 4; ++i) {
            const auto entry = entryAt(*sector, 0xD0 + (i * 11));
            EXPECT_EQ(LedSetting::decode(entry).encode(), entry) << i;
        }
    }
}

TEST(LedSettingEncodeTest, OffFixedAndCycleUseTheObservedFormats)
{
    EXPECT_EQ(LedSetting::off().encode(), (Entry{}));
    EXPECT_EQ(LedSetting::fixed(0x00, 0xff, 0xff).encode(),
              (Entry{0x01, 0x00, 0xff, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    EXPECT_EQ(LedSetting::cycle(8000, 100)->encode(),
              (Entry{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x40, 0x64, 0x00, 0x00}));
}

TEST(LedSettingEncodeTest, CycleRejectsZeroPeriodAndBrightnessAbove100)
{
    const auto noPeriod = LedSetting::cycle(0, 100);
    ASSERT_FALSE(noPeriod.has_value());
    EXPECT_EQ(noPeriod.error().code, ErrorCode::InvalidArgument);
    const auto tooBright = LedSetting::cycle(5000, 101);
    ASSERT_FALSE(tooBright.has_value());
    EXPECT_EQ(tooBright.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(LedSetting::cycle(65535, 0).has_value());
    EXPECT_FALSE(LedSetting::cycle(65536, 50).has_value());
}

TEST(LedSettingEqualityTest, ComparesKindAndContent)
{
    EXPECT_EQ(LedSetting::off(), LedSetting::decode(Entry{}));
    EXPECT_NE(LedSetting::fixed(1, 2, 3), LedSetting::fixed(1, 2, 4));
    EXPECT_NE(LedSetting::off(), LedSetting::fixed(0, 0, 0));
}

} // namespace
} // namespace ghub::pointer
