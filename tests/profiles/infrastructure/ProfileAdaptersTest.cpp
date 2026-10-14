#include "modules/profiles/infrastructure/CameraSettingsAdapter.hpp"
#include "modules/profiles/infrastructure/EqualizerSettingsAdapter.hpp"

#include "fakes/FakeCameraBackend.hpp"
#include "fakes/FakeCameraControlAccess.hpp"
#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <optional>
#include <vector>

using namespace std::chrono_literals;
using ghub::camera::CameraControlSpec;
using ghub::camera::CameraDescription;
using ghub::camera::CameraSettings;
using ghub::camera::ControlKind;
using ghub::camera::SetCameraControl;
using ghub::device_access::AccessRegistry;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::profiles::CameraControlSetting;
using ghub::profiles::CameraSettingsAdapter;
using ghub::profiles::EqualizerSettingsAdapter;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;

namespace {

constexpr std::uint32_t brightnessId = 0x00980900;
constexpr std::uint32_t contrastId = 0x00980901;

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

class CameraSettingsAdapterTest : public ::testing::Test {
protected:
    CameraSettingsAdapterTest()
    {
        access.controls = {spec(contrastId, "Contrast", 40), spec(brightnessId, "Brightness", 100)};
        CameraDescription description{.name = "Brio 100", .modes = {}, .controls = {}};
        for (const auto& controlSpec : access.controls) {
            description.controls.push_back(ghub::camera::CameraControl::create(controlSpec).value());
        }
        backend.setResult(std::optional<CameraDescription>{description});
    }

    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeCameraControlAccess access;
    ghub::fakes::FakeCameraBackend backend;
    SetCameraControl setCameraControl{access, registry, cameraInterface()};
    CameraSettings settings{backend, setCameraControl};
    CameraSettingsAdapter adapter{settings};
};

class FakeCurve final : public ghub::audio_processing::EqualizerCurveAccess {
public:
    std::array<int, 10> gains{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    ghub::shared_kernel::Result<void> result{};
    std::vector<std::array<int, 10>> requested;

    std::array<int, 10> currentGains() const override { return gains; }

    ghub::shared_kernel::Result<void> requestGains(const std::array<int, 10>& wanted) override
    {
        requested.push_back(wanted);
        return result;
    }
};

} // namespace

TEST_F(CameraSettingsAdapterTest, ReadCurrent_ConvertsValuesToProfileSettings)
{
    const auto result = adapter.readCurrent();

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    const std::vector<CameraControlSetting> expected{{brightnessId, "Brightness", 100}, {contrastId, "Contrast", 40}};
    EXPECT_EQ(*result, expected);
}

TEST_F(CameraSettingsAdapterTest, ReadCurrent_ForwardsTheError)
{
    backend.setResult(std::optional<CameraDescription>{});

    const auto result = adapter.readCurrent();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
}

TEST_F(CameraSettingsAdapterTest, Apply_WritesAndReturnsTheRereadValue)
{
    const auto result = adapter.apply(brightnessId, 200);

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    EXPECT_EQ(*result, 200);
    ASSERT_EQ(access.writes.size(), 1U);
    EXPECT_EQ(access.writes[0].id, brightnessId);
}

TEST_F(CameraSettingsAdapterTest, Apply_ForwardsDeviceBusy)
{
    const auto lease = registry
                           .acquire(LeaseRequest{.interfaceId = cameraInterface(),
                                                 .holder = LeaseHolder::create("outro.modulo").value(),
                                                 .purpose = "teste",
                                                 .maxDuration = 60s})
                           .value();

    const auto result = adapter.apply(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
}

TEST(EqualizerSettingsAdapterTest, CurrentGains_ReadsFromTheOwner)
{
    FakeCurve curve;
    EqualizerSettingsAdapter adapter{curve};

    EXPECT_EQ(adapter.currentGains(), curve.gains);
}

TEST(EqualizerSettingsAdapterTest, RequestGains_ForwardsGainsAndSuccess)
{
    FakeCurve curve;
    EqualizerSettingsAdapter adapter{curve};
    const std::array<int, 10> wanted{-1, 0, 1, 2, 3, 4, 5, 6, 7, 8};

    EXPECT_TRUE(adapter.requestGains(wanted).has_value());
    ASSERT_EQ(curve.requested.size(), 1U);
    EXPECT_EQ(curve.requested[0], wanted);
}

TEST(EqualizerSettingsAdapterTest, RequestGains_ForwardsTheOwnerError)
{
    FakeCurve curve;
    curve.result = ghub::shared_kernel::fail(ErrorCode::InvalidArgument, "fora da faixa");
    EqualizerSettingsAdapter adapter{curve};

    const auto result = adapter.requestGains({});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), (DomainError{ErrorCode::InvalidArgument, "fora da faixa"}));
}
