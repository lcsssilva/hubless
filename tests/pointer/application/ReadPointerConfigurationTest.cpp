#include "modules/pointer/application/ReadPointerConfiguration.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakePointerBackend.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <variant>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::device_inventory::ConnectionPath;
using ghub::device_inventory::UsbId;
using ghub::pointer::DiscoveredPointer;
using ghub::pointer::NotDetected;
using ghub::pointer::PointerConfiguration;
using ghub::pointer::PointerProfile;
using ghub::pointer::PointerStatus;
using ghub::pointer::ReadPointerConfiguration;
using ghub::pointer::Ready;
using ghub::pointer::Unavailable;
using ghub::shared_kernel::ErrorCode;

namespace {

constexpr UsbId g703Receiver{.vendorId = 0x046D, .productId = 0x4086};
constexpr UsbId g703Cable{.vendorId = 0x046D, .productId = 0xC090};
constexpr UsbId g733{.vendorId = 0x046D, .productId = 0x0AFE};

PointerConfiguration configurationNamed(const std::string& name)
{
    const auto profile = PointerProfile::create({
                                                    .index = 0,
                                                    .name = "",
                                                    .active = true,
                                                    .enabled = true,
                                                    .reportRate = 1000,
                                                    .supportedReportRates = {1000},
                                                    .stages = {},
                                                    .buttons = {},
                                                    .leds = {},
                                                })
                             .value();
    return PointerConfiguration::create(name, {profile}).value();
}

DiscoveredPointer discovered(UsbId id, const std::string& name)
{
    return {.usbId = id, .name = name, .configuration = configurationNamed(name)};
}

class ReadPointerConfigurationTest : public ::testing::Test {
protected:
    PhysicalInterfaceId interfaceId = PhysicalInterfaceId::create("ratbag:046d:4086").value();

    PointerStatus run() { return ReadPointerConfiguration{backend, probe, registry, interfaceId}.execute(); }

    void blockWith(ExternalOwner owner) { probe.setResult(std::vector<ExternalOwner>{owner}); }

    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeExternalProcessProbe probe;
    ghub::fakes::FakePointerBackend backend;
};

const Unavailable& unavailable(const PointerStatus& status)
{
    return std::get<Unavailable>(status);
}

} // namespace

TEST_F(ReadPointerConfigurationTest, ReadyForTheG703ReceiverWithItsConnectionPath)
{
    backend.setResult(
        std::vector{discovered(g733, "Logitech G733 Gaming Headset"), discovered(g703Receiver, "Logitech G703 LS")});

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Ready>(status));
    EXPECT_EQ(std::get<Ready>(status).configuration.name(), "Logitech G703 LS");
    EXPECT_EQ(std::get<Ready>(status).path, ConnectionPath::LightspeedReceiver);
    EXPECT_EQ(backend.calls(), 1);
}

TEST_F(ReadPointerConfigurationTest, ReadyForTheG703OnCable)
{
    backend.setResult(std::vector{discovered(g703Cable, "Logitech G703")});

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Ready>(status));
    EXPECT_EQ(std::get<Ready>(status).path, ConnectionPath::UsbCable);
}

TEST_F(ReadPointerConfigurationTest, G733AloneIsIgnoredAndMouseNotDetected)
{
    backend.setResult(std::vector{discovered(g733, "Logitech G733 Gaming Headset")});

    EXPECT_TRUE(std::holds_alternative<NotDetected>(run()));
}

TEST_F(ReadPointerConfigurationTest, DeviceWithoutUsbIdIsIgnored)
{
    backend.setResult(std::vector{
        DiscoveredPointer{
            .usbId = std::nullopt,
            .name = "Bluetooth mouse",
            .configuration = configurationNamed("Bluetooth mouse"),
        },
    });

    EXPECT_TRUE(std::holds_alternative<NotDetected>(run()));
}

TEST_F(ReadPointerConfigurationTest, NoDevicesAtAllIsNotDetected)
{
    backend.setResult(std::vector<DiscoveredPointer>{});

    EXPECT_TRUE(std::holds_alternative<NotDetected>(run()));
}

TEST_F(ReadPointerConfigurationTest, GlobalBackendErrorIsUnavailable)
{
    backend.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "ServiceUnknown"));

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(unavailable(status).error.detail, "ServiceUnknown");
}

TEST_F(ReadPointerConfigurationTest, G703ConfigurationErrorIsUnavailableWithThatError)
{
    backend.setResult(std::vector{
        DiscoveredPointer{.usbId = g733, .name = "G733", .configuration = configurationNamed("G733")},
        DiscoveredPointer{
            .usbId = g703Receiver,
            .name = "G703",
            .configuration = ghub::shared_kernel::fail(ErrorCode::MalformedData, "tipo inesperado"),
        },
    });

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::MalformedData);
    EXPECT_EQ(unavailable(status).error.detail, "tipo inesperado");
}

TEST_F(ReadPointerConfigurationTest, ErrorInAnotherDeviceDoesNotAffectTheG703)
{
    backend.setResult(std::vector{
        DiscoveredPointer{
            .usbId = g733,
            .name = "G733",
            .configuration = ghub::shared_kernel::fail(ErrorCode::MalformedData, "x"),
        },
        discovered(g703Receiver, "Logitech G703 LS"),
    });

    EXPECT_TRUE(std::holds_alternative<Ready>(run()));
}

TEST_F(ReadPointerConfigurationTest, OpenRgbBlocksWithoutCallingTheBackend)
{
    blockWith(ExternalOwner::OpenRgb);

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::DeviceBusy);
    EXPECT_NE(unavailable(status).error.detail.find("openrgb"), std::string::npos);
    EXPECT_EQ(backend.calls(), 0);
}

TEST_F(ReadPointerConfigurationTest, SolaarBlocksWithoutCallingTheBackend)
{
    blockWith(ExternalOwner::Solaar);

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::DeviceBusy);
    EXPECT_NE(unavailable(status).error.detail.find("solaar"), std::string::npos);
    EXPECT_EQ(backend.calls(), 0);
}

TEST_F(ReadPointerConfigurationTest, HeadsetControlBlocksWithoutCallingTheBackend)
{
    blockWith(ExternalOwner::HeadsetControl);

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::DeviceBusy);
    EXPECT_NE(unavailable(status).error.detail.find("headsetcontrol"), std::string::npos);
    EXPECT_EQ(backend.calls(), 0);
}

TEST_F(ReadPointerConfigurationTest, PiperDoesNotBlock)
{
    blockWith(ExternalOwner::Piper);
    backend.setResult(std::vector{discovered(g703Receiver, "Logitech G703 LS")});

    EXPECT_TRUE(std::holds_alternative<Ready>(run()));
    EXPECT_EQ(backend.calls(), 1);
}

TEST_F(ReadPointerConfigurationTest, RatbagdDoesNotBlock)
{
    blockWith(ExternalOwner::Ratbagd);
    backend.setResult(std::vector{discovered(g703Receiver, "Logitech G703 LS")});

    EXPECT_TRUE(std::holds_alternative<Ready>(run()));
    EXPECT_EQ(backend.calls(), 1);
}

TEST_F(ReadPointerConfigurationTest, BlockingOwnerAmongHarmlessOnesStillBlocks)
{
    probe.setResult(std::vector{ExternalOwner::Piper, ExternalOwner::Solaar, ExternalOwner::Ratbagd});

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::DeviceBusy);
    EXPECT_EQ(backend.calls(), 0);
}

TEST_F(ReadPointerConfigurationTest, ProbeFailureIsUnavailableWithoutReading)
{
    probe.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "/proc ilegível"));

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(unavailable(status).error.detail, "/proc ilegível");
    EXPECT_EQ(backend.calls(), 0);
}

TEST_F(ReadPointerConfigurationTest, BusyLeaseIsDeviceBusyWithoutCallingTheBackend)
{
    auto foreign = registry.acquire(LeaseRequest{
        .interfaceId = interfaceId,
        .holder = LeaseHolder::create("piper-like").value(),
        .purpose = "teste",
        .maxDuration = std::chrono::seconds{60},
    });
    ASSERT_TRUE(foreign.has_value());

    const auto status = run();

    ASSERT_TRUE(std::holds_alternative<Unavailable>(status));
    EXPECT_EQ(unavailable(status).error.code, ErrorCode::DeviceBusy);
    EXPECT_EQ(backend.calls(), 0);
}

TEST_F(ReadPointerConfigurationTest, HoldsTheLeaseDuringTheReadAndReleasesAfterwards)
{
    backend.setResult(std::vector{discovered(g703Receiver, "Logitech G703 LS")});
    ErrorCode duringRead = ErrorCode::InvalidArgument;
    backend.setOnRead([&] {
        const auto attempt = registry.acquire(LeaseRequest{
            .interfaceId = interfaceId,
            .holder = LeaseHolder::create("outro").value(),
            .purpose = "teste",
            .maxDuration = std::chrono::seconds{1},
        });
        duringRead = attempt.has_value() ? ErrorCode::InvalidArgument : attempt.error().code;
    });

    EXPECT_TRUE(std::holds_alternative<Ready>(run()));

    EXPECT_EQ(duringRead, ErrorCode::DeviceBusy);
    EXPECT_TRUE(registry
                    .acquire(LeaseRequest{.interfaceId = interfaceId,
                                          .holder = LeaseHolder::create("depois").value(),
                                          .purpose = "teste",
                                          .maxDuration = std::chrono::seconds{1}})
                    .has_value());
}

TEST_F(ReadPointerConfigurationTest, ReleasesTheLeaseAlsoWhenTheBackendFails)
{
    backend.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "x"));

    EXPECT_TRUE(std::holds_alternative<Unavailable>(run()));

    EXPECT_TRUE(registry
                    .acquire(LeaseRequest{.interfaceId = interfaceId,
                                          .holder = LeaseHolder::create("depois").value(),
                                          .purpose = "teste",
                                          .maxDuration = std::chrono::seconds{1}})
                    .has_value());
}

TEST_F(ReadPointerConfigurationTest, LeaseExpiresAfterSixtySecondsAndIsOwnedByPointerRatbag)
{
    backend.setResult(std::vector{discovered(g703Receiver, "Logitech G703 LS")});
    bool alive59 = false;
    bool expired61 = false;
    backend.setOnRead([&] {
        // Um segundo lease só seria possível se o primeiro tivesse expirado: verifica o prazo de 60 s.
        clock.advance(std::chrono::seconds{59});
        alive59 = !registry
                       .acquire(LeaseRequest{
                           .interfaceId = interfaceId,
                           .holder = LeaseHolder::create("x").value(),
                           .purpose = "t",
                           .maxDuration = std::chrono::seconds{1},
                       })
                       .has_value();
        clock.advance(std::chrono::seconds{2});
        expired61 = registry
                        .acquire(LeaseRequest{
                            .interfaceId = interfaceId,
                            .holder = LeaseHolder::create("x").value(),
                            .purpose = "t",
                            .maxDuration = std::chrono::seconds{1},
                        })
                        .has_value();
    });

    (void)run();

    EXPECT_TRUE(alive59);
    EXPECT_TRUE(expired61);
}
