#include "modules/camera/infrastructure/V4l2VideoCapture.hpp"

#include "FakeV4l2Api.hpp"
#include "TempDir.hpp"

#include <gtest/gtest.h>

#include <linux/videodev2.h>
#include <poll.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::PreviewMode;
using ghub::camera::V4l2VideoCapture;
using ghub::camera::VideoCaptureSession;
using ghub::camera::testing::FakeFrame;
using ghub::camera::testing::FakeV4l2Api;
using ghub::camera::testing::TempDir;
using ghub::device_inventory::UsbId;
using ghub::shared_kernel::ErrorCode;
namespace fixtures = ghub::camera::testing::fixtures;
namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

constexpr UsbId brioId{.vendorId = 0x046d, .productId = 0x094c};
constexpr auto brioModalias = "usb:v046Dp094Cd9915dcEFdsc02dp01ic0Eisc01ip00in00\n";
constexpr auto otherModalias = "usb:v046Dp0AFEd0100dc00dsc00dp00ic01isc01ip00in00\n";

PreviewMode mjpg720p()
{
    return {
        .format = PixelFormat::create("MJPG").value(),
        .width = 1280,
        .height = 720,
        .frameRate = FrameRate::create(30, 1).value(),
    };
}

std::vector<std::byte> bytesOf(std::initializer_list<unsigned> values)
{
    std::vector<std::byte> bytes;
    for (const unsigned value : values) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

class V4l2VideoCaptureTest : public ::testing::Test {
protected:
    // Cria <sysfs>/<name>/device/modalias.
    void addSysfsEntry(const std::string& name, const std::string& modalias = brioModalias)
    {
        const fs::path directory = sysfs_.path() / name / "device";
        fs::create_directories(directory);
        std::ofstream(directory / "modalias") << modalias;
    }

    // Sysfs e fake de uma Brio 100 com captura em video0 e metadados em video1.
    void SetUp() override
    {
        addSysfsEntry("video0");
        addSysfsEntry("video1");
        api_ = fixtures::brio100();
    }

    ghub::shared_kernel::Result<std::unique_ptr<VideoCaptureSession>> start(const PreviewMode& mode = mjpg720p())
    {
        V4l2VideoCapture capture{brioId, api_, sysfs_.path()};
        return capture.start(mode);
    }

    // Inicia com sucesso (falha o teste se não conseguir).
    std::unique_ptr<VideoCaptureSession> startOk()
    {
        auto result = start();
        EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().detail);
        return result.has_value() ? std::move(*result) : nullptr;
    }

    TempDir sysfs_;
    FakeV4l2Api api_;
};

TEST_F(V4l2VideoCaptureTest, StartsStreaming_WithRequestedMode)
{
    const auto session = startOk();

    ASSERT_NE(session, nullptr);
    if (!api_.lastSetFormat.has_value()) {
        FAIL() << "S_FMT não foi chamado";
    }
    const auto& format = *api_.lastSetFormat;
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): v4l2_format é uma união do uAPI do kernel.
    EXPECT_EQ(format.type, static_cast<std::uint32_t>(V4L2_BUF_TYPE_VIDEO_CAPTURE));
    EXPECT_EQ(format.fmt.pix.pixelformat, V4L2_PIX_FMT_MJPEG);
    EXPECT_EQ(format.fmt.pix.width, 1280U);
    EXPECT_EQ(format.fmt.pix.height, 720U);
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)
    EXPECT_TRUE(api_.streaming);
    EXPECT_EQ(api_.mappings.size(), 4U);
    EXPECT_EQ(api_.queuedIndexes, (std::vector<std::uint32_t>{0, 1, 2, 3}));
    // Só o nó de captura é aberto para streaming; o de metadados nem chega a ser aberto.
    ASSERT_EQ(api_.streamingOpened.size(), 1U);
    EXPECT_EQ(api_.pathOf(api_.streamingOpened.front()), "/dev/video0");
    for (const int fd : api_.opened) {
        EXPECT_EQ(api_.pathOf(fd), "/dev/video0");
    }
}

TEST_F(V4l2VideoCaptureTest, IssuesIoctlsInExpectedOrder)
{
    const auto session = startOk();

    const std::vector<unsigned long> expected{
        VIDIOC_QUERYCAP, VIDIOC_S_FMT,    VIDIOC_REQBUFS, VIDIOC_QUERYBUF, VIDIOC_QBUF, VIDIOC_QUERYBUF,
        VIDIOC_QBUF,     VIDIOC_QUERYBUF, VIDIOC_QBUF,    VIDIOC_QUERYBUF, VIDIOC_QBUF, VIDIOC_STREAMON,
    };
    EXPECT_EQ(api_.requests, expected);
}

TEST_F(V4l2VideoCaptureTest, UsesOnlyStreamingIoctls)
{
    {
        const auto session = startOk();
    }

    const std::set<unsigned long> allowed{
        VIDIOC_QUERYCAP, VIDIOC_S_FMT, VIDIOC_REQBUFS,  VIDIOC_QUERYBUF,
        VIDIOC_QBUF,     VIDIOC_DQBUF, VIDIOC_STREAMON, VIDIOC_STREAMOFF,
    };
    for (const unsigned long request : api_.requests) {
        EXPECT_TRUE(allowed.contains(request)) << "ioctl fora da lista: " << request;
    }
    EXPECT_FALSE(std::ranges::contains(api_.requests, static_cast<unsigned long>(VIDIOC_S_PARM)));
    EXPECT_FALSE(std::ranges::contains(api_.requests, static_cast<unsigned long>(VIDIOC_S_CTRL)));
    EXPECT_FALSE(std::ranges::contains(api_.requests, static_cast<unsigned long>(VIDIOC_S_EXT_CTRLS)));
}

// --- falhas ao iniciar ---

TEST_F(V4l2VideoCaptureTest, FailsNotDetected_WhenNoCaptureNode)
{
    fs::remove_all(sysfs_.path());
    addSysfsEntry("video0", otherModalias);

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("câmera não detectada"), std::string::npos) << result.error().detail;
    EXPECT_TRUE(api_.streamingOpened.empty());
}

TEST_F(V4l2VideoCaptureTest, FailsNotDetected_WhenOnlyMetadataNode)
{
    fs::remove_all(sysfs_.path());
    addSysfsEntry("video1");

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().detail.find("câmera não detectada"), std::string::npos) << result.error().detail;
    EXPECT_TRUE(api_.streamingOpened.empty());
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2VideoCaptureTest, IgnoresMetadataNode)
{
    api_ = fixtures::brio100("/dev/video1", "/dev/video0"); // metadados em video0, captura em video1

    const auto session = startOk();

    ASSERT_NE(session, nullptr);
    ASSERT_EQ(api_.streamingOpened.size(), 1U);
    EXPECT_EQ(api_.pathOf(api_.streamingOpened.front()), "/dev/video1");
}

TEST_F(V4l2VideoCaptureTest, FailsUnavailable_WhenOpenDenied)
{
    api_.streamingOpenErrors["/dev/video0"] = -EACCES;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("Permission denied"), std::string::npos) << result.error().detail;
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2VideoCaptureTest, FailsUnavailable_WhenProbeOpenDenied)
{
    api_.openErrors["/dev/video0"] = -EACCES;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("Permission denied"), std::string::npos) << result.error().detail;
    EXPECT_TRUE(api_.streamingOpened.empty());
}

TEST_F(V4l2VideoCaptureTest, FailsBusy_WhenOpenReportsBusy)
{
    api_.streamingOpenErrors["/dev/video0"] = -EBUSY;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
}

TEST_F(V4l2VideoCaptureTest, FailsBusy_OnSetFormatBusy)
{
    api_.requestErrors[VIDIOC_S_FMT] = -EBUSY;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
}

TEST_F(V4l2VideoCaptureTest, FailsBusy_OnRequestBuffersBusy)
{
    api_.requestErrors[VIDIOC_REQBUFS] = -EBUSY;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
}

TEST_F(V4l2VideoCaptureTest, FailsBusy_OnStreamOnBusy)
{
    api_.requestErrors[VIDIOC_STREAMON] = -EBUSY;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
}

TEST_F(V4l2VideoCaptureTest, FailsBusy_WhenAnotherProgramOwnsTheQueue)
{
    api_.queueBusy = true;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_FALSE(api_.streaming);
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenDriverAdjustsSize)
{
    api_.nodes[0].formats.resize(2);
    api_.nodes[0].formats[1].sizes.resize(1); // MJPG só em 640x480 (primeiro tamanho da tabela)

    const auto result = start(); // pede MJPG 1280x720; o driver devolve MJPG 640x480 sem erro

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("não aceitou o modo"), std::string::npos) << result.error().detail;
    EXPECT_FALSE(api_.streaming);
    EXPECT_TRUE(api_.mappings.empty());
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenDriverAdjustsFourcc)
{
    api_.nodes[0].formats.erase(api_.nodes[0].formats.begin() + 1); // sobra só YUYV

    const auto result = start(); // pede MJPG; o driver devolve YUYV sem erro

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("não aceitou o modo"), std::string::npos) << result.error().detail;
    EXPECT_FALSE(api_.streaming);
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenDriverAdjustsOnlyWidth)
{
    api_.forcedReplyWidth = 1281; // FourCC e altura iguais ao pedido

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("não aceitou o modo"), std::string::npos) << result.error().detail;
    EXPECT_FALSE(api_.streaming);
    EXPECT_TRUE(api_.mappings.empty());
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenDriverAdjustsOnlyHeight)
{
    api_.forcedReplyHeight = 721; // FourCC e largura iguais ao pedido

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("não aceitou o modo"), std::string::npos) << result.error().detail;
    EXPECT_FALSE(api_.streaming);
    EXPECT_TRUE(api_.mappings.empty());
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenDriverAdjustsOnlyFourcc)
{
    api_.forcedReplyPixelFormat = V4L2_PIX_FMT_YUYV; // tamanho igual ao pedido

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("não aceitou o modo"), std::string::npos) << result.error().detail;
    EXPECT_FALSE(api_.streaming);
    EXPECT_TRUE(api_.mappings.empty());
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenFewerThanTwoBuffersGranted)
{
    for (const std::uint32_t granted : {0U, 1U}) {
        api_ = fixtures::brio100();
        api_.grantedBuffers = granted;

        const auto result = start();

        ASSERT_FALSE(result.has_value()) << granted;
        EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable) << granted;
        EXPECT_FALSE(api_.streaming);
        EXPECT_TRUE(api_.mappings.empty());
    }
}

TEST_F(V4l2VideoCaptureTest, FailsMalformed_WhenTooManyBuffersGranted)
{
    api_.grantedBuffers = 33;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(api_.mappings.empty());
}

TEST_F(V4l2VideoCaptureTest, FailsMalformed_OnZeroLengthBuffer)
{
    api_.reportedLengths[1] = 0;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_FALSE(api_.streaming);
}

TEST_F(V4l2VideoCaptureTest, FailsMalformed_OnOversizedBuffer)
{
    api_.reportedLengths[0] = (64U * 1024U * 1024U) + 1U;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(api_.mappings.empty());
}

TEST_F(V4l2VideoCaptureTest, FailsMalformed_OnInconsistentBufferIndex)
{
    api_.reportedIndexes[2] = 7;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenMapFails_AndUnmapsPreviousBuffers)
{
    api_.mapErrors[2] = -ENOMEM;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("Cannot allocate memory"), std::string::npos) << result.error().detail;
    ASSERT_EQ(api_.mappings.size(), 2U);
    EXPECT_EQ(api_.unmapped.size(), 2U);
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2VideoCaptureTest, Fails_WhenQueueBufferFails)
{
    api_.queueBufferErrors[1] = -EIO;

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("VIDIOC_QBUF"), std::string::npos) << result.error().detail;
}

// Cada passo de start() que pode falhar: nada fica mapeado, ligado ou aberto depois da falha.
struct FailingStep {
    const char* name;
    void (*inject)(FakeV4l2Api&);
};

class V4l2VideoCaptureCleanupTest : public V4l2VideoCaptureTest, public ::testing::WithParamInterface<FailingStep> {};

TEST_P(V4l2VideoCaptureCleanupTest, CleansUp_OnEveryStartFailure)
{
    GetParam().inject(api_);

    const auto result = start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(api_.unmapped.size(), api_.mappings.size());
    EXPECT_FALSE(api_.streaming);
    EXPECT_EQ(api_.streamOffCalls, 0); // STREAMON é o último passo: se falhou, nunca ligou
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

INSTANTIATE_TEST_SUITE_P(
    EveryStep, V4l2VideoCaptureCleanupTest,
    ::testing::Values(FailingStep{"SetFormat", [](FakeV4l2Api& api) { api.requestErrors[VIDIOC_S_FMT] = -EIO; }},
                      FailingStep{"RequestBuffers", [](FakeV4l2Api& api) { api.requestErrors[VIDIOC_REQBUFS] = -EIO; }},
                      FailingStep{"QueryBuffer", [](FakeV4l2Api& api) { api.requestErrors[VIDIOC_QUERYBUF] = -EIO; }},
                      FailingStep{"Map", [](FakeV4l2Api& api) { api.mapErrors[3] = -ENOMEM; }},
                      FailingStep{"QueueBuffer", [](FakeV4l2Api& api) { api.queueBufferErrors[2] = -EIO; }},
                      FailingStep{"StreamOn", [](FakeV4l2Api& api) { api.requestErrors[VIDIOC_STREAMON] = -EIO; }},
                      FailingStep{"AdjustedFormat", [](FakeV4l2Api& api) { api.nodes[0].formats.resize(1); }},
                      FailingStep{"BadLength", [](FakeV4l2Api& api) { api.reportedLengths[2] = 0; }}),
    [](const ::testing::TestParamInfo<FailingStep>& step) { return std::string{step.param.name}; });

// --- leitura de quadros ---

class V4l2VideoCaptureReadTest : public V4l2VideoCaptureTest {
protected:
    void SetUp() override
    {
        V4l2VideoCaptureTest::SetUp();
        session_ = startOk();
        ASSERT_NE(session_, nullptr);
    }

    // Quadro bom de `count` bytes com o valor `fill`.
    static FakeFrame goodFrame(std::size_t count, std::byte fill = std::byte{0x5A})
    {
        FakeFrame frame;
        frame.bytes.assign(count, fill);
        return frame;
    }

    std::unique_ptr<VideoCaptureSession> session_;
    std::vector<std::byte> frame_;
};

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_CopiesBytesUsed_AndRequeuesBuffer)
{
    api_.frames.push_back({.bytes = bytesOf({1, 2, 3, 4, 5}), .flags = 0, .bytesUsed = 3, .index = std::nullopt});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
    EXPECT_EQ(frame_, bytesOf({1, 2, 3}));
    // Os 4 QBUF do início e o do buffer 0, devolvido ao driver depois da cópia.
    EXPECT_EQ(api_.queuedIndexes, (std::vector<std::uint32_t>{0, 1, 2, 3, 0}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_DeliversFramesInOrder)
{
    api_.frames.push_back(goodFrame(2, std::byte{0xA1}));
    api_.frames.push_back(goodFrame(3, std::byte{0xB2}));

    const auto first = session_->readFrame(100ms, frame_);
    const std::vector<std::byte> firstBytes = frame_;
    const auto second = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(first.has_value() && *first);
    ASSERT_TRUE(second.has_value() && *second);
    EXPECT_EQ(firstBytes, std::vector<std::byte>(2, std::byte{0xA1}));
    EXPECT_EQ(frame_, std::vector<std::byte>(3, std::byte{0xB2}));
    EXPECT_EQ(api_.queuedIndexes, (std::vector<std::uint32_t>{0, 1, 2, 3, 0, 1}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_ReplacesPreviousContent)
{
    frame_ = std::vector<std::byte>(10, std::byte{0xEE});
    api_.frames.push_back({.bytes = bytesOf({7, 8}), .flags = 0, .bytesUsed = std::nullopt, .index = std::nullopt});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(result.has_value() && *result);
    EXPECT_EQ(frame_, bytesOf({7, 8}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_ReturnsFalse_OnTimeout)
{
    frame_ = bytesOf({9, 9});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
    EXPECT_EQ(frame_, bytesOf({9, 9}));
    EXPECT_EQ(api_.queuedIndexes.size(), 4U);
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_ReturnsFalse_OnEagain)
{
    frame_ = bytesOf({9});
    api_.pollResults.push_back(POLLIN); // o poll acorda, mas o driver não tem buffer pronto

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
    EXPECT_EQ(frame_, bytesOf({9}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_DiscardsErrorFlaggedFrame_AndRequeues)
{
    frame_ = bytesOf({9});
    api_.frames.push_back(
        {.bytes = bytesOf({1, 2, 3}), .flags = V4L2_BUF_FLAG_ERROR, .bytesUsed = std::nullopt, .index = std::nullopt});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
    EXPECT_EQ(frame_, bytesOf({9}));
    EXPECT_EQ(api_.queuedIndexes, (std::vector<std::uint32_t>{0, 1, 2, 3, 0}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_DiscardsEmptyFrame)
{
    frame_ = bytesOf({9});
    api_.frames.push_back({.bytes = bytesOf({1, 2, 3}), .flags = 0, .bytesUsed = 0, .index = std::nullopt});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
    EXPECT_EQ(frame_, bytesOf({9}));
    EXPECT_EQ(api_.queuedIndexes, (std::vector<std::uint32_t>{0, 1, 2, 3, 0}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_DiscardsFrameLargerThanBuffer)
{
    frame_ = bytesOf({9});
    api_.frames.push_back(
        {.bytes = bytesOf({1}), .flags = 0, .bytesUsed = api_.bufferLength + 1, .index = std::nullopt});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
    EXPECT_EQ(frame_, bytesOf({9}));
    EXPECT_EQ(api_.queuedIndexes, (std::vector<std::uint32_t>{0, 1, 2, 3, 0}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_FailsMalformed_OnBufferIndexOutOfRange)
{
    frame_ = bytesOf({9});
    api_.frames.push_back({.bytes = bytesOf({1, 2}), .flags = 0, .bytesUsed = std::nullopt, .index = 9});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(frame_, bytesOf({9}));
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_FailsMalformed_OnBufferIndexEqualToBufferCount)
{
    ASSERT_EQ(api_.mappings.size(), 4U);
    frame_ = bytesOf({9});
    api_.frames.push_back({.bytes = bytesOf({1, 2}), .flags = 0, .bytesUsed = std::nullopt, .index = 4});

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(frame_, bytesOf({9}));
    EXPECT_EQ(api_.queuedIndexes, (std::vector<std::uint32_t>{0, 1, 2, 3})); // nada foi devolvido ao driver
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_Fails_OnPollError)
{
    api_.pollResults.push_back(-EIO);

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("poll"), std::string::npos) << result.error().detail;
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_Fails_OnPollHangupWithoutData)
{
    api_.pollResults.push_back(POLLERR | POLLHUP);

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("encerrado"), std::string::npos) << result.error().detail;
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_Fails_OnDequeueEnodev)
{
    api_.frames.push_back(goodFrame(4));
    api_.requestErrors[VIDIOC_DQBUF] = -ENODEV;

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("VIDIOC_DQBUF"), std::string::npos) << result.error().detail;
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_Fails_WhenRequeueFails)
{
    api_.frames.push_back(goodFrame(4));
    api_.queueBufferErrors[4] = -EIO; // o QBUF depois da cópia (os quatro primeiros foram do start)

    const auto result = session_->readFrame(100ms, frame_);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("VIDIOC_QBUF"), std::string::npos) << result.error().detail;
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_KeepsFailing_AfterError_WithoutTouchingDevice)
{
    api_.requestErrors[VIDIOC_DQBUF] = -ENODEV;
    api_.frames.push_back(goodFrame(4));
    ASSERT_FALSE(session_->readFrame(100ms, frame_).has_value());
    const std::size_t requestsAfterError = api_.requests.size();
    const std::size_t pollsAfterError = api_.pollTimeouts.size();

    const auto again = session_->readFrame(100ms, frame_);

    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(api_.requests.size(), requestsAfterError);
    EXPECT_EQ(api_.pollTimeouts.size(), pollsAfterError);
}

TEST_F(V4l2VideoCaptureReadTest, ReadFrame_PassesTimeoutToPoll)
{
    const auto result = session_->readFrame(250ms, frame_);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(api_.pollTimeouts.size(), 1U);
    EXPECT_EQ(api_.pollTimeouts.front(), 250ms);
}

// --- fim da sessão ---

TEST_F(V4l2VideoCaptureTest, SessionDestruction_StopsStream_UnmapsAll_ClosesDescriptor)
{
    auto session = startOk();
    ASSERT_NE(session, nullptr);
    std::set<const std::byte*> mapped;
    for (const auto& mapping : api_.mappings) {
        mapped.insert(mapping.address);
    }

    session.reset();

    EXPECT_FALSE(api_.streaming);
    EXPECT_EQ(api_.streamOffCalls, 1);
    EXPECT_EQ(api_.requests.back(), static_cast<unsigned long>(VIDIOC_STREAMOFF)); // STREAMOFF antes do munmap/close
    EXPECT_EQ(api_.unmapped.size(), 4U);
    EXPECT_EQ((std::set<const std::byte*>{api_.unmapped.begin(), api_.unmapped.end()}), mapped);
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2VideoCaptureTest, SessionDestruction_IgnoresStreamOffError)
{
    api_.requestErrors[VIDIOC_STREAMOFF] = -EIO;
    auto session = startOk();
    ASSERT_NE(session, nullptr);

    session.reset();

    EXPECT_EQ(api_.unmapped.size(), 4U);
    EXPECT_TRUE(api_.allDescriptorsClosed());
}

TEST_F(V4l2VideoCaptureTest, SecondStart_AfterSessionDestroyed_Works)
{
    V4l2VideoCapture capture{brioId, api_, sysfs_.path()};
    {
        const auto first = capture.start(mjpg720p());
        ASSERT_TRUE(first.has_value());
    }
    ASSERT_FALSE(api_.streaming);

    const auto second = capture.start(mjpg720p());

    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(api_.streaming);
    EXPECT_EQ(api_.mappings.size(), 8U);
}

} // namespace
