#include "modules/camera/api/CameraSettings.hpp"

#include "fakes/FakeCameraBackend.hpp"
#include "fakes/FakeCameraControlAccess.hpp"
#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <vector>

using namespace std::chrono_literals;
using ghub::camera::CameraControlSpec;
using ghub::camera::CameraControlValue;
using ghub::camera::CameraDescription;
using ghub::camera::CameraSettings;
using ghub::camera::ControlKind;
using ghub::camera::SetCameraControl;
using ghub::device_access::AccessRegistry;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;

namespace {

constexpr std::uint32_t brightnessId = 0x00980900;
constexpr std::uint32_t contrastId = 0x00980901;
constexpr std::uint32_t inactiveId = 0x009a0902;
constexpr std::uint32_t readOnlyId = 0x009a0903;
constexpr std::uint32_t noValueId = 0x009a0904;

PhysicalInterfaceId cameraInterface()
{
    return PhysicalInterfaceId::create("v4l2:046d:094c").value();
}

CameraControlSpec spec(std::uint32_t id, std::string name, std::int64_t current)
{
    return {.id = id,
            .name = std::move(name),
            .kind = ControlKind::Integer,
            .minimum = 0,
            .maximum = 255,
            .step = 1,
            .defaultValue = 128,
            .currentValue = current,
            .inactive = false,
            .readOnly = false,
            .options = {}};
}

class CameraSettingsTest : public ::testing::Test {
protected:
    CameraSettingsTest()
    {
        // Fora de ordem de propósito: a fachada ordena por id crescente.
        auto inactive = spec(inactiveId, "Exposure Time, Absolute", 156);
        inactive.inactive = true;
        auto readOnly = spec(readOnlyId, "Sensor", 5);
        readOnly.readOnly = true;
        auto noValue = spec(noValueId, "Sem valor", 0);
        noValue.currentValue = std::nullopt;
        access.controls = {spec(contrastId, "Contrast", 40), inactive, readOnly, noValue,
                           spec(brightnessId, "Brightness", 100)};
        setBackendControls();
    }

    void setBackendControls()
    {
        CameraDescription description{.name = "Brio 100", .modes = {}, .controls = {}};
        for (const auto& controlSpec : access.controls) {
            description.controls.push_back(ghub::camera::CameraControl::create(controlSpec).value());
        }
        backend.setResult(std::optional<CameraDescription>{description});
    }

    ghub::device_access::DeviceLease takeLeaseAsAnotherHolder()
    {
        return registry
            .acquire(LeaseRequest{.interfaceId = cameraInterface(),
                                  .holder = LeaseHolder::create("outro.modulo").value(),
                                  .purpose = "teste",
                                  .maxDuration = 60s})
            .value();
    }

    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeCameraControlAccess access;
    ghub::fakes::FakeCameraBackend backend;
    SetCameraControl setCameraControl{access, registry, cameraInterface()};
    CameraSettings settings{backend, setCameraControl};
};

} // namespace

TEST_F(CameraSettingsTest, CurrentValues_FailsWithBackendUnavailable_WhenCameraIsAbsent)
{
    backend.setResult(std::optional<CameraDescription>{});

    const auto result = settings.currentValues();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "câmera não detectada");
}

TEST_F(CameraSettingsTest, CurrentValues_ForwardsTheBackendError)
{
    backend.setResult(std::unexpected(DomainError{ErrorCode::MalformedData, "lixo"}));

    const auto result = settings.currentValues();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(result.error().detail, "lixo");
}

TEST_F(CameraSettingsTest, CurrentValues_ReturnsOnlyWritableActiveControlsWithValue_SortedById)
{
    const auto result = settings.currentValues();

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    const std::vector<CameraControlValue> expected{{brightnessId, "Brightness", 100}, {contrastId, "Contrast", 40}};
    EXPECT_EQ(*result, expected);
    EXPECT_EQ(backend.calls(), 1);
}

TEST_F(CameraSettingsTest, CurrentValues_OmitsFirmwareManagedControls)
{
    auto gain = spec(0x00980913, "Gain", 0);
    gain.firmwareManaged = true;
    access.controls.push_back(gain);
    setBackendControls();

    const auto result = settings.currentValues();

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    const std::vector<CameraControlValue> expected{{brightnessId, "Brightness", 100}, {contrastId, "Contrast", 40}};
    EXPECT_EQ(*result, expected);
}

TEST_F(CameraSettingsTest, CurrentValues_ReadsTheBackendEveryTime)
{
    ASSERT_TRUE(settings.currentValues().has_value());
    ASSERT_TRUE(settings.currentValues().has_value());

    EXPECT_EQ(backend.calls(), 2);
}

TEST_F(CameraSettingsTest, Apply_WritesAndReturnsTheRereadValue)
{
    const auto result = settings.apply(brightnessId, 200);

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    EXPECT_EQ(*result, 200);
    ASSERT_EQ(access.writes.size(), 1U);
    EXPECT_EQ(access.writes[0].id, brightnessId);
    EXPECT_EQ(access.writes[0].value, 200);
}

TEST_F(CameraSettingsTest, Apply_FailsWithMalformedData_WhenRereadHasNoCurrentValue)
{
    access.dropValueAfterWrite = true;

    const auto result = settings.apply(brightnessId, 90);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(result.error().detail, "controle sem valor após a escrita");
    ASSERT_EQ(access.writes.size(), 1U);
}

TEST_F(CameraSettingsTest, Apply_RejectsOutOfRangeValueWithoutWriting)
{
    const auto result = settings.apply(brightnessId, 256);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(CameraSettingsTest, Apply_FailsWithDeviceBusy_WhenAnotherHolderHasTheLease)
{
    const auto lease = takeLeaseAsAnotherHolder();

    const auto result = settings.apply(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(access.writes.empty());
}
