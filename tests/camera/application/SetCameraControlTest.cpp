#include "modules/camera/application/SetCameraControl.hpp"

#include "fakes/FakeCameraControlAccess.hpp"
#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

using namespace std::chrono_literals;
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
constexpr std::uint32_t exposureId = 0x009a0901;
constexpr std::uint32_t inactiveId = 0x009a0902;
constexpr std::uint32_t readOnlyId = 0x009a0903;

PhysicalInterfaceId cameraInterface()
{
    return PhysicalInterfaceId::create("v4l2:046d:094c").value();
}

class SetCameraControlTest : public ::testing::Test {
protected:
    SetCameraControlTest()
    {
        access.controls = {
            {
                .id = brightnessId,
                .name = "Brightness",
                .kind = ControlKind::Integer,
                .minimum = 0,
                .maximum = 255,
                .step = 1,
                .defaultValue = 128,
                .currentValue = 100,
                .inactive = false,
                .readOnly = false,
                .options = {},
            },
            {
                .id = exposureId,
                .name = "Auto Exposure",
                .kind = ControlKind::Menu,
                .minimum = 0,
                .maximum = 3,
                .step = 1,
                .defaultValue = 3,
                .currentValue = 3,
                .inactive = false,
                .readOnly = false,
                .options = {{.value = 1, .label = "Manual Mode"}, {.value = 3, .label = "Aperture Priority Mode"}},
            },
            {
                .id = inactiveId,
                .name = "Exposure Time, Absolute",
                .kind = ControlKind::Integer,
                .minimum = 5,
                .maximum = 2500,
                .step = 1,
                .defaultValue = 156,
                .currentValue = 156,
                .inactive = true,
                .readOnly = false,
                .options = {},
            },
            {
                .id = readOnlyId,
                .name = "Sensor",
                .kind = ControlKind::Integer,
                .minimum = 0,
                .maximum = 10,
                .step = 1,
                .defaultValue = 5,
                .currentValue = 5,
                .inactive = false,
                .readOnly = true,
                .options = {},
            },
        };
    }

    // Um lease de outro dono na mesma interface.
    ghub::device_access::DeviceLease takeLeaseAsAnotherHolder()
    {
        return registry
            .acquire(LeaseRequest{
                .interfaceId = cameraInterface(),
                .holder = LeaseHolder::create("outro.modulo").value(),
                .purpose = "teste",
                .maxDuration = 60s,
            })
            .value();
    }

    // Depois da operação a interface tem de estar livre.
    void expectInterfaceFree()
    {
        EXPECT_TRUE(registry
                        .acquire(LeaseRequest{.interfaceId = cameraInterface(),
                                              .holder = LeaseHolder::create("verificador").value(),
                                              .purpose = "teste",
                                              .maxDuration = 1s})
                        .has_value());
    }

    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeCameraControlAccess access;
    SetCameraControl setCameraControl{access, registry, cameraInterface()};
};

} // namespace

TEST_F(SetCameraControlTest, WritesValueAndReturnsTheRereadControl)
{
    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    EXPECT_EQ(result->id(), brightnessId);
    EXPECT_EQ(result->currentValue(), std::optional<std::int64_t>{200});
    ASSERT_EQ(access.writes.size(), 1U);
    EXPECT_EQ(access.writes[0].id, brightnessId);
    EXPECT_EQ(access.writes[0].value, 200);
}

TEST_F(SetCameraControlTest, ReadsBeforeAndAfterTheWrite)
{
    ASSERT_TRUE(setCameraControl.setValue(brightnessId, 200).has_value());

    EXPECT_EQ(access.reads, 2U);
    EXPECT_EQ(access.opens, 1U);
}

TEST_F(SetCameraControlTest, RestoreDefaultWritesTheDriverDefault)
{
    const auto result = setCameraControl.restoreDefault(brightnessId);

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    ASSERT_EQ(access.writes.size(), 1U);
    EXPECT_EQ(access.writes[0].value, 128);
    EXPECT_EQ(result->currentValue(), std::optional<std::int64_t>{128});
}

TEST_F(SetCameraControlTest, RestoreDefaultWritesNothing_WhenControlIsInactive)
{
    const auto result = setCameraControl.restoreDefault(inactiveId);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, RejectsInvalidValueWithoutWriting)
{
    for (const std::int64_t value : {-1, 256}) {
        const auto result = setCameraControl.setValue(brightnessId, value);

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    }
    const auto menu = setCameraControl.setValue(exposureId, 2); // item que o menu não enumera
    ASSERT_FALSE(menu.has_value());
    EXPECT_EQ(menu.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, RejectsInactiveAndReadOnlyControlsWithoutWriting)
{
    const auto inactive = setCameraControl.setValue(inactiveId, 200);
    const auto readOnly = setCameraControl.setValue(readOnlyId, 3);

    ASSERT_FALSE(inactive.has_value());
    EXPECT_EQ(inactive.error().code, ErrorCode::InvalidArgument);
    ASSERT_FALSE(readOnly.has_value());
    EXPECT_EQ(readOnly.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, FailsWithInvalidArgument_WhenControlDoesNotExist)
{
    const auto result = setCameraControl.setValue(0xdead, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, FailsWithDeviceBusy_WhenAnotherHolderHasTheLease)
{
    const auto other = takeLeaseAsAnotherHolder();

    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(access.writes.empty());
    EXPECT_EQ(access.opens, 0U); // nem abriu a câmera sem o lease
}

TEST_F(SetCameraControlTest, FailsWithLeaseExpired_WhenLeaseExpiresBetweenValidationAndWrite)
{
    access.onRead = [this] { clock.advance(16s); }; // além do prazo de 15 s
    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::LeaseExpired);
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, FailsWithLeaseRevoked_WhenDeviceIsRemovedBeforeTheWrite)
{
    access.onRead = [this] { registry.deviceRemoved(cameraInterface()); };

    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::LeaseRevoked);
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, PropagatesOpenError_AndReleasesTheLease)
{
    access.openError = DomainError{.code = ErrorCode::BackendUnavailable, .detail = "câmera não detectada"};

    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "câmera não detectada");
    expectInterfaceFree();
}

TEST_F(SetCameraControlTest, PropagatesFirstReadError_WithoutWriting)
{
    access.readErrors.insert_or_assign(0,
                                       DomainError{.code = ErrorCode::BackendUnavailable, .detail = "câmera removida"});

    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "câmera removida");
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, PropagatesWriteError_WithoutRetrying)
{
    access.writeError = DomainError{.code = ErrorCode::DeviceBusy, .detail = "ocupada"};

    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(access.reads, 1U); // não releu depois da falha
    EXPECT_TRUE(access.writes.empty());
}

TEST_F(SetCameraControlTest, PropagatesRereadError_AfterTheWrite)
{
    access.readErrors.insert_or_assign(1,
                                       DomainError{.code = ErrorCode::BackendUnavailable, .detail = "falha ao reler"});

    const auto result = setCameraControl.setValue(brightnessId, 200);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "falha ao reler");
    EXPECT_EQ(access.writes.size(), 1U); // a escrita aconteceu; o erro é de leitura
}

TEST_F(SetCameraControlTest, ReleasesTheLease_OnSuccessAndOnFailure)
{
    ASSERT_TRUE(setCameraControl.setValue(brightnessId, 200).has_value());
    expectInterfaceFree();

    ASSERT_FALSE(setCameraControl.setValue(brightnessId, 999).has_value());
    expectInterfaceFree();
}

TEST_F(SetCameraControlTest, DestroysTheSession_OnSuccessAndOnFailure)
{
    ASSERT_TRUE(setCameraControl.setValue(brightnessId, 200).has_value());
    EXPECT_EQ(access.liveSessions, 0);

    ASSERT_FALSE(setCameraControl.setValue(brightnessId, 999).has_value());
    EXPECT_EQ(access.liveSessions, 0);

    access.writeError = DomainError{.code = ErrorCode::BackendUnavailable, .detail = "x"};
    ASSERT_FALSE(setCameraControl.setValue(brightnessId, 201).has_value());
    EXPECT_EQ(access.liveSessions, 0);
}
