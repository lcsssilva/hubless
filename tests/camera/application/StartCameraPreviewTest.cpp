#include "modules/camera/application/StartCameraPreview.hpp"
#include "fakes/FakeCameraBackend.hpp"
#include "fakes/FakePreviewRunner.hpp"

#include <gtest/gtest.h>

#include <string_view>

using ghub::camera::CameraDescription;
using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::PreviewError;
using ghub::camera::PreviewFailure;
using ghub::camera::PreviewMode;
using ghub::camera::StartCameraPreview;
using ghub::camera::VideoMode;
using ghub::shared_kernel::ErrorCode;

namespace {

FrameRate fps(std::uint32_t frames)
{
    return FrameRate::create(frames, 1).value();
}

VideoMode mode(std::string_view fourcc, std::uint32_t width, std::uint32_t height, FrameRate rate)
{
    return VideoMode::create(PixelFormat::create(fourcc).value(), width, height, {rate}).value();
}

CameraDescription brio()
{
    return {
        .name = "Brio 100",
        .modes = {mode("YUYV", 640, 480, fps(30)), mode("MJPG", 1280, 720, fps(30)), mode("MJPG", 1920, 1080, fps(30))},
        .controls = {},
    };
}

PreviewMode selectedMode()
{
    return {.format = PixelFormat::create("MJPG").value(), .width = 1280, .height = 720, .frameRate = fps(30)};
}

class StartCameraPreviewTest : public ::testing::Test {
protected:
    void SetUp() override { backend.setResult(std::optional<CameraDescription>{brio()}); }

    ghub::fakes::FakeCameraBackend backend;
    ghub::fakes::FakePreviewRunner runner;
    StartCameraPreview startCameraPreview{backend, runner};
};

} // namespace

TEST_F(StartCameraPreviewTest, FailsNotDetected_WithoutStartingRunner)
{
    backend.setResult(std::optional<CameraDescription>{});

    const auto result = startCameraPreview.execute();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().reason, PreviewFailure::CameraNotDetected);
    EXPECT_TRUE(runner.startedModes().empty());
}

TEST_F(StartCameraPreviewTest, FailsUnavailable_WithBackendDetail)
{
    backend.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "open negado"));

    const auto result = startCameraPreview.execute();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), (PreviewError{PreviewFailure::CameraUnavailable, "open negado"}));
    EXPECT_TRUE(runner.startedModes().empty());
}

TEST_F(StartCameraPreviewTest, FailsNoCompatibleMode_WithoutStartingRunner)
{
    // Só H264 e um modo acima de 720p: nada que o produto saiba mostrar.
    backend.setResult(std::optional<CameraDescription>{CameraDescription{
        .name = "Brio 100",
        .modes = {mode("H264", 1280, 720, fps(30)), mode("MJPG", 1920, 1080, fps(30))},
        .controls = {},
    }});

    const auto result = startCameraPreview.execute();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().reason, PreviewFailure::NoCompatibleMode);
    EXPECT_TRUE(runner.startedModes().empty());
}

TEST_F(StartCameraPreviewTest, StartsRunnerOnce_WithSelectedMode)
{
    const auto result = startCameraPreview.execute();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, selectedMode());
    ASSERT_EQ(runner.startedModes().size(), 1U);
    EXPECT_EQ(runner.startedModes().front(), selectedMode());
    EXPECT_EQ(backend.calls(), 1);
}
