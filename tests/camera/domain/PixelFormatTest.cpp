#include "modules/camera/domain/PixelFormat.hpp"

#include <gtest/gtest.h>

using ghub::camera::PixelFormat;
using ghub::shared_kernel::ErrorCode;

TEST(PixelFormat, Accepts_FourPrintableCharacters)
{
    auto format = PixelFormat::create("MJPG");

    ASSERT_TRUE(format.has_value());
    EXPECT_EQ(format->fourcc(), "MJPG");
}

TEST(PixelFormat, Rejects_WrongLength)
{
    EXPECT_EQ(PixelFormat::create("").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(PixelFormat::create("YUV").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(PixelFormat::create("YUYV2").error().code, ErrorCode::InvalidArgument);
}

TEST(PixelFormat, Rejects_SpaceAndNonPrintable)
{
    EXPECT_EQ(PixelFormat::create("YU V").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(PixelFormat::create(std::string_view{"YU\0V", 4}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(PixelFormat::create("YU\x7F"
                                  "V")
                  .error()
                  .code,
              ErrorCode::InvalidArgument);
    EXPECT_EQ(PixelFormat::create("YU\xC3V").error().code, ErrorCode::InvalidArgument);
}

TEST(PixelFormat, ComparesByValue)
{
    EXPECT_EQ(PixelFormat::create("MJPG").value(), PixelFormat::create("MJPG").value());
    EXPECT_NE(PixelFormat::create("MJPG").value(), PixelFormat::create("YUYV").value());
}
