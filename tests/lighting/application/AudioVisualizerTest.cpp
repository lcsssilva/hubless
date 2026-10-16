#include "modules/lighting/application/AudioVisualizer.hpp"

#include "fakes/FakeAudioLevelSource.hpp"
#include "modules/lighting/domain/AnimationFrames.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {

using ghub::lighting::application::AudioVisualizer;
using ghub::lighting::domain::audioBarsFrame;
using ghub::lighting::domain::RgbColor;
using ghub::shared_kernel::ErrorCode;

constexpr RgbColor kGreen{0, 255, 0};

TEST(AudioVisualizerTest, ConstructingDoesNotStartTheCapture)
{
    ghub::fakes::FakeAudioLevelSource audio;
    const AudioVisualizer visualizer{audio, kGreen};

    EXPECT_EQ(audio.starts(), 0);
    EXPECT_FALSE(audio.running());
}

TEST(AudioVisualizerTest, StartOpensTheCaptureAndStopClosesIt)
{
    ghub::fakes::FakeAudioLevelSource audio;
    AudioVisualizer visualizer{audio, kGreen};

    ASSERT_TRUE(visualizer.start().has_value());
    EXPECT_TRUE(audio.running());
    visualizer.stop();
    EXPECT_FALSE(audio.running());
    EXPECT_EQ(audio.starts(), 1);
}

TEST(AudioVisualizerTest, DestroyingWhileRunningClosesTheCapture)
{
    ghub::fakes::FakeAudioLevelSource audio;
    {
        AudioVisualizer visualizer{audio, kGreen};
        ASSERT_TRUE(visualizer.start().has_value());
    }
    EXPECT_FALSE(audio.running());
    EXPECT_GE(audio.stops(), 1);
}

TEST(AudioVisualizerTest, FailedStartIsReportedAndLeavesNothingOpen)
{
    ghub::fakes::FakeAudioLevelSource audio;
    audio.failStart({ErrorCode::BackendUnavailable, "sem PipeWire"});
    AudioVisualizer visualizer{audio, kGreen};

    const auto started = visualizer.start();

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, ErrorCode::BackendUnavailable);
    EXPECT_FALSE(audio.running());
}

TEST(AudioVisualizerTest, FrameSourceDrawsTheCurrentLevelsAsBarsIgnoringTime)
{
    ghub::fakes::FakeAudioLevelSource audio;
    AudioVisualizer visualizer{audio, kGreen};
    ASSERT_TRUE(visualizer.start().has_value());
    const std::array<float, 10> levels{0.0F, 0.3F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 0.0F};
    audio.setLevels(levels);

    const auto source = visualizer.frameSource();

    EXPECT_EQ(source(0), audioBarsFrame(levels, kGreen));
    EXPECT_EQ(source(123456), audioBarsFrame(levels, kGreen));
}

TEST(AudioVisualizerTest, AfterStopTheFrameIsDark)
{
    ghub::fakes::FakeAudioLevelSource audio;
    AudioVisualizer visualizer{audio, kGreen};
    ASSERT_TRUE(visualizer.start().has_value());
    audio.setLevels({1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F});
    const auto source = visualizer.frameSource();
    visualizer.stop();

    EXPECT_EQ(source(0), audioBarsFrame({}, kGreen));
}

} // namespace
