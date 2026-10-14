#include "modules/pointer/domain/G703ProfileSettings.hpp"
#include "G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace ghub::pointer {
namespace {

using shared_kernel::ErrorCode;
using Dpis = std::vector<std::uint16_t>;

std::vector<std::uint8_t> raw0001()
{
    return {fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
}

OnboardSector sector0001()
{
    return *OnboardSector::fromBytes(raw0001());
}

OnboardSector sector0002()
{
    return *OnboardSector::fromBytes({fixtures::kSector0002.begin(), fixtures::kSector0002.end()});
}

TEST(G703ProfileSettingsReadTest, ReadsRealSector0001)
{
    const auto settings = G703ProfileSettings::read(sector0001());
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->reportRateHz(), 1000U);
    EXPECT_EQ(settings->dpiStages(), (Dpis{7050}));
    EXPECT_EQ(settings->defaultStage(), 0U);
}

TEST(G703ProfileSettingsReadTest, ReadsRealSector0002)
{
    const auto settings = G703ProfileSettings::read(sector0002());
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->reportRateHz(), 1000U);
    EXPECT_EQ(settings->dpiStages(), (Dpis{400, 800, 1600, 3200}));
    EXPECT_EQ(settings->defaultStage(), 1U);
}

TEST(G703ProfileSettingsReadTest, RejectsCorruptedCrc)
{
    auto raw = raw0001();
    raw[3] ^= 0x01;
    const auto settings = G703ProfileSettings::read(*OnboardSector::fromBytes(std::move(raw)));
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

TEST(G703ProfileSettingsReadTest, RejectsInvalidReportRateInterval)
{
    auto raw = raw0001();
    raw[0] = 3;
    const auto settings = G703ProfileSettings::read(OnboardSector::fromBytes(std::move(raw))->withCrc());
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

TEST(G703ProfileSettingsReadTest, ReadsEveryValidReportRateInterval)
{
    const std::pair<std::uint8_t, std::uint32_t> cases[] = {{1, 1000}, {2, 500}, {4, 250}, {8, 125}};
    for (const auto& [ms, hz] : cases) {
        auto raw = raw0001();
        raw[0] = ms;
        const auto settings = G703ProfileSettings::read(OnboardSector::fromBytes(std::move(raw))->withCrc());
        ASSERT_TRUE(settings.has_value()) << "ms " << int{ms};
        EXPECT_EQ(settings->reportRateHz(), hz);
    }
}

TEST(G703ProfileSettingsReadTest, RejectsSectorWithoutDpiStage)
{
    auto raw = raw0001();
    raw[3] = 0;
    raw[4] = 0;
    const auto settings = G703ProfileSettings::read(OnboardSector::fromBytes(std::move(raw))->withCrc());
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

TEST(G703ProfileSettingsReadTest, StopsAtFirstZeroDpi)
{
    auto raw = raw0001();
    raw[3] = 0x90; // 400 = 0x0190
    raw[4] = 0x01;
    raw[5] = 0;
    raw[6] = 0;
    raw[7] = 0x20; // 800 depois do zero: ignorado
    raw[8] = 0x03;
    const auto settings = G703ProfileSettings::read(OnboardSector::fromBytes(std::move(raw))->withCrc());
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->dpiStages(), (Dpis{400}));
}

TEST(G703ProfileSettingsReadTest, RejectsDefaultStageBeyondListedStages)
{
    auto raw = raw0001();
    raw[1] = 1; // só há 1 estágio (índice 0)
    const auto settings = G703ProfileSettings::read(OnboardSector::fromBytes(std::move(raw))->withCrc());
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::MalformedData);
}

TEST(G703ProfileSettingsMakeTest, AcceptsValidSettings)
{
    const auto settings = G703ProfileSettings::make(500, {400, 800, 1600}, 2);
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->reportRateHz(), 500U);
    EXPECT_EQ(settings->dpiStages(), (Dpis{400, 800, 1600}));
    EXPECT_EQ(settings->defaultStage(), 2U);
}

TEST(G703ProfileSettingsMakeTest, AcceptsEveryReportRate)
{
    for (const std::uint32_t hz : {125U, 250U, 500U, 1000U}) {
        EXPECT_TRUE(G703ProfileSettings::make(hz, {800}, 0).has_value()) << hz;
    }
}

TEST(G703ProfileSettingsMakeTest, RejectsUnsupportedReportRate)
{
    const auto settings = G703ProfileSettings::make(300, {800}, 0);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, AcceptsDpiBoundaries)
{
    EXPECT_TRUE(G703ProfileSettings::make(1000, {100}, 0).has_value());
    EXPECT_TRUE(G703ProfileSettings::make(1000, {25500}, 0).has_value());
    EXPECT_TRUE(G703ProfileSettings::make(1000, {50 * 2}, 0).has_value());
}

TEST(G703ProfileSettingsMakeTest, RejectsDpiOutsideRange)
{
    for (const std::uint16_t dpi : {std::uint16_t{50}, std::uint16_t{25550}, std::uint16_t{0}}) {
        const auto settings = G703ProfileSettings::make(1000, {dpi}, 0);
        ASSERT_FALSE(settings.has_value()) << dpi;
        EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
    }
}

TEST(G703ProfileSettingsMakeTest, RejectsDpiNotMultipleOf50)
{
    const auto settings = G703ProfileSettings::make(1000, {800, 7025}, 0);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, RejectsNoStages)
{
    const auto settings = G703ProfileSettings::make(1000, {}, 0);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, AcceptsFiveAndRejectsSixStages)
{
    EXPECT_TRUE(G703ProfileSettings::make(1000, {100, 200, 300, 400, 500}, 4).has_value());
    const auto six = G703ProfileSettings::make(1000, {100, 200, 300, 400, 500, 600}, 0);
    ASSERT_FALSE(six.has_value());
    EXPECT_EQ(six.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsMakeTest, RejectsDefaultStageEqualToStageCount)
{
    const auto settings = G703ProfileSettings::make(1000, {400, 800}, 2);
    ASSERT_FALSE(settings.has_value());
    EXPECT_EQ(settings.error().code, ErrorCode::InvalidArgument);
}

TEST(G703ProfileSettingsApplyTest, RoundTripOfRealSectorsIsByteExact)
{
    for (const auto& original : {sector0001(), sector0002()}) {
        const auto settings = G703ProfileSettings::read(original);
        ASSERT_TRUE(settings.has_value());
        const auto applied = settings->applyTo(original);
        ASSERT_TRUE(applied.has_value());
        EXPECT_EQ(applied->bytes(), original.bytes());
    }
}

TEST(G703ProfileSettingsApplyTest, ChangesOnlySettingsBytesAndCrc)
{
    const auto original = sector0001();
    const auto settings = G703ProfileSettings::make(1000, {7050, 1600}, 0);
    ASSERT_TRUE(settings.has_value());
    const auto applied = settings->applyTo(original);
    ASSERT_TRUE(applied.has_value());
    ASSERT_EQ(applied->size(), original.size());
    EXPECT_TRUE(applied->hasValidCrc());
    for (std::size_t i = 0; i < original.size(); ++i) {
        const bool editable = i == 0 || i == 1 || (i >= 3 && i <= 12) || i >= original.size() - 2;
        if (!editable) {
            EXPECT_EQ(applied->bytes()[i], original.bytes()[i]) << "byte " << i;
        }
    }
    const auto reread = G703ProfileSettings::read(*applied);
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->dpiStages(), (Dpis{7050, 1600}));
    EXPECT_EQ(reread->reportRateHz(), 1000U);
    EXPECT_EQ(reread->defaultStage(), 0U);
}

TEST(G703ProfileSettingsApplyTest, WritesRateDefaultStageAndZeroesUnusedStages)
{
    const auto settings = G703ProfileSettings::make(250, {400, 800}, 1);
    ASSERT_TRUE(settings.has_value());
    const auto applied = settings->applyTo(sector0002()); // original com 4 estágios
    ASSERT_TRUE(applied.has_value());
    const auto& b = applied->bytes();
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

OnboardSector sector0002WithShiftIndex(std::uint8_t shift)
{
    std::vector<std::uint8_t> raw{fixtures::kSector0002.begin(), fixtures::kSector0002.end()};
    raw[2] = shift;
    return OnboardSector::fromBytes(std::move(raw))->withCrc();
}

TEST(G703ProfileSettingsApplyTest, RejectsShiftIndexThatWouldPointToARemovedStage)
{
    const auto original = sector0002WithShiftIndex(2);
    const auto settings = G703ProfileSettings::make(1000, {400}, 0);
    ASSERT_TRUE(settings.has_value());
    const auto applied = settings->applyTo(original);
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(applied.error().detail, "o DPI do shift aponta para um estágio removido");
}

TEST(G703ProfileSettingsApplyTest, KeepsShiftIndexWhenItStillPointsToAnExistingStage)
{
    const auto original = sector0002WithShiftIndex(2);
    const auto settings = G703ProfileSettings::make(1000, {400, 800, 1600}, 0);
    ASSERT_TRUE(settings.has_value());
    const auto applied = settings->applyTo(original);
    ASSERT_TRUE(applied.has_value());
    EXPECT_EQ(applied->bytes()[2], original.bytes()[2]);
}

TEST(G703ProfileSettingsApplyTest, RejectsOriginalWithInvalidCrc)
{
    auto raw = raw0001();
    raw[20] ^= 0x01;
    const auto settings = G703ProfileSettings::make(1000, {800}, 0);
    ASSERT_TRUE(settings.has_value());
    const auto applied = settings->applyTo(*OnboardSector::fromBytes(std::move(raw)));
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::MalformedData);
}

} // namespace
} // namespace ghub::pointer
