#include "modules/pointer/domain/PointerSensorStatus.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace ghub::pointer {
namespace {

using Bytes = std::vector<std::uint8_t>;

// Bytes da captura C-00 do G703 (INC-015 parte 2): 0x2201 fn1 -> 00 0064 e032 6400.
TEST(DpiListTest, ParsesTheCapturedRangeAsOneSegment)
{
    const Bytes response{0x00, 0x00, 0x64, 0xE0, 0x32, 0x64, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    const auto list = DpiList::parse(response);

    ASSERT_TRUE(list.valid());
    ASSERT_EQ(list.segments().size(), 1U);
    EXPECT_EQ(list.segments()[0].minimum, 100);
    EXPECT_EQ(list.segments()[0].maximum, 25600);
    EXPECT_EQ(list.segments()[0].step, 50);
}

TEST(DpiListTest, KeepsLooseValuesAsSingleSegments)
{
    const auto list = DpiList::parse(Bytes{0x00, 0x03, 0x20, 0x06, 0x40, 0x0C, 0x80, 0x00, 0x00});

    ASSERT_TRUE(list.valid());
    ASSERT_EQ(list.segments().size(), 3U);
    EXPECT_EQ(list.segments()[0], (DpiSegment{.minimum = 800, .maximum = 800, .step = 0}));
    EXPECT_EQ(list.segments()[2], (DpiSegment{.minimum = 3200, .maximum = 3200, .step = 0}));
}

TEST(DpiListTest, MixesLooseValuesAndRanges)
{
    // 400, depois 800..1600 passo 100, depois 3200
    const auto list =
        DpiList::parse(Bytes{0x00, 0x01, 0x90, 0x03, 0x20, 0xE0, 0x64, 0x06, 0x40, 0x0C, 0x80, 0x00, 0x00});

    ASSERT_TRUE(list.valid());
    ASSERT_EQ(list.segments().size(), 3U);
    EXPECT_EQ(list.segments()[1], (DpiSegment{.minimum = 800, .maximum = 1600, .step = 100}));
}

TEST(DpiListTest, StopsAtTheZeroTerminator)
{
    const auto list = DpiList::parse(Bytes{0x00, 0x03, 0x20, 0x00, 0x00, 0x06, 0x40});

    ASSERT_TRUE(list.valid());
    EXPECT_EQ(list.segments().size(), 1U);
}

TEST(DpiListTest, IsInvalidWithTheRawBytesWhenTheStepIsZero)
{
    const Bytes response{0x00, 0x00, 0x64, 0xE0, 0x00, 0x64, 0x00};

    const auto list = DpiList::parse(response);

    EXPECT_FALSE(list.valid());
    EXPECT_EQ(list.rawBytes(), response);
    EXPECT_TRUE(list.segments().empty());
}

TEST(DpiListTest, IsInvalidWhenTheRangeIsInverted)
{
    EXPECT_FALSE(DpiList::parse(Bytes{0x00, 0x64, 0x00, 0xE0, 0x32, 0x00, 0x64, 0x00, 0x00}).valid());
}

TEST(DpiListTest, IsInvalidWhenAStepHasNoNeighbours)
{
    EXPECT_FALSE(DpiList::parse(Bytes{0x00, 0xE0, 0x32, 0x00, 0x64, 0x00, 0x00}).valid()) << "passo sem valor anterior";
    EXPECT_FALSE(DpiList::parse(Bytes{0x00, 0x00, 0x64, 0xE0, 0x32, 0x00, 0x00}).valid()) << "passo sem valor seguinte";
    EXPECT_FALSE(DpiList::parse(Bytes{0x00, 0x00, 0x64, 0xE0, 0x32, 0xE0, 0x32, 0x64, 0x00}).valid())
        << "dois passos seguidos";
}

TEST(DpiListTest, IsInvalidWhenTwoStepsShareTheSameValue)
{
    // 100 passo 50 25600 passo 100 30000: formato nunca capturado; não pode sobrescrever a primeira faixa em silêncio.
    const DpiList list =
        DpiList::parse(Bytes{0x00, 0x00, 0x64, 0xE0, 0x32, 0x64, 0x00, 0xE0, 0x64, 0x75, 0x30, 0x00, 0x00});
    EXPECT_FALSE(list.valid());
    EXPECT_TRUE(list.segments().empty());
}

TEST(DpiListTest, IsInvalidForAnEmptyResponseOrAnotherSensor)
{
    EXPECT_FALSE(DpiList::parse(Bytes{}).valid());
    EXPECT_FALSE(DpiList::parse(Bytes{0x01, 0x00, 0x64, 0x00, 0x00}).valid());
}

TEST(DpiListTest, AcceptsAtMostSixteenValues)
{
    Bytes sixteen{0x00};
    for (unsigned i = 1; i <= 16; ++i) {
        sixteen.push_back(0x00);
        sixteen.push_back(static_cast<std::uint8_t>(i * 50));
    }
    EXPECT_TRUE(DpiList::parse(sixteen).valid());
    EXPECT_EQ(DpiList::parse(sixteen).segments().size(), 16U);

    Bytes seventeen = sixteen;
    seventeen.push_back(0x03);
    seventeen.push_back(0x20);
    const auto list = DpiList::parse(seventeen);
    EXPECT_FALSE(list.valid());
    EXPECT_EQ(list.rawBytes(), seventeen);
}

TEST(DpiSettingTest, ParsesTheCapturedCurrentAndDefaultDpi)
{
    const auto setting = parseDpiSetting(Bytes{0x00, 0x1B, 0x8A, 0x03, 0x20});

    ASSERT_TRUE(setting.has_value());
    EXPECT_EQ(setting->current, 7050);
    EXPECT_EQ(setting->defaultDpi, 800);
}

TEST(DpiSettingTest, RejectsShortResponsesAndAnotherSensor)
{
    EXPECT_FALSE(parseDpiSetting(Bytes{0x00, 0x1B, 0x8A, 0x03}).has_value());
    EXPECT_FALSE(parseDpiSetting(Bytes{0x01, 0x1B, 0x8A, 0x03, 0x20}).has_value());
    EXPECT_EQ(parseDpiSetting(Bytes{}).error().code, shared_kernel::ErrorCode::MalformedData);
}

TEST(ReportRateTest, ParsesTheCapturedBitmapAndPeriod)
{
    const auto support = parseReportRateSupport(Bytes{0x8B});
    const auto period = parseReportPeriodMs(Bytes{0x01});

    ASSERT_TRUE(support.has_value());
    EXPECT_EQ(support->periodsMs(), (std::vector<std::uint8_t>{1, 2, 4, 8}));
    EXPECT_TRUE(support->supports(2));
    EXPECT_FALSE(support->supports(3));
    ASSERT_TRUE(period.has_value());
    EXPECT_EQ(*period, 1);
}

TEST(ReportRateTest, RejectsEmptyBitmapZeroPeriodAndEmptyResponses)
{
    EXPECT_FALSE(parseReportRateSupport(Bytes{0x00}).has_value());
    EXPECT_FALSE(parseReportRateSupport(Bytes{}).has_value());
    EXPECT_FALSE(parseReportPeriodMs(Bytes{0x00}).has_value());
    EXPECT_FALSE(parseReportPeriodMs(Bytes{}).has_value());
}

} // namespace
} // namespace ghub::pointer
