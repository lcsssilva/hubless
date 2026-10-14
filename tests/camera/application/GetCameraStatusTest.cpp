#include "modules/camera/application/GetCameraStatus.hpp"
#include "fakes/FakeCameraBackend.hpp"

#include <gtest/gtest.h>

using ghub::camera::CameraControl;
using ghub::camera::CameraDescription;
using ghub::camera::CameraState;
using ghub::camera::CameraStatus;
using ghub::camera::ControlKind;
using ghub::camera::FrameRate;
using ghub::camera::GetCameraStatus;
using ghub::camera::PixelFormat;
using ghub::camera::VideoMode;
using ghub::shared_kernel::ErrorCode;

namespace {

CameraDescription brio()
{
    return {
        .name = "Brio 100",
        .modes =
            {
                VideoMode::create(PixelFormat::create("MJPG").value(), 1920, 1080, {FrameRate::create(30, 1).value()})
                    .value(),
            },
        .controls =
            {
                CameraControl::create({
                                          .id = 1,
                                          .name = "Brightness",
                                          .kind = ControlKind::Integer,
                                          .minimum = 0,
                                          .maximum = 255,
                                          .step = 1,
                                          .defaultValue = 128,
                                          .currentValue = 128,
                                          .inactive = false,
                                          .readOnly = false,
                                          .options = {},
                                      })
                    .value(),
            },
    };
}

class GetCameraStatusTest : public ::testing::Test {
protected:
    ghub::fakes::FakeCameraBackend backend;
    GetCameraStatus getCameraStatus{backend};
};

} // namespace

TEST_F(GetCameraStatusTest, ReportsNotDetected)
{
    backend.setResult(std::optional<CameraDescription>{});

    const auto status = getCameraStatus.execute();

    EXPECT_EQ(status.state(), CameraState::NotDetected);
    EXPECT_EQ(status.description(), nullptr);
    EXPECT_EQ(status.error(), nullptr);
}

TEST_F(GetCameraStatusTest, ReportsUnavailable_WithBackendError)
{
    backend.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "open negado"));

    const auto status = getCameraStatus.execute();

    EXPECT_EQ(status.state(), CameraState::Unavailable);
    ASSERT_NE(status.error(), nullptr);
    EXPECT_EQ(status.error()->code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(status.error()->detail, "open negado");
    EXPECT_EQ(status.description(), nullptr);
}

TEST_F(GetCameraStatusTest, ReportsDescription)
{
    backend.setResult(std::optional<CameraDescription>{brio()});

    const auto status = getCameraStatus.execute();

    EXPECT_EQ(status.state(), CameraState::Ready);
    ASSERT_NE(status.description(), nullptr);
    EXPECT_EQ(*status.description(), brio());
    EXPECT_EQ(status.error(), nullptr);
}

TEST_F(GetCameraStatusTest, ReadsBackendOncePerExecution)
{
    (void)getCameraStatus.execute();
    (void)getCameraStatus.execute();

    EXPECT_EQ(backend.calls(), 2);
}

TEST(CameraStatus, ComparesByValue)
{
    EXPECT_EQ(CameraStatus::notDetected(), CameraStatus::notDetected());
    EXPECT_NE(CameraStatus::notDetected(), CameraStatus::ready(brio()));
    EXPECT_EQ(CameraStatus::ready(brio()), CameraStatus::ready(brio()));
}
