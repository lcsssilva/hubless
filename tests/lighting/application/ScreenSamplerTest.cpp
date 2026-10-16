#include "modules/lighting/application/ScreenSampler.hpp"

#include "fakes/FakeScreenColorSource.hpp"
#include "modules/lighting/domain/ScreenRegions.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using ghub::lighting::application::ScreenSampler;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::domain::screenRegionsFrame;
using ghub::shared_kernel::ErrorCode;

std::vector<RgbColor> tenRegions()
{
    std::vector<RgbColor> regions;
    for (std::uint8_t i = 0; i < 10; ++i) {
        regions.emplace_back(i, 100, 200);
    }
    return regions;
}

TEST(ScreenSamplerTest, ConstructingDoesNotStartTheCapture)
{
    ghub::fakes::FakeScreenColorSource screen;
    const ScreenSampler sampler{screen};

    EXPECT_EQ(screen.starts(), 0);
    EXPECT_FALSE(screen.running());
}

TEST(ScreenSamplerTest, StartOpensTheCaptureAndStopClosesIt)
{
    ghub::fakes::FakeScreenColorSource screen;
    ScreenSampler sampler{screen};

    ASSERT_TRUE(sampler.start().has_value());
    EXPECT_TRUE(screen.running());
    sampler.stop();
    EXPECT_FALSE(screen.running());
}

TEST(ScreenSamplerTest, DestroyingWhileRunningClosesTheCapture)
{
    ghub::fakes::FakeScreenColorSource screen;
    {
        ScreenSampler sampler{screen};
        ASSERT_TRUE(sampler.start().has_value());
    }
    EXPECT_FALSE(screen.running());
}

TEST(ScreenSamplerTest, StartFailureIsReturnedAndNothingStaysOpen)
{
    ghub::fakes::FakeScreenColorSource screen;
    screen.failStart({.code = ErrorCode::BackendUnavailable, .detail = "negado"});
    ScreenSampler sampler{screen};

    const auto started = sampler.start();

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, ErrorCode::BackendUnavailable);
    EXPECT_FALSE(screen.running());
}

TEST(ScreenSamplerTest, FrameSourcePaintsTheCurrentRegionsIgnoringTime)
{
    ghub::fakes::FakeScreenColorSource screen;
    screen.setRegions(tenRegions());
    ScreenSampler sampler{screen};
    ASSERT_TRUE(sampler.start().has_value());
    const auto source = sampler.frameSource();

    EXPECT_EQ(source(0), screenRegionsFrame(tenRegions()));
    EXPECT_EQ(source(123456), screenRegionsFrame(tenRegions()));
}

TEST(ScreenSamplerTest, FrameIsBlackBeforeTheFirstFrameAndAfterStop)
{
    ghub::fakes::FakeScreenColorSource screen;
    ScreenSampler sampler{screen};
    const auto source = sampler.frameSource();
    const auto black = screenRegionsFrame({});

    ASSERT_TRUE(sampler.start().has_value());
    EXPECT_EQ(source(0), black); // sem regiões ainda
    screen.setRegions(tenRegions());
    EXPECT_NE(source(0), black);
    sampler.stop();
    EXPECT_EQ(source(0), black);
}

} // namespace
