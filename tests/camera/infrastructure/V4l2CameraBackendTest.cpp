#include "modules/camera/infrastructure/V4l2CameraBackend.hpp"

#include "modules/camera/domain/VideoModeSummary.hpp"

#include "FakeV4l2Api.hpp"
#include "TempDir.hpp"

#include <gtest/gtest.h>

#include <linux/videodev2.h>

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <set>
#include <string>
#include <utility>

using ghub::camera::V4l2CameraBackend;
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

class V4l2CameraBackendTest : public ::testing::Test {
protected:
    // Cria <sysfs>/<name>/device/modalias.
    void addSysfsEntry(const std::string& name, const std::string& modalias = brioModalias)
    {
        const fs::path directory = sysfs_.path() / name / "device";
        fs::create_directories(directory);
        std::ofstream(directory / "modalias") << modalias;
    }

    // sysfs e fake de uma Brio 100 com captura em video0 e metadados em video1.
    FakeV4l2Api brioWithSysfs()
    {
        addSysfsEntry("video0");
        addSysfsEntry("video1");
        return fixtures::brio100();
    }

    ghub::shared_kernel::Result<std::optional<ghub::camera::CameraDescription>> read(FakeV4l2Api& api)
    {
        V4l2CameraBackend backend{brioId, api, sysfs_.path()};
        return backend.readCamera();
    }

    TempDir sysfs_;
};

TEST_F(V4l2CameraBackendTest, ReadsCameraName)
{
    FakeV4l2Api api = brioWithSysfs();

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_EQ((*result)->name, "Brio 100");
}

TEST_F(V4l2CameraBackendTest, SkipsMetadataNode)
{
    addSysfsEntry("video0");
    addSysfsEntry("video1");
    FakeV4l2Api api = fixtures::brio100("/dev/video1", "/dev/video0");
    api.nodes[0].card = "Capture node";
    api.nodes[1].card = "Metadata node";

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_EQ((*result)->name, "Capture node");
}

TEST_F(V4l2CameraBackendTest, ReturnsNothing_WhenNoMatchingNode)
{
    addSysfsEntry("video0", otherModalias);
    FakeV4l2Api api = fixtures::brio100();

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
    EXPECT_TRUE(api.opened.empty());
}

TEST_F(V4l2CameraBackendTest, ReturnsNothing_WhenOnlyMetadataNode)
{
    addSysfsEntry("video1");
    FakeV4l2Api api = fixtures::brio100();

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
    EXPECT_TRUE(api.allDescriptorsClosed());
}

TEST_F(V4l2CameraBackendTest, FailsUnavailable_WhenSysfsRootMissing)
{
    FakeV4l2Api api = fixtures::brio100();
    V4l2CameraBackend backend{brioId, api, sysfs_.path() / "missing"};

    const auto result = backend.readCamera();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("missing"), std::string::npos);
}

TEST_F(V4l2CameraBackendTest, SkipsNode_WhenDeviceNodeVanished)
{
    for (const int error : {-ENOENT, -ENODEV, -ENXIO}) {
        TempDir sysfs;
        const fs::path directory0 = sysfs.path() / "video0" / "device";
        const fs::path directory1 = sysfs.path() / "video1" / "device";
        fs::create_directories(directory0);
        fs::create_directories(directory1);
        std::ofstream(directory0 / "modalias") << brioModalias;
        std::ofstream(directory1 / "modalias") << brioModalias;
        FakeV4l2Api api = fixtures::brio100("/dev/video1", "/dev/video2");
        api.nodes[0].card = "Second node";
        api.openErrors["/dev/video0"] = error;
        V4l2CameraBackend backend{brioId, api, sysfs.path()};

        const auto result = backend.readCamera();

        ASSERT_TRUE(result.has_value()) << error;
        if (!result->has_value()) {
            FAIL() << error;
        }
        EXPECT_EQ((*result)->name, "Second node");
    }
}

TEST_F(V4l2CameraBackendTest, FailsUnavailable_WhenOpenDenied)
{
    FakeV4l2Api api = brioWithSysfs();
    api.openErrors["/dev/video0"] = -EACCES;

    const auto result = read(api);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("/dev/video0"), std::string::npos);
    EXPECT_NE(result.error().detail.find("Permission denied"), std::string::npos);
}

TEST_F(V4l2CameraBackendTest, OrdersNodesNumerically)
{
    addSysfsEntry("video10");
    addSysfsEntry("video2");
    FakeV4l2Api api;
    api.nodes.push_back(fixtures::captureNode("/dev/video10", "Ten"));
    api.nodes.push_back(fixtures::captureNode("/dev/video2", "Two"));

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_EQ((*result)->name, "Two");
}

TEST_F(V4l2CameraBackendTest, HandlesCardWithoutNulTerminator)
{
    FakeV4l2Api api = brioWithSysfs();
    api.nodes[0].card = std::string(32, 'A');

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_EQ((*result)->name, std::string(32, 'A'));
}

TEST_F(V4l2CameraBackendTest, ReadsFormatsSizesAndIntervals)
{
    FakeV4l2Api api = brioWithSysfs();

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    const auto& modes = (*result)->modes;
    ASSERT_EQ(modes.size(), 38U);
    EXPECT_EQ(modes.front().format().fourcc(), "YUYV");
    EXPECT_EQ(modes.front().width(), 640U);
    EXPECT_EQ(modes.front().height(), 480U);
    ASSERT_EQ(modes.front().frameRates().size(), 7U);
    EXPECT_EQ(modes.front().maxFrameRate(), ghub::camera::FrameRate::create(30, 1).value());
    EXPECT_EQ(modes.front().frameRates().back(), ghub::camera::FrameRate::create(5, 1).value());
    // 2/15 s por quadro = 7,5 fps (15 quadros em 2 s).
    EXPECT_EQ(modes.front().frameRates()[5], ghub::camera::FrameRate::create(15, 2).value());
    const auto summary = ghub::camera::summarizeModes(modes);
    ASSERT_EQ(summary.size(), 2U);
    EXPECT_EQ(summary[0].format.fourcc(), "YUYV");
    EXPECT_EQ(summary[0].width, 1920U);
    EXPECT_EQ(summary[0].maxFrameRate, ghub::camera::FrameRate::create(5, 1).value());
    EXPECT_EQ(summary[1].format.fourcc(), "MJPG");
    EXPECT_EQ(summary[1].width, 1920U);
    EXPECT_EQ(summary[1].maxFrameRate, ghub::camera::FrameRate::create(30, 1).value());
}

TEST_F(V4l2CameraBackendTest, IgnoresStepwiseFrameSizes_AndStepwiseIntervals)
{
    FakeV4l2Api api = brioWithSysfs();
    ghub::camera::testing::FakeFormat stepSizes{
        .fourcc = "NV12",
        .sizes =
            {
                {
                    .width = 640,
                    .height = 480,
                    .intervals =
                        {
                            {.numerator = 1, .denominator = 30},
                        },
                    .stepwiseIntervals = false,
                },
            },
        .stepwiseSizes = true,
        .rawPixelFormat = std::nullopt,
    };
    ghub::camera::testing::FakeFormat stepIntervals{
        .fourcc = "H264",
        .sizes =
            {
                {
                    .width = 640,
                    .height = 480,
                    .intervals =
                        {
                            {.numerator = 1, .denominator = 30},
                        },
                    .stepwiseIntervals = true,
                },
                {
                    .width = 320,
                    .height = 240,
                    .intervals =
                        {
                            {.numerator = 1, .denominator = 15},
                        },
                    .stepwiseIntervals = false,
                },
            },
        .stepwiseSizes = false,
        .rawPixelFormat = std::nullopt,
    };
    api.nodes[0].formats = {stepSizes, stepIntervals};

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    // O primeiro tamanho de H264 sem taxa discreta interrompe a enumeração de intervalos desse
    // tamanho, mas não vira modo; o tamanho seguinte continua válido.
    ASSERT_EQ((*result)->modes.size(), 1U);
    EXPECT_EQ((*result)->modes[0].format().fourcc(), "H264");
    EXPECT_EQ((*result)->modes[0].width(), 320U);
}

TEST_F(V4l2CameraBackendTest, RejectsZeroFrameInterval)
{
    for (const auto interval : {
             ghub::camera::testing::FakeInterval{.numerator = 0, .denominator = 30},
             ghub::camera::testing::FakeInterval{.numerator = 1, .denominator = 0},
         }) {
        FakeV4l2Api api = brioWithSysfs();
        api.nodes[0].formats[0].sizes[0].intervals = {interval};

        const auto result = read(api);

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    }
}

TEST_F(V4l2CameraBackendTest, RejectsInvalidFourcc)
{
    FakeV4l2Api api = brioWithSysfs();
    api.nodes[0].formats[0].rawPixelFormat = 0x00000000U;

    const auto result = read(api);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST_F(V4l2CameraBackendTest, RejectsEndlessFormatEnumeration)
{
    FakeV4l2Api api = brioWithSysfs();
    api.endlessFormats = true;

    const auto result = read(api);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

const ghub::camera::CameraControl* findControl(const ghub::camera::CameraDescription& description,
                                               const std::string& name)
{
    const auto found = std::ranges::find(description.controls, name, &ghub::camera::CameraControl::name);
    return found == description.controls.end() ? nullptr : &*found;
}

TEST_F(V4l2CameraBackendTest, ReadsBrio100Description)
{
    FakeV4l2Api api = brioWithSysfs();

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    const auto& description = **result;
    EXPECT_EQ(description.name, "Brio 100");
    EXPECT_EQ(description.modes.size(), 38U);
    ASSERT_EQ(description.controls.size(), 12U); // a entrada CTRL_CLASS não aparece
    EXPECT_EQ(description.controls.front().name(), "Brightness");
    EXPECT_EQ(description.controls.back().name(), "Exposure, Dynamic Framerate");

    const auto* brightness = findControl(description, "Brightness");
    ASSERT_NE(brightness, nullptr);
    EXPECT_EQ(brightness->kind(), ghub::camera::ControlKind::Integer);
    EXPECT_EQ(brightness->minimum(), 0);
    EXPECT_EQ(brightness->maximum(), 255);
    EXPECT_EQ(brightness->step(), 1);
    EXPECT_EQ(brightness->defaultValue(), 128);
    EXPECT_EQ(brightness->currentValue(), 128);
    EXPECT_FALSE(brightness->inactive());
    EXPECT_FALSE(brightness->readOnly());

    const auto* whiteBalanceAuto = findControl(description, "White Balance, Automatic");
    ASSERT_NE(whiteBalanceAuto, nullptr);
    EXPECT_EQ(whiteBalanceAuto->kind(), ghub::camera::ControlKind::Boolean);
    EXPECT_EQ(whiteBalanceAuto->currentValue(), 1);

    const auto* temperature = findControl(description, "White Balance Temperature");
    ASSERT_NE(temperature, nullptr);
    EXPECT_EQ(temperature->minimum(), 2800);
    EXPECT_EQ(temperature->maximum(), 7500);
    EXPECT_TRUE(temperature->inactive());

    const auto* powerLine = findControl(description, "Power Line Frequency");
    ASSERT_NE(powerLine, nullptr);
    EXPECT_EQ(powerLine->kind(), ghub::camera::ControlKind::Menu);
    ASSERT_EQ(powerLine->options().size(), 3U);
    const auto currentOption = powerLine->currentOption();
    if (!currentOption.has_value()) {
        FAIL() << "o menu deveria ter uma opção atual";
    }
    EXPECT_EQ(currentOption->label, "60 Hz");

    const auto* autoExposure = findControl(description, "Auto Exposure");
    ASSERT_NE(autoExposure, nullptr);
    ASSERT_EQ(autoExposure->options().size(), 2U);
    EXPECT_EQ(autoExposure->options()[0], (ghub::camera::MenuOption{1, "Manual Mode"}));
    EXPECT_EQ(autoExposure->options()[1], (ghub::camera::MenuOption{3, "Aperture Priority Mode"}));
}

TEST_F(V4l2CameraBackendTest, ReportsControlIdsFromTheDriver)
{
    FakeV4l2Api api = brioWithSysfs();

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_EQ(findControl(**result, "Brightness")->id(), static_cast<std::uint32_t>(V4L2_CID_BRIGHTNESS));
    EXPECT_EQ(findControl(**result, "Auto Exposure")->id(), static_cast<std::uint32_t>(V4L2_CID_EXPOSURE_AUTO));
    EXPECT_EQ(findControl(**result, "Exposure Time, Absolute")->id(),
              static_cast<std::uint32_t>(V4L2_CID_EXPOSURE_ABSOLUTE));
}

TEST_F(V4l2CameraBackendTest, SkipsDisabledAndUnsupportedControls)
{
    FakeV4l2Api api = brioWithSysfs();
    api.nodes[0].controls = {
        fixtures::control(1, V4L2_CTRL_TYPE_INTEGER, "Disabled", 0, 10, 0, 0, V4L2_CTRL_FLAG_DISABLED),
        fixtures::control(2, V4L2_CTRL_TYPE_BUTTON, "Button", 0, 0, 0, 0),
        fixtures::control(3, V4L2_CTRL_TYPE_INTEGER64, "Big", 0, 10, 0, 0),
        fixtures::control(4, V4L2_CTRL_TYPE_CTRL_CLASS, "Class", 0, 0, 0, 0),
        fixtures::control(5, V4L2_CTRL_TYPE_INTEGER, "Kept", 0, 10, 5, 5),
    };

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    ASSERT_EQ((*result)->controls.size(), 1U);
    EXPECT_EQ((*result)->controls[0].name(), "Kept");
}

TEST_F(V4l2CameraBackendTest, SkipsMissingMenuItems)
{
    FakeV4l2Api api = brioWithSysfs();

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    const auto* autoExposure = findControl(**result, "Auto Exposure");
    ASSERT_NE(autoExposure, nullptr);
    EXPECT_EQ(autoExposure->minimum(), 0);
    EXPECT_EQ(autoExposure->maximum(), 3);
    EXPECT_EQ(autoExposure->options().size(), 2U); // índices 0 e 2 devolvem EINVAL
}

TEST_F(V4l2CameraBackendTest, SkipsWriteOnlyValueRead)
{
    FakeV4l2Api api = brioWithSysfs();
    api.nodes[0].controls = {
        fixtures::control(1, V4L2_CTRL_TYPE_INTEGER, "Secret", 0, 10, 5, 7, V4L2_CTRL_FLAG_WRITE_ONLY),
    };

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    ASSERT_EQ((*result)->controls.size(), 1U);
    EXPECT_FALSE((*result)->controls[0].currentValue().has_value());
    EXPECT_EQ(std::ranges::count(api.requests, static_cast<unsigned long>(VIDIOC_G_CTRL)), 0);
}

TEST_F(V4l2CameraBackendTest, ListsControlWithoutValue_WhenValueReadFails)
{
    FakeV4l2Api api = brioWithSysfs();
    api.controlValueErrors[V4L2_CID_CONTRAST] = -EIO;

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    const auto* contrast = findControl(**result, "Contrast");
    ASSERT_NE(contrast, nullptr);
    EXPECT_FALSE(contrast->currentValue().has_value());
    EXPECT_EQ((*result)->controls.size(), 12U);
    EXPECT_EQ(findControl(**result, "Brightness")->currentValue(), 128);
}

TEST_F(V4l2CameraBackendTest, FailsUnavailable_WhenDeviceDisappearsMidEnumeration)
{
    {
        FakeV4l2Api api = brioWithSysfs();
        api.requestErrors[VIDIOC_ENUM_FRAMESIZES] = -ENODEV;

        const auto result = read(api);

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
        EXPECT_NE(result.error().detail.find("VIDIOC_ENUM_FRAMESIZES"), std::string::npos);
        EXPECT_TRUE(api.allDescriptorsClosed());
    }
    {
        FakeV4l2Api api = brioWithSysfs();
        api.requestErrors[VIDIOC_G_CTRL] = -ENODEV;

        const auto result = read(api);

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
        EXPECT_NE(result.error().detail.find("VIDIOC_G_CTRL"), std::string::npos);
    }
}

TEST_F(V4l2CameraBackendTest, RejectsNonIncreasingControlIds)
{
    FakeV4l2Api api = brioWithSysfs();
    api.stuckControlIds = true;

    const auto result = read(api);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST_F(V4l2CameraBackendTest, RejectsHugeMenuRange)
{
    for (const auto& [minimum, maximum] : {std::pair<std::int64_t, std::int64_t>{0, 256}, {-1, 2}}) {
        FakeV4l2Api api = brioWithSysfs();
        auto menu = fixtures::control(1, V4L2_CTRL_TYPE_MENU, "Menu", minimum, maximum, 0, 0);
        menu.items = {{.index = 0, .name = "Zero"}};
        api.nodes[0].controls = {menu};

        const auto result = read(api);

        ASSERT_FALSE(result.has_value()) << minimum << ".." << maximum;
        EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    }
}

TEST_F(V4l2CameraBackendTest, AcceptsLargestAllowedMenuRange)
{
    FakeV4l2Api api = brioWithSysfs();
    auto menu = fixtures::control(1, V4L2_CTRL_TYPE_MENU, "Menu", 0, 255, 0, 0);
    menu.items = {{.index = 0, .name = "Zero"}, {.index = 255, .name = "Last"}};
    api.nodes[0].controls = {menu};

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    ASSERT_EQ((*result)->controls.size(), 1U);
    EXPECT_EQ((*result)->controls[0].options().size(), 2U);
}

TEST_F(V4l2CameraBackendTest, RejectsIncoherentControl)
{
    FakeV4l2Api api = brioWithSysfs();
    api.nodes[0].controls = {fixtures::control(1, V4L2_CTRL_TYPE_INTEGER, "Broken", 10, 5, 7, 7)};

    const auto result = read(api);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_NE(result.error().detail.find("Broken"), std::string::npos);
}

TEST_F(V4l2CameraBackendTest, TreatsZeroStepAsOne)
{
    FakeV4l2Api api = brioWithSysfs();
    auto control = fixtures::control(1, V4L2_CTRL_TYPE_INTEGER, "Stepless", 0, 10, 5, 5);
    control.step = 0;
    api.nodes[0].controls = {control};

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    ASSERT_EQ((*result)->controls.size(), 1U);
    EXPECT_EQ((*result)->controls[0].step(), 1);
}

TEST_F(V4l2CameraBackendTest, RejectsStepAboveInt64Max)
{
    FakeV4l2Api api = brioWithSysfs();
    auto control = fixtures::control(1, V4L2_CTRL_TYPE_INTEGER, "Huge step", 0, 10, 5, 5);
    control.step = std::uint64_t{1} << 63U;
    api.nodes[0].controls = {control};

    const auto result = read(api);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST_F(V4l2CameraBackendTest, ReadsReadOnlyFlag)
{
    FakeV4l2Api api = brioWithSysfs();
    api.nodes[0].controls = {
        fixtures::control(1, V4L2_CTRL_TYPE_INTEGER, "Fixed", 0, 10, 5, 5, V4L2_CTRL_FLAG_READ_ONLY),
    };

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    ASSERT_EQ((*result)->controls.size(), 1U);
    EXPECT_TRUE((*result)->controls[0].readOnly());
}

TEST_F(V4l2CameraBackendTest, RejectsMoreControlsThanTheLimit)
{
    FakeV4l2Api api = brioWithSysfs();
    api.nodes[0].controls.clear();
    for (std::uint32_t id = 1; id <= 257; ++id) {
        api.nodes[0].controls.push_back(fixtures::control(id, V4L2_CTRL_TYPE_INTEGER, "C", 0, 1, 0, 0));
    }

    const auto result = read(api);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST_F(V4l2CameraBackendTest, ClosesEveryDescriptor_OnSuccessAndOnError)
{
    {
        FakeV4l2Api api = brioWithSysfs();
        ASSERT_TRUE(read(api).has_value());
        EXPECT_FALSE(api.opened.empty());
        EXPECT_TRUE(api.allDescriptorsClosed());
    }
    for (const unsigned long request : {
             static_cast<unsigned long>(VIDIOC_QUERYCAP),
             static_cast<unsigned long>(VIDIOC_ENUM_FMT),
             static_cast<unsigned long>(VIDIOC_QUERY_EXT_CTRL),
         }) {
        FakeV4l2Api api = brioWithSysfs();
        api.requestErrors[request] = -EIO;
        EXPECT_FALSE(read(api).has_value());
        EXPECT_FALSE(api.opened.empty());
        EXPECT_TRUE(api.allDescriptorsClosed()) << request;
    }
}

TEST_F(V4l2CameraBackendTest, UsesOnlyQueryIoctls)
{
    FakeV4l2Api api = brioWithSysfs();

    ASSERT_TRUE(read(api).has_value());

    const std::set<unsigned long> allowed{
        VIDIOC_QUERYCAP,       VIDIOC_ENUM_FMT,  VIDIOC_ENUM_FRAMESIZES, VIDIOC_ENUM_FRAMEINTERVALS,
        VIDIOC_QUERY_EXT_CTRL, VIDIOC_QUERYMENU, VIDIOC_G_CTRL,
    };
    const std::set<unsigned long> used(api.requests.begin(), api.requests.end());
    EXPECT_TRUE(std::ranges::includes(allowed, used));
    EXPECT_EQ(used, allowed); // e todos eles foram exercitados contra a Brio 100
}

} // namespace

namespace {

std::vector<std::string> firmwareManagedNames(const ghub::camera::CameraDescription& description)
{
    std::vector<std::string> names;
    for (const auto& control : description.controls) {
        if (control.firmwareManaged()) {
            names.push_back(control.name());
        }
    }
    return names;
}

} // namespace

TEST_F(V4l2CameraBackendTest, MarksGainAsFirmwareManaged_WhenExposureIsAutomatic)
{
    FakeV4l2Api api = brioWithSysfs(); // Auto Exposure = 3 (Aperture Priority)

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_EQ(firmwareManagedNames(**result), std::vector<std::string>{"Gain"});
}

TEST_F(V4l2CameraBackendTest, DoesNotMarkGain_WhenExposureIsManual)
{
    FakeV4l2Api api = brioWithSysfs();
    std::ranges::find(api.nodes[0].controls, V4L2_CID_EXPOSURE_AUTO, &ghub::camera::testing::FakeControl::id)->value =
        V4L2_EXPOSURE_MANUAL;

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_TRUE(firmwareManagedNames(**result).empty());
}

TEST_F(V4l2CameraBackendTest, DoesNotMarkGain_WhenThereIsNoExposureControl)
{
    FakeV4l2Api api = brioWithSysfs();
    std::erase_if(api.nodes[0].controls, [](const auto& control) { return control.id == V4L2_CID_EXPOSURE_AUTO; });

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_NE(findControl(**result, "Gain"), nullptr);
    EXPECT_TRUE(firmwareManagedNames(**result).empty());
}

TEST_F(V4l2CameraBackendTest, DoesNotMarkGain_WhenExposureValueCannotBeRead)
{
    FakeV4l2Api api = brioWithSysfs();
    api.controlValueErrors[V4L2_CID_EXPOSURE_AUTO] = -EIO;

    const auto result = read(api);

    ASSERT_TRUE(result.has_value());
    if (!result->has_value()) {
        FAIL() << "câmera não encontrada";
    }
    EXPECT_TRUE(firmwareManagedNames(**result).empty());
}
