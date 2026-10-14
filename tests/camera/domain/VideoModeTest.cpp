#include "modules/camera/domain/VideoMode.hpp"

#include <gtest/gtest.h>

#include <vector>

using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::VideoMode;
using ghub::shared_kernel::ErrorCode;

namespace {

PixelFormat mjpg()
{
    return PixelFormat::create("MJPG").value();
}

FrameRate fps(std::uint32_t frames, std::uint32_t seconds = 1)
{
    return FrameRate::create(frames, seconds).value();
}

} // namespace

TEST(VideoMode, Accepts_ValidMode)
{
    auto mode = VideoMode::create(mjpg(), 1920, 1080, {fps(30)});

    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(mode->format(), mjpg());
    EXPECT_EQ(mode->width(), 1920U);
    EXPECT_EQ(mode->height(), 1080U);
    EXPECT_EQ(mode->maxFrameRate(), fps(30));
}

TEST(VideoMode, Rejects_ZeroWidth)
{
    EXPECT_EQ(VideoMode::create(mjpg(), 0, 1080, {fps(30)}).error().code, ErrorCode::InvalidArgument);
}

TEST(VideoMode, Rejects_ZeroHeight)
{
    EXPECT_EQ(VideoMode::create(mjpg(), 1920, 0, {fps(30)}).error().code, ErrorCode::InvalidArgument);
}

TEST(VideoMode, Rejects_NoFrameRates)
{
    EXPECT_EQ(VideoMode::create(mjpg(), 1920, 1080, {}).error().code, ErrorCode::InvalidArgument);
}

TEST(VideoMode, SortsRatesDescending)
{
    auto mode = VideoMode::create(mjpg(), 640, 480, {fps(5), fps(30), fps(15, 2), fps(10)});

    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(mode->frameRates(), (std::vector{fps(30), fps(10), fps(15, 2), fps(5)}));
    EXPECT_EQ(mode->maxFrameRate(), fps(30));
}

TEST(VideoMode, RemovesRepeatedRates)
{
    auto mode = VideoMode::create(mjpg(), 640, 480, {fps(30), fps(60, 2), fps(15), fps(30)});

    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(mode->frameRates(), (std::vector{fps(30), fps(15)}));
}

TEST(VideoMode, ComparesByValue)
{
    EXPECT_EQ(VideoMode::create(mjpg(), 640, 480, {fps(30), fps(15)}).value(),
              VideoMode::create(mjpg(), 640, 480, {fps(15), fps(30)}).value());
    EXPECT_NE(VideoMode::create(mjpg(), 640, 480, {fps(30)}).value(),
              VideoMode::create(mjpg(), 640, 481, {fps(30)}).value());
}
