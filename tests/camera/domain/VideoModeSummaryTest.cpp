#include "modules/camera/domain/VideoModeSummary.hpp"

#include <gtest/gtest.h>

#include <vector>

using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::summarizeModes;
using ghub::camera::VideoMode;
using ghub::camera::VideoModeSummary;

namespace {

PixelFormat format(std::string_view fourcc)
{
    return PixelFormat::create(fourcc).value();
}

FrameRate fps(std::uint32_t frames, std::uint32_t seconds = 1)
{
    return FrameRate::create(frames, seconds).value();
}

VideoMode mode(std::string_view fourcc, std::uint32_t width, std::uint32_t height, std::vector<FrameRate> rates)
{
    return VideoMode::create(format(fourcc), width, height, std::move(rates)).value();
}

std::vector<FrameRate> allRates()
{
    return {fps(30), fps(24), fps(20), fps(15), fps(10), fps(15, 2), fps(5)};
}

// Subconjunto de modos da tabela lida da Brio 100 em 2026-10-13, na ordem real de enumeração (YUYV antes de MJPG).
std::vector<VideoMode> brio100Modes()
{
    return {
        mode("YUYV", 640, 480, allRates()), mode("YUYV", 1280, 720, {fps(5)}),   mode("YUYV", 1920, 1080, {fps(5)}),
        mode("MJPG", 640, 480, allRates()), mode("MJPG", 1280, 720, allRates()), mode("MJPG", 1920, 1080, allRates()),
    };
}

} // namespace

TEST(VideoModeSummary, PicksLargestResolution_AndItsHighestRate_PerFormat)
{
    const auto modes = brio100Modes();

    const auto summary = summarizeModes(modes);

    ASSERT_EQ(summary.size(), 2U);
    EXPECT_EQ(summary[0],
              (VideoModeSummary{.format = format("YUYV"), .width = 1920, .height = 1080, .maxFrameRate = fps(5)}));
    EXPECT_EQ(summary[1],
              (VideoModeSummary{.format = format("MJPG"), .width = 1920, .height = 1080, .maxFrameRate = fps(30)}));
}

TEST(VideoModeSummary, KeepsOrderOfFirstAppearance)
{
    const std::vector modes{
        mode("YUYV", 640, 480, {fps(30)}),
        mode("MJPG", 640, 480, {fps(30)}),
        mode("YUYV", 320, 240, {fps(30)}),
    };

    const auto summary = summarizeModes(modes);

    ASSERT_EQ(summary.size(), 2U);
    EXPECT_EQ(summary[0].format, format("YUYV"));
    EXPECT_EQ(summary[1].format, format("MJPG"));
}

TEST(VideoModeSummary, PrefersWiderMode_WhenAreasTie)
{
    const std::vector modes{mode("MJPG", 1000, 600, {fps(30)}), mode("MJPG", 1200, 500, {fps(10)})};

    const auto summary = summarizeModes(modes);

    ASSERT_EQ(summary.size(), 1U);
    EXPECT_EQ(summary[0].width, 1200U);
    EXPECT_EQ(summary[0].height, 500U);
    EXPECT_EQ(summary[0].maxFrameRate, fps(10));
}

TEST(VideoModeSummary, UsesHighestRate_WhenSameResolutionAppearsTwice)
{
    const std::vector modes{
        mode("MJPG", 640, 480, {fps(15)}),
        mode("MJPG", 640, 480, {fps(30), fps(5)}),
        mode("MJPG", 640, 480, {fps(20)}),
    };

    const auto summary = summarizeModes(modes);

    ASSERT_EQ(summary.size(), 1U);
    EXPECT_EQ(summary[0].maxFrameRate, fps(30));
}

TEST(VideoModeSummary, AvoidsOverflow_InAreaComparison)
{
    const std::vector modes{mode("MJPG", 4000000000U, 2, {fps(1)}), mode("MJPG", 3, 3, {fps(30)})};

    const auto summary = summarizeModes(modes);

    ASSERT_EQ(summary.size(), 1U);
    EXPECT_EQ(summary[0].width, 4000000000U);
}

TEST(VideoModeSummary, ReturnsEmpty_ForNoModes)
{
    EXPECT_TRUE(summarizeModes({}).empty());
}
