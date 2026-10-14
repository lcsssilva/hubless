#include "modules/camera/domain/PreviewMode.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::PreviewMode;
using ghub::camera::selectPreviewMode;
using ghub::camera::VideoMode;

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

PreviewMode preview(std::string_view fourcc, std::uint32_t width, std::uint32_t height, FrameRate rate)
{
    return {.format = format(fourcc), .width = width, .height = height, .frameRate = rate};
}

} // namespace

TEST(PreviewMode, SelectsLargestAreaWithin720p)
{
    const std::vector<VideoMode> modes{
        mode("MJPG", 640, 480, {fps(30)}),
        mode("MJPG", 1280, 720, {fps(30)}),
        mode("MJPG", 320, 240, {fps(30)}),
    };

    EXPECT_EQ(selectPreviewMode(modes), preview("MJPG", 1280, 720, fps(30)));
}

TEST(PreviewMode, IgnoresModesAbove1280x720)
{
    const std::vector<VideoMode> modes{
        mode("MJPG", 1920, 1080, {fps(30)}),
        mode("MJPG", 1280, 960, {fps(30)}), // altura acima
        mode("MJPG", 1600, 600, {fps(30)}), // largura acima
        mode("MJPG", 640, 480, {fps(30)}),
    };

    EXPECT_EQ(selectPreviewMode(modes), preview("MJPG", 640, 480, fps(30)));
}

TEST(PreviewMode, IgnoresModesBelow15Fps)
{
    const std::vector<VideoMode> modes{
        mode("MJPG", 1280, 720, {fps(15, 2), fps(5)}), // 7,5 fps: fora
        mode("MJPG", 960, 540, {fps(14), fps(10)}),    // 14 fps: fora
        mode("MJPG", 800, 600, {fps(15), fps(15, 2)}), // 15 fps: no limite, entra
        mode("MJPG", 640, 480, {fps(30)}),
    };

    EXPECT_EQ(selectPreviewMode(modes), preview("MJPG", 800, 600, fps(15)));
}

TEST(PreviewMode, IgnoresUnknownPixelFormats)
{
    const std::vector<VideoMode> modes{
        mode("H264", 1280, 720, {fps(30)}),
        mode("NV12", 1280, 720, {fps(30)}),
        mode("YUYV", 640, 480, {fps(30)}),
    };

    EXPECT_EQ(selectPreviewMode(modes), preview("YUYV", 640, 480, fps(30)));
}

TEST(PreviewMode, BreaksAreaTieByFrameRate)
{
    const std::vector<VideoMode> modes{
        mode("MJPG", 640, 480, {fps(20)}),
        mode("MJPG", 640, 480, {fps(30), fps(20)}),
        mode("MJPG", 640, 480, {fps(25)}),
    };

    EXPECT_EQ(selectPreviewMode(modes), preview("MJPG", 640, 480, fps(30)));
}

TEST(PreviewMode, PrefersYuyvOverMjpg_WhenAreaAndRateTie)
{
    const std::vector<VideoMode> mjpgFirst{
        mode("MJPG", 640, 480, {fps(30)}),
        mode("YUYV", 640, 480, {fps(30)}),
    };
    const std::vector<VideoMode> yuyvFirst{
        mode("YUYV", 640, 480, {fps(30)}),
        mode("MJPG", 640, 480, {fps(30)}),
    };

    EXPECT_EQ(selectPreviewMode(mjpgFirst), preview("YUYV", 640, 480, fps(30)));
    EXPECT_EQ(selectPreviewMode(yuyvFirst), preview("YUYV", 640, 480, fps(30)));
}

TEST(PreviewMode, KeepsFirst_WhenFullyTied)
{
    // Mesma área (640x480 e 480x640), mesmo formato e mesma taxa: só a ordem de entrada decide.
    const std::vector<VideoMode> mjpg{
        mode("MJPG", 640, 480, {fps(30)}),
        mode("MJPG", 480, 640, {fps(30)}),
    };
    const std::vector<VideoMode> yuyv{
        mode("YUYV", 640, 480, {fps(30)}),
        mode("YUYV", 480, 640, {fps(30)}),
    };

    EXPECT_EQ(selectPreviewMode(mjpg), preview("MJPG", 640, 480, fps(30)));
    EXPECT_EQ(selectPreviewMode(yuyv), preview("YUYV", 640, 480, fps(30)));
}

TEST(PreviewMode, ReturnsEmpty_WhenNoCandidate)
{
    EXPECT_EQ(selectPreviewMode({}), std::nullopt);

    const std::vector<VideoMode> onlyIneligible{
        mode("H264", 640, 480, {fps(30)}),
        mode("MJPG", 1920, 1080, {fps(30)}),
        mode("YUYV", 1280, 720, {fps(5)}),
    };
    EXPECT_EQ(selectPreviewMode(onlyIneligible), std::nullopt);
}

TEST(PreviewMode, UsesMaxFrameRateOfSelectedMode)
{
    const std::vector<VideoMode> modes{mode("YUYV", 1280, 720, {fps(24), fps(15), fps(15, 2)})};

    const auto selected = selectPreviewMode(modes);

    if (!selected.has_value()) {
        FAIL() << "nenhum modo selecionado";
    }
    EXPECT_EQ(selected->frameRate, fps(24));
}

TEST(PreviewMode, SelectsMjpg720p30_ForBrio100Table)
{
    const std::vector<FrameRate> all{fps(30), fps(24), fps(20), fps(15), fps(10), fps(15, 2), fps(5)};
    const std::vector<VideoMode> modes{
        mode("YUYV", 640, 480, all),         mode("YUYV", 320, 240, all),        mode("YUYV", 160, 120, all),
        mode("YUYV", 1280, 720, {fps(5)}),   mode("YUYV", 1920, 1080, {fps(5)}), mode("MJPG", 160, 120, {fps(30)}),
        mode("MJPG", 640, 480, {fps(30)}),   mode("MJPG", 800, 600, {fps(30)}),  mode("MJPG", 1280, 720, {fps(30)}),
        mode("MJPG", 1920, 1080, {fps(30)}),
    };

    EXPECT_EQ(selectPreviewMode(modes), preview("MJPG", 1280, 720, fps(30)));
}
