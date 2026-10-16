#include "modules/pointer/domain/G703ProfileSettings.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace ghub::pointer {
namespace {

using shared_kernel::ErrorCode;
using Dpis = std::vector<std::uint16_t>;
using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kLedOffset = 0xD0;
constexpr std::size_t kLedEntrySize = 11;

Bytes sector0001()
{
    return {fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
}

Bytes sector0002()
{
    return {fixtures::kSector0002.begin(), fixtures::kSector0002.end()};
}

// Atalho dos testes: sem shift de DPI e com os dois LEDs desligados.
shared_kernel::Result<G703ProfileSettings> plain(std::uint32_t hz, Dpis stages, std::uint32_t defaultStage)
{
    return G703ProfileSettings::make(hz, std::move(stages), defaultStage, std::nullopt, LedSetting::off(),
                                     LedSetting::off());
}

// Índices dos bytes que um perfil de G703 pode alterar: taxa, padrão, shift, 5 DPIs e as duas entradas de LED.
bool isEditableIndex(std::size_t i)
{
    return i <= 12 || (i >= kLedOffset && i < kLedOffset + (2 * kLedEntrySize));
}

// --- leitura ---

TEST(G703ProfileSettingsReadTest, ReadsRealSector0001)
{
    const auto settings = G703ProfileSettings::read(sector0001());
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->reportRateHz(), 1000U);
    EXPECT_EQ(settings->dpiStages(), (Dpis{7050}));
    EXPECT_EQ(settings->defaultStage(), 0U);
    EXPECT_EQ(settings->dpiShiftStage(), std::optional<std::uint32_t>{0});
    EXPECT_EQ(settings->primaryLed(), LedSetting::off());
    EXPECT_EQ(settings->logoLed(), LedSetting::off());
}

TEST(G703ProfileSettingsReadTest, ReadsRealSector0002)
{
    const auto settings = G703ProfileSettings::read(sector0002());
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->reportRateHz(), 1000U);
    EXPECT_EQ(settings->dpiStages(), (Dpis{400, 800, 1600, 3200}));
    EXPECT_EQ(settings->defaultStage(), 1U);
    EXPECT_EQ(settings->dpiShiftStage(), std::optional<std::uint32_t>{0});
    EXPECT_EQ(settings->primaryLed(), *LedSetting::cycle(8000, 100));
    EXPECT_EQ(settings->logoLed(), *LedSetting::cycle(8000, 100));
}

TEST(G703ProfileSettingsReadTest, RejectsInvalidReportRateInterval)
{
    auto raw = sector0001();
    raw[0] = 3; // 3 ms não é uma taxa do G703
    const auto settings = G703ProfileSettings::read(raw);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

TEST(G703ProfileSettingsReadTest, ReadsEveryValidReportRateInterval)
{
    for (const auto& [interval, hz] :
         {std::pair{1, 1000U}, std::pair{2, 500U}, std::pair{4, 250U}, std::pair{8, 125U}}) {
        auto raw = sector0001();
        raw[0] = static_cast<std::uint8_t>(interval);
        const auto settings = G703ProfileSettings::read(raw);
        ASSERT_TRUE(settings.has_value()) << interval;
        EXPECT_EQ(settings->reportRateHz(), hz) << interval;
    }
}

TEST(G703ProfileSettingsReadTest, RejectsSectorWithoutDpiStage)
{
    auto raw = sector0001();
    raw[3] = 0;
    raw[4] = 0;
    const auto settings = G703ProfileSettings::read(raw);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

TEST(G703ProfileSettingsReadTest, StopsAtFirstZeroDpi)
{
    auto raw = sector0002();
    raw[7] = 0; // terceiro estágio zerado: o quarto é ignorado
    raw[8] = 0;
    const auto settings = G703ProfileSettings::read(raw);
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->dpiStages(), (Dpis{400, 800}));
}

TEST(G703ProfileSettingsReadTest, RejectsDefaultStageBeyondListedStages)
{
    auto raw = sector0001();
    raw[1] = 1; // só há 1 estágio (índice 0)
    const auto settings = G703ProfileSettings::read(raw);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

TEST(G703ProfileSettingsReadTest, ShiftIndexOutsideTheStagesReadsAsNoShift)
{
    for (const std::uint8_t byte : {std::uint8_t{1}, std::uint8_t{5}, std::uint8_t{0xff}}) {
        auto raw = sector0001(); // 1 estágio
        raw[2] = byte;
        const auto settings = G703ProfileSettings::read(raw);
        ASSERT_TRUE(settings.has_value());
        EXPECT_EQ(settings->dpiShiftStage(), std::nullopt) << static_cast<int>(byte);
    }
}

TEST(G703ProfileSettingsReadTest, RejectsSectorTooShortForTheLedBlock)
{
    auto raw = sector0001();
    raw.resize(0xD0 + 10);
    const auto settings = G703ProfileSettings::read(raw);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

// --- criação ---

TEST(G703ProfileSettingsMakeTest, AcceptsValidSettings)
{
    const auto settings = G703ProfileSettings::make(500, {400, 800, 1600}, 2, 0, LedSetting::fixed(1, 2, 3),
                                                    *LedSetting::cycle(5000, 50));
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->reportRateHz(), 500U);
    EXPECT_EQ(settings->dpiStages(), (Dpis{400, 800, 1600}));
    EXPECT_EQ(settings->defaultStage(), 2U);
    EXPECT_EQ(settings->dpiShiftStage(), std::optional<std::uint32_t>{0});
    EXPECT_EQ(settings->primaryLed(), LedSetting::fixed(1, 2, 3));
    EXPECT_EQ(settings->logoLed(), *LedSetting::cycle(5000, 50));
}

TEST(G703ProfileSettingsMakeTest, AcceptsEveryReportRate)
{
    for (const std::uint32_t hz : {125U, 250U, 500U, 1000U}) {
        EXPECT_TRUE(plain(hz, {800}, 0).has_value()) << hz;
    }
}

TEST(G703ProfileSettingsMakeTest, RejectsUnsupportedReportRate)
{
    const auto settings = plain(300, {800}, 0);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, AcceptsDpiBoundaries)
{
    EXPECT_TRUE(plain(1000, {100}, 0).has_value());
    EXPECT_TRUE(plain(1000, {25600}, 0).has_value());
    EXPECT_TRUE(plain(1000, {50 * 2}, 0).has_value());
}

TEST(G703ProfileSettingsMakeTest, RejectsDpiOutsideRange)
{
    for (const std::uint16_t dpi : {std::uint16_t{50}, std::uint16_t{25650}, std::uint16_t{0}}) {
        const auto settings = plain(1000, {dpi}, 0);
        ASSERT_FALSE(settings.has_value()) << dpi;
        EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
    }
}

TEST(G703ProfileSettingsMakeTest, RejectsDpiNotMultipleOf50)
{
    const auto settings = plain(1000, {800, 7025}, 0);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, RejectsNoStages)
{
    const auto settings = plain(1000, {}, 0);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, AcceptsFiveAndRejectsSixStages)
{
    EXPECT_TRUE(plain(1000, {100, 200, 300, 400, 500}, 4).has_value());
    const auto six = plain(1000, {100, 200, 300, 400, 500, 600}, 0);
    ASSERT_FALSE(six.has_value());
    EXPECT_EQ(six.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, RejectsDefaultStageEqualToStageCount)
{
    const auto settings = plain(1000, {400, 800}, 2);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, RejectsShiftStageBeyondTheStages)
{
    const auto settings = G703ProfileSettings::make(1000, {400, 800}, 0, 2, LedSetting::off(), LedSetting::off());
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(G703ProfileSettings::make(1000, {400, 800}, 0, 1, LedSetting::off(), LedSetting::off()).has_value());
}

// --- aplicação sobre os bytes do setor ---

TEST(G703ProfileSettingsApplyTest, RoundTripOfRealSectorsIsByteExact)
{
    for (const auto& original : {sector0001(), sector0002()}) {
        const auto settings = G703ProfileSettings::read(original);
        ASSERT_TRUE(settings.has_value());
        const auto applied = settings->applyTo(original);
        ASSERT_TRUE(applied.has_value());
        EXPECT_EQ(*applied, original);
    }
}

TEST(G703ProfileSettingsApplyTest, RoundTripKeepsUnknownLedFormatsAndAnOutOfRangeShiftByteExact)
{
    auto original = sector0002();
    original[2] = 0xff; // shift fora dos estágios
    const Bytes ripple{0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00};
    std::ranges::copy(ripple, original.begin() + kLedOffset);
    const auto settings = G703ProfileSettings::read(original);
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->dpiShiftStage(), std::nullopt);
    EXPECT_EQ(settings->primaryLed().kind(), LedSetting::Kind::Raw);
    const auto applied = settings->applyTo(original);
    ASSERT_TRUE(applied.has_value());
    EXPECT_EQ(*applied, original);
}

TEST(G703ProfileSettingsApplyTest, ChangesOnlySettingsBytesOnEveryRealSector)
{
    const auto changed = G703ProfileSettings::make(500, {800, 1600, 3200}, 2, 1, LedSetting::fixed(10, 20, 30),
                                                   *LedSetting::cycle(6000, 40));
    ASSERT_TRUE(changed.has_value());
    for (const auto& original : {sector0001(), sector0002()}) {
        const auto applied = changed->applyTo(original);
        ASSERT_TRUE(applied.has_value());
        ASSERT_EQ(applied->size(), original.size());
        for (std::size_t i = 0; i < original.size(); ++i) {
            if (!isEditableIndex(i)) {
                EXPECT_EQ((*applied)[i], original[i]) << "byte " << i;
            }
        }
        const auto reread = G703ProfileSettings::read(*applied);
        ASSERT_TRUE(reread.has_value());
        EXPECT_EQ(*reread, *changed);
    }
}

TEST(G703ProfileSettingsApplyTest, WritesRateDefaultStageAndZeroesUnusedStages)
{
    const auto settings = plain(250, {400, 800}, 1);
    ASSERT_TRUE(settings.has_value());
    const auto applied = settings->applyTo(sector0002()); // original com 4 estágios
    ASSERT_TRUE(applied.has_value());
    const auto& b = *applied;
    EXPECT_EQ(b[0], 4);
    EXPECT_EQ(b[1], 1);
    EXPECT_EQ(b[3], 0x90);
    EXPECT_EQ(b[4], 0x01);
    EXPECT_EQ(b[5], 0x20);
    EXPECT_EQ(b[6], 0x03);
    for (std::size_t i = 7; i <= 12; ++i) {
        EXPECT_EQ(b[i], 0) << "byte " << i;
    }
}

TEST(G703ProfileSettingsApplyTest, LedEditTouchesOnlyItsOwnEntry)
{
    const auto original = sector0001();
    const auto read = G703ProfileSettings::read(original);
    ASSERT_TRUE(read.has_value());
    const auto edited =
        G703ProfileSettings::make(read->reportRateHz(), read->dpiStages(), read->defaultStage(), read->dpiShiftStage(),
                                  LedSetting::fixed(0, 255, 255), read->logoLed());
    ASSERT_TRUE(edited.has_value());
    const auto applied = edited->applyTo(original);
    ASSERT_TRUE(applied.has_value());
    for (std::size_t i = 0; i < original.size(); ++i) {
        const bool primaryEntry = i >= kLedOffset && i < kLedOffset + kLedEntrySize;
        if (!primaryEntry) {
            EXPECT_EQ((*applied)[i], original[i]) << "byte " << i;
        }
    }
    const Bytes expected{0x01, 0x00, 0xff, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), applied->begin() + kLedOffset));
}

TEST(G703ProfileSettingsApplyTest, LogoLedUsesTheSecondEntryAndTheDimmedCopiesStayUntouched)
{
    const auto original = sector0002();
    const auto read = G703ProfileSettings::read(original);
    ASSERT_TRUE(read.has_value());
    const auto edited = G703ProfileSettings::make(read->reportRateHz(), read->dpiStages(), read->defaultStage(),
                                                  read->dpiShiftStage(), read->primaryLed(), LedSetting::off());
    ASSERT_TRUE(edited.has_value());
    const auto applied = edited->applyTo(original);
    ASSERT_TRUE(applied.has_value());
    for (std::size_t i = 0; i < original.size(); ++i) {
        const bool logoEntry = i >= kLedOffset + kLedEntrySize && i < kLedOffset + (2 * kLedEntrySize);
        if (logoEntry) {
            EXPECT_EQ((*applied)[i], 0) << "byte " << i;
        } else {
            EXPECT_EQ((*applied)[i], original[i]) << "byte " << i;
        }
    }
}

TEST(G703ProfileSettingsApplyTest, ShiftNoneWritesFfOnlyWhenTheOriginalByteWouldPointToAStage)
{
    const auto none = plain(1000, {400, 800}, 0);
    ASSERT_TRUE(none.has_value());
    auto pointing = sector0002();
    pointing[2] = 1;
    EXPECT_EQ((*none->applyTo(pointing))[2], 0xff);
    auto outOfRange = sector0002();
    outOfRange[2] = 3; // fora dos 2 estágios novos: o byte original é mantido
    EXPECT_EQ((*none->applyTo(outOfRange))[2], 3);
}

TEST(G703ProfileSettingsApplyTest, ShiftStageIsWrittenAsItsIndex)
{
    const auto settings = G703ProfileSettings::make(1000, {400, 800, 1600}, 0, 2, LedSetting::off(), LedSetting::off());
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ((*settings->applyTo(sector0002()))[2], 2);
}

TEST(G703ProfileSettingsApplyTest, KeepsShiftByteWhenTheRequestedShiftIsTheOneAlreadyThere)
{
    auto original = sector0002();
    original[2] = 2;
    const auto settings = G703ProfileSettings::make(1000, {400, 800, 1600}, 0, 2, LedSetting::off(), LedSetting::off());
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ((*settings->applyTo(original))[2], 2);
}

TEST(G703ProfileSettingsApplyTest, RejectsSectorTooShort)
{
    const auto settings = plain(1000, {800}, 0);
    ASSERT_TRUE(settings.has_value());
    auto raw = sector0001();
    raw.resize(0x40);
    const auto applied = settings->applyTo(raw);
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::MalformedData);
}

} // namespace
} // namespace ghub::pointer
