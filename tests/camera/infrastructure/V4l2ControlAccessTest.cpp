#include "modules/camera/infrastructure/V4l2ControlAccess.hpp"

#include "FakeV4l2Api.hpp"
#include "TempDir.hpp"

#include <gtest/gtest.h>

#include <linux/videodev2.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

using ghub::camera::CameraControlSession;
using ghub::camera::V4l2ControlAccess;
using ghub::camera::testing::FakeV4l2Api;
using ghub::camera::testing::TempDir;
using ghub::device_inventory::UsbId;
using ghub::shared_kernel::ErrorCode;
namespace fixtures = ghub::camera::testing::fixtures;
namespace fs = std::filesystem;

namespace {

constexpr UsbId brioId{.vendorId = 0x046d, .productId = 0x094c};
constexpr auto brioModalias = "usb:v046Dp094Cd9915dcEFdsc02dp01ic0Eisc01ip00in00\n";
constexpr auto otherModalias = "usb:v046Dp0AFEd0100dc00dsc00dp00ic01isc01ip00in00\n";

constexpr std::uint32_t brightnessId = V4L2_CID_BRIGHTNESS;
constexpr std::uint32_t autoExposureId = V4L2_CID_EXPOSURE_AUTO;
constexpr std::uint32_t exposureTimeId = V4L2_CID_EXPOSURE_ABSOLUTE;
constexpr std::uint32_t autoWhiteBalanceId = V4L2_CID_AUTO_WHITE_BALANCE;

class V4l2ControlAccessTest : public ::testing::Test {
protected:
    void addSysfsEntry(const std::string& name, const std::string& modalias = brioModalias)
    {
        const fs::path directory = sysfs_.path() / name / "device";
        fs::create_directories(directory);
        std::ofstream(directory / "modalias") << modalias;
    }

    V4l2ControlAccessTest()
    {
        addSysfsEntry("video0");
        addSysfsEntry("video1");
    }

    ghub::shared_kernel::Result<std::unique_ptr<CameraControlSession>> open()
    {
        V4l2ControlAccess access{brioId, api_, sysfs_.path()};
        return access.open();
    }

    std::unique_ptr<CameraControlSession> openOk()
    {
        auto session = open();
        EXPECT_TRUE(session.has_value());
        return session.has_value() ? std::move(*session) : nullptr;
    }

    [[nodiscard]] bool wroteControls() const
    {
        return std::ranges::contains(api_.requests, static_cast<unsigned long>(VIDIOC_S_CTRL));
    }

    TempDir sysfs_;
    FakeV4l2Api api_ = fixtures::brio100();
};

// --- abrir ---

TEST_F(V4l2ControlAccessTest, OpensTheCaptureNode_WithoutAnyIoctlButQuerycap)
{
    const auto session = openOk();

    ASSERT_NE(session, nullptr);
    ASSERT_EQ(api_.opened.size(), 1U);
    EXPECT_EQ(api_.pathOf(api_.opened[0]), "/dev/video0");
    EXPECT_TRUE(api_.streamingOpened.empty()); // abre sem intenção de escrita, nunca para streaming
    EXPECT_EQ(api_.requests, std::vector<unsigned long>{VIDIOC_QUERYCAP});
}

TEST_F(V4l2ControlAccessTest, SkipsMetadataNode)
{
    api_ = fixtures::brio100("/dev/video1", "/dev/video0");

    const auto session = openOk();

    ASSERT_NE(session, nullptr);
    ASSERT_EQ(api_.opened.size(), 2U); // abriu o nó de metadados, viu que não captura e o fechou
    EXPECT_EQ(api_.pathOf(api_.opened[1]), "/dev/video1");
    EXPECT_EQ(api_.closed.size(), 1U);
    EXPECT_EQ(api_.closed[0], api_.opened[0]);
}

TEST_F(V4l2ControlAccessTest, FailsNotDetected_WhenNoNodeMatchesTheCamera)
{
    fs::remove_all(sysfs_.path());
    addSysfsEntry("video0", otherModalias);

    const auto session = open();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(session.error().detail.find("câmera não detectada"), std::string::npos) << session.error().detail;
    EXPECT_TRUE(api_.opened.empty());
}

TEST_F(V4l2ControlAccessTest, FailsNotDetected_WhenOnlyMetadataNodeExists)
{
    fs::remove_all(sysfs_.path());
    addSysfsEntry("video1");
    api_ = fixtures::brio100("/dev/video0", "/dev/video1");
    api_.nodes.erase(api_.nodes.begin()); // sobra só o nó de metadados

    const auto session = open();

    ASSERT_FALSE(session.has_value());
    EXPECT_NE(session.error().detail.find("câmera não detectada"), std::string::npos) << session.error().detail;
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2ControlAccessTest, TreatsEnodevOnOpenAsNotDetected)
{
    api_.openErrors["/dev/video0"] = -ENODEV;

    const auto session = open();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(session.error().detail.find("câmera não detectada"), std::string::npos) << session.error().detail;
}

TEST_F(V4l2ControlAccessTest, FailsUnavailable_WhenOpenIsDenied)
{
    api_.openErrors["/dev/video0"] = -EACCES;

    const auto session = open();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(session.error().detail.find("/dev/video0"), std::string::npos) << session.error().detail;
}

TEST_F(V4l2ControlAccessTest, FailsUnavailable_AndClosesTheNode_WhenQuerycapFails)
{
    api_.requestErrors[VIDIOC_QUERYCAP] = -EIO;

    const auto session = open();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

// --- ler ---

TEST_F(V4l2ControlAccessTest, ReadsMenuControlWithItsOptionsAndId)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto control = session->readControl(autoExposureId);

    ASSERT_TRUE(control.has_value()) << control.error().detail;
    EXPECT_EQ(control->id(), autoExposureId);
    EXPECT_EQ(control->name(), "Auto Exposure");
    ASSERT_EQ(control->options().size(), 2U);
    EXPECT_EQ(control->options()[0].label, "Manual Mode");
    EXPECT_EQ(control->currentValue(), std::optional<std::int64_t>{3});
}

TEST_F(V4l2ControlAccessTest, ReadReportsInactiveFlag)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto control = session->readControl(exposureTimeId);

    ASSERT_TRUE(control.has_value()) << control.error().detail;
    EXPECT_TRUE(control->inactive());
}

TEST_F(V4l2ControlAccessTest, ReadsTheDeviceEveryTime_NotACache)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);
    ASSERT_TRUE(session->readControl(brightnessId).has_value());

    api_.nodes[0].controls[1].value = 77; // outro programa mudou o controle

    const auto control = session->readControl(brightnessId);
    ASSERT_TRUE(control.has_value());
    EXPECT_EQ(control->currentValue(), std::optional<std::int64_t>{77});
}

TEST_F(V4l2ControlAccessTest, ReadsWithExactIdAndNeverWrites)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);
    api_.requests.clear();

    ASSERT_TRUE(session->readControl(brightnessId).has_value());

    EXPECT_EQ(api_.requests, (std::vector<unsigned long>{VIDIOC_QUERY_EXT_CTRL, VIDIOC_G_CTRL}));
}

TEST_F(V4l2ControlAccessTest, ReadFailsInvalidArgument_WhenControlDoesNotExist)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto control = session->readControl(0x00980fff);

    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::InvalidArgument);
    EXPECT_NE(control.error().detail.find("controle inexistente"), std::string::npos) << control.error().detail;
}

TEST_F(V4l2ControlAccessTest, ReadFailsInvalidArgument_WhenIdCarriesEnumerationFlags)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto control = session->readControl(brightnessId | V4L2_CTRL_FLAG_NEXT_CTRL);

    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::InvalidArgument);
}

TEST_F(V4l2ControlAccessTest, ReadFailsInvalidArgument_WhenControlTypeIsUnsupported)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto control = session->readControl(V4L2_CID_USER_CLASS);

    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::InvalidArgument);
}

TEST_F(V4l2ControlAccessTest, ReadFailsInvalidArgument_WhenControlIsDisabled)
{
    api_.nodes[0].controls[1].flags = V4L2_CTRL_FLAG_DISABLED;
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto control = session->readControl(brightnessId);

    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::InvalidArgument);
}

TEST_F(V4l2ControlAccessTest, ReadFailsUnavailable_WhenCameraIsRemovedOnGetCtrl)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);
    api_.requestErrors[VIDIOC_G_CTRL] = -ENODEV;

    const auto control = session->readControl(brightnessId);

    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::BackendUnavailable);
}

TEST_F(V4l2ControlAccessTest, ReadFailsUnavailable_WhenQueryFailsWithAnotherError)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);
    api_.requestErrors[VIDIOC_QUERY_EXT_CTRL] = -EIO;

    const auto control = session->readControl(brightnessId);

    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::BackendUnavailable);
}

// --- gravar ---

TEST_F(V4l2ControlAccessTest, WritesTheValueWithSetCtrl_AndReadsItBack)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto written = session->writeControl(brightnessId, 200);

    ASSERT_TRUE(written.has_value()) << written.error().detail;
    ASSERT_EQ(api_.controlWrites.size(), 1U);
    EXPECT_EQ(api_.controlWrites[0].id, brightnessId);
    EXPECT_EQ(api_.controlWrites[0].requested, 200);
    EXPECT_EQ(session->readControl(brightnessId)->currentValue(), std::optional<std::int64_t>{200});
}

TEST_F(V4l2ControlAccessTest, WriteSendsTheValueUntouched_EvenWhenTheDriverWouldRound)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    ASSERT_TRUE(session->writeControl(brightnessId, 1000).has_value()); // o fake limita como o uvcvideo

    EXPECT_EQ(api_.controlWrites[0].requested, 1000);
    EXPECT_EQ(api_.controlWrites[0].applied, 255);
}

TEST_F(V4l2ControlAccessTest, WriteSupportsNegativeInt32Boundaries)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    ASSERT_TRUE(session->writeControl(brightnessId, std::numeric_limits<std::int32_t>::min()).has_value());
    ASSERT_TRUE(session->writeControl(brightnessId, std::numeric_limits<std::int32_t>::max()).has_value());

    EXPECT_EQ(api_.controlWrites[0].requested, std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(api_.controlWrites[1].requested, std::numeric_limits<std::int32_t>::max());
}

TEST_F(V4l2ControlAccessTest, WriteRejectsValueOutsideInt32_WithoutCallingTheDriver)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    for (const std::int64_t value : {
             static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1,
             static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) - 1,
         }) {
        const auto written = session->writeControl(brightnessId, value);

        ASSERT_FALSE(written.has_value());
        EXPECT_EQ(written.error().code, ErrorCode::InvalidArgument);
    }
    EXPECT_FALSE(wroteControls());
}

TEST_F(V4l2ControlAccessTest, WriteMapsDriverErrors)
{
    struct Case {
        int error;
        ErrorCode expected;
        std::string fragment;
    };
    const std::vector<Case> cases{
        {.error = -EACCES, .expected = ErrorCode::InvalidArgument, .fragment = ""},
        {.error = -ERANGE, .expected = ErrorCode::InvalidArgument, .fragment = ""},
        {.error = -EINVAL, .expected = ErrorCode::InvalidArgument, .fragment = ""},
        {.error = -EBUSY, .expected = ErrorCode::DeviceBusy, .fragment = ""},
        {.error = -ENODEV, .expected = ErrorCode::BackendUnavailable, .fragment = "câmera removida"},
        {.error = -EIO, .expected = ErrorCode::BackendUnavailable, .fragment = "Input/output error"},
    };
    const auto session = openOk();
    ASSERT_NE(session, nullptr);
    for (const Case& test : cases) {
        api_.requestErrors[VIDIOC_S_CTRL] = test.error;

        const auto written = session->writeControl(brightnessId, 10);

        ASSERT_FALSE(written.has_value()) << "errno " << -test.error;
        EXPECT_EQ(written.error().code, test.expected) << "errno " << -test.error;
        EXPECT_NE(written.error().detail.find(test.fragment), std::string::npos) << written.error().detail;
    }
    EXPECT_TRUE(api_.controlWrites.empty());
}

TEST_F(V4l2ControlAccessTest, WriteToInactiveControl_FailsWithInvalidArgument_AsEacces)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);

    const auto written = session->writeControl(exposureTimeId, 200);

    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error().code, ErrorCode::InvalidArgument);
}

TEST_F(V4l2ControlAccessTest, WriteDoesNotRetry)
{
    const auto session = openOk();
    ASSERT_NE(session, nullptr);
    api_.requestErrors[VIDIOC_S_CTRL] = -EIO;
    api_.requests.clear();

    ASSERT_FALSE(session->writeControl(brightnessId, 10).has_value());

    EXPECT_EQ(api_.requests, std::vector<unsigned long>{VIDIOC_S_CTRL});
}

// --- descritores e lista de ioctls ---

TEST_F(V4l2ControlAccessTest, KeepsTheNodeOpenUntilTheSessionIsDestroyed)
{
    auto session = openOk();
    ASSERT_NE(session, nullptr);
    ASSERT_EQ(api_.opened.size(), 1U);
    EXPECT_TRUE(api_.closed.empty());

    session.reset();

    EXPECT_EQ(api_.closed, api_.opened);
}

TEST_F(V4l2ControlAccessTest, ClosesEveryDescriptor_OnEveryFailure)
{
    {
        const auto session = openOk();
        ASSERT_NE(session, nullptr);
        api_.requestErrors[VIDIOC_S_CTRL] = -ENODEV;
        EXPECT_FALSE(session->writeControl(brightnessId, 10).has_value());
        api_.requestErrors[VIDIOC_G_CTRL] = -ENODEV;
        EXPECT_FALSE(session->readControl(brightnessId).has_value());
        EXPECT_FALSE(session->readControl(0x00980fff).has_value());
    }
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2ControlAccessTest, UsesOnlyControlIoctls)
{
    {
        const auto session = openOk();
        ASSERT_NE(session, nullptr);
        ASSERT_TRUE(session->readControl(autoExposureId).has_value()); // menu: QUERYMENU
        ASSERT_TRUE(session->writeControl(autoWhiteBalanceId, 0).has_value());
    }

    const std::set<unsigned long> allowed{
        VIDIOC_QUERYCAP, VIDIOC_QUERY_EXT_CTRL, VIDIOC_QUERYMENU, VIDIOC_G_CTRL, VIDIOC_S_CTRL,
    };
    const std::set<unsigned long> used(api_.requests.begin(), api_.requests.end());
    EXPECT_EQ(used, allowed);
    EXPECT_FALSE(std::ranges::contains(api_.requests, static_cast<unsigned long>(VIDIOC_S_EXT_CTRLS)));
    EXPECT_FALSE(std::ranges::contains(api_.requests, static_cast<unsigned long>(VIDIOC_S_PARM)));
}

} // namespace
