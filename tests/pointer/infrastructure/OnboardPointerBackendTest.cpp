#include "modules/pointer/infrastructure/OnboardPointerBackend.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ghub::pointer::infrastructure {
namespace {

using shared_kernel::ErrorCode;
using Bytes = std::vector<std::uint8_t>;

Bytes sector0001()
{
    return {fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
}

class OnboardPointerBackendTest : public ::testing::Test {
protected:
    OnboardPointerBackendTest()
    {
        connector.state.sectors[0] = {fixtures::kDirectory0000.begin(), fixtures::kDirectory0000.end()};
        connector.state.sectors[1] = sector0001();
    }

    fakes::FakeOnboardMemoryConnector connector;
};

TEST_F(OnboardPointerBackendTest, ReadsTheActiveSectorAsTheG703WithItsUsbId)
{
    OnboardPointerBackend backend{connector};
    const auto devices = backend.readDevices();
    ASSERT_TRUE(devices.has_value());
    ASSERT_EQ(devices->size(), 1U);
    const auto& device = devices->front();
    ASSERT_TRUE(device.usbId.has_value());
    EXPECT_EQ(device.usbId->vendorId, 0x046dU);
    EXPECT_EQ(device.usbId->productId, 0x4086U);
    EXPECT_EQ(device.name, "G703");
    ASSERT_TRUE(device.configuration.has_value());
    const auto& profile = device.configuration->profiles().front();
    EXPECT_EQ(profile.index(), 1U); // fn4 contada a partir de 1
    EXPECT_EQ(profile.stages().front().dpiX(), 7050U);
    EXPECT_EQ(profile.buttons().size(), 6U);
    EXPECT_EQ(profile.leds().size(), 2U);
}

TEST_F(OnboardPointerBackendTest, OnlyReadsAndClosesTheNode)
{
    OnboardPointerBackend backend{connector};
    ASSERT_TRUE(backend.readDevices().has_value());
    EXPECT_EQ(connector.state.calls,
              (std::vector<std::string>{"open", "describe", "mode", "current", "read:0", "read:1"}));
    EXPECT_EQ(connector.state.liveMemories, 0);
}

TEST_F(OnboardPointerBackendTest, SafetyRefusalsOfTheActiveSectorAreAGlobalError)
{
    connector.state.mode = 2;
    OnboardPointerBackend backend{connector};
    const auto devices = backend.readDevices();
    ASSERT_FALSE(devices.has_value());
    EXPECT_EQ(devices.error().detail, "mouse fora do modo onboard");
}

TEST_F(OnboardPointerBackendTest, OpenFailureIsAGlobalError)
{
    connector.state.openError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "G703 não encontrado"};
    OnboardPointerBackend backend{connector};
    const auto devices = backend.readDevices();
    ASSERT_FALSE(devices.has_value());
    EXPECT_EQ(devices.error().detail, "G703 não encontrado");
}

TEST_F(OnboardPointerBackendTest, SectorTheDomainCannotReadIsAPerDeviceConfigurationError)
{
    Bytes raw = sector0001();
    raw[0] = 3;
    connector.state.sectors[1] = onboard_memory::OnboardSector::fromBytes(std::move(raw))->withCrc().bytes();
    OnboardPointerBackend backend{connector};
    const auto devices = backend.readDevices();
    ASSERT_TRUE(devices.has_value());
    ASSERT_EQ(devices->size(), 1U);
    ASSERT_FALSE(devices->front().configuration.has_value());
    EXPECT_EQ(devices->front().configuration.error().code, ErrorCode::MalformedData);
}

} // namespace
} // namespace ghub::pointer::infrastructure
