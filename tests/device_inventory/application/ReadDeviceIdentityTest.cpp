#include "modules/device_inventory/application/ReadDeviceIdentity.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeIdentitySource.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

namespace ghub::device_inventory {
namespace {

using device_access::AccessRegistry;
using device_access::LeaseHolder;
using device_access::LeaseRequest;
using device_access::PhysicalInterfaceId;
using shared_kernel::ErrorCode;

LeaseRequest requestFor(const char* interfaceId, const char* holder)
{
    return LeaseRequest{.interfaceId = PhysicalInterfaceId::create(interfaceId).value(),
                        .holder = LeaseHolder::create(holder).value(),
                        .purpose = "teste",
                        .maxDuration = std::chrono::seconds{1}};
}

class ReadDeviceIdentityTest : public ::testing::Test {
protected:
    fakes::FakeClock clock;
    AccessRegistry registry{clock};
    fakes::FakeIdentitySource source;
    ReadDeviceIdentity useCase{source, registry};
};

TEST_F(ReadDeviceIdentityTest, ReturnsTheIdentityTheSourceReads)
{
    DeviceIdentityParts parts;
    parts.name = "G703";
    source.setResult(DeviceIdentity{std::move(parts)});

    const auto identity = useCase.execute(IdentityTarget::G703);

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(identity->name(), "G703");
    EXPECT_EQ(source.targets(), (std::vector{IdentityTarget::G703}));
}

TEST_F(ReadDeviceIdentityTest, HoldsTheInterfaceLeaseOfEachDeviceWhileReading)
{
    struct Case {
        IdentityTarget target;
        const char* interfaceId;
    };
    for (const Case entry :
         {Case{IdentityTarget::G915, "hidpp:046d:407c"}, Case{IdentityTarget::G703, "hidpp:046d:c539"},
          Case{IdentityTarget::G733, "hidpp:046d:0afe"}}) {
        std::optional<shared_kernel::Result<device_access::DeviceLease>> concurrent;
        source.setOnRead([&] { concurrent = registry.acquire(requestFor(entry.interfaceId, "outro.modulo")); });

        ASSERT_TRUE(useCase.execute(entry.target).has_value());

        ASSERT_TRUE(concurrent.has_value()) << entry.interfaceId;
        ASSERT_FALSE(concurrent->has_value()) << entry.interfaceId;
        EXPECT_EQ(concurrent->error().code, ErrorCode::DeviceBusy);
        EXPECT_NE(concurrent->error().detail.find("inventory.identity"), std::string::npos)
            << "o dono do lease deve aparecer: " << concurrent->error().detail;
    }
}

TEST_F(ReadDeviceIdentityTest, LeaseOfOneDeviceDoesNotBlockTheOthers)
{
    std::optional<shared_kernel::Result<device_access::DeviceLease>> other;
    source.setOnRead([&] { other = registry.acquire(requestFor("hidpp:046d:0afe", "outro.modulo")); });

    ASSERT_TRUE(useCase.execute(IdentityTarget::G915).has_value());

    ASSERT_TRUE(other.has_value());
    EXPECT_TRUE(other->has_value());
}

TEST_F(ReadDeviceIdentityTest, LeaseExpiresAfterTenSeconds)
{
    std::optional<shared_kernel::Result<device_access::DeviceLease>> before;
    std::optional<shared_kernel::Result<device_access::DeviceLease>> after;
    source.setOnRead([&] {
        clock.advance(std::chrono::seconds{9});
        before = registry.acquire(requestFor("hidpp:046d:407c", "outro.modulo"));
        clock.advance(std::chrono::seconds{1});
        after = registry.acquire(requestFor("hidpp:046d:407c", "outro.modulo"));
    });

    ASSERT_TRUE(useCase.execute(IdentityTarget::G915).has_value());

    ASSERT_TRUE(before.has_value());
    EXPECT_FALSE(before->has_value()) << "aos 9 s o prazo de 10 s ainda vale";
    ASSERT_TRUE(after.has_value());
    EXPECT_TRUE(after->has_value()) << "aos 10 s o lease expirou";
}

TEST_F(ReadDeviceIdentityTest, ReleasesTheLeaseAfterASuccessfulRead)
{
    ASSERT_TRUE(useCase.execute(IdentityTarget::G733).has_value());

    EXPECT_TRUE(registry.acquire(requestFor("hidpp:046d:0afe", "outro.modulo")).has_value());
}

TEST_F(ReadDeviceIdentityTest, ReleasesTheLeaseAndPropagatesTheErrorWhenTheReadFails)
{
    source.setResult(shared_kernel::fail(ErrorCode::BackendUnavailable, "headset desligado ou fora de alcance"));

    const auto identity = useCase.execute(IdentityTarget::G733);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(identity.error().detail, "headset desligado ou fora de alcance");
    EXPECT_TRUE(registry.acquire(requestFor("hidpp:046d:0afe", "outro.modulo")).has_value());
}

TEST_F(ReadDeviceIdentityTest, RefusesWithoutTouchingTheSourceWhenTheLeaseIsBusy)
{
    auto holder = registry.acquire(requestFor("hidpp:046d:407c", "lighting.live"));
    ASSERT_TRUE(holder.has_value());

    const auto identity = useCase.execute(IdentityTarget::G915);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().code, ErrorCode::DeviceBusy);
    EXPECT_NE(identity.error().detail.find("lighting.live"), std::string::npos) << identity.error().detail;
    EXPECT_TRUE(source.targets().empty()) << "o hidraw não pode ser tocado com o lease ocupado";
}

TEST_F(ReadDeviceIdentityTest, TheBusyLeaseSurvivesTheRefusedRead)
{
    auto holder = registry.acquire(requestFor("hidpp:046d:407c", "lighting.live"));
    ASSERT_TRUE(holder.has_value());
    ASSERT_FALSE(useCase.execute(IdentityTarget::G915).has_value());

    EXPECT_TRUE(holder->verify().has_value());
}

} // namespace
} // namespace ghub::device_inventory
