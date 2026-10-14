#include "LeaseTestSupport.hpp"

#include "fakes/FakeClock.hpp"
#include "modules/device_access/domain/AccessRegistry.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>

using namespace std::chrono_literals;
using ghub::device_access::AccessRegistry;
using ghub::device_access::DeviceLease;
using ghub::device_access::test::request;
using ghub::shared_kernel::ErrorCode;

namespace {

class AccessRegistryTest : public ::testing::Test {
protected:
    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
};

} // namespace

TEST_F(AccessRegistryTest, GrantsLease_WhenInterfaceIsFree)
{
    auto lease = registry.acquire(request("hid:a"));

    ASSERT_TRUE(lease.has_value());
    EXPECT_EQ(lease->interfaceId().value(), "hid:a");
    EXPECT_TRUE(lease->verify().has_value());
}

TEST_F(AccessRegistryTest, RejectsWithDeviceBusy_WhenInterfaceAlreadyLeased)
{
    auto first = registry.acquire(request("hid:a"));
    ASSERT_TRUE(first.has_value());

    auto second = registry.acquire(request("hid:a"));

    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(first->verify().has_value());
}

TEST_F(AccessRegistryTest, GrantsLease_ForDifferentInterface)
{
    auto first = registry.acquire(request("hid:a"));
    auto second = registry.acquire(request("hid:b"));

    EXPECT_TRUE(first.has_value());
    EXPECT_TRUE(second.has_value());
}

TEST_F(AccessRegistryTest, RejectsNonPositiveDuration)
{
    auto zero = registry.acquire(request("hid:a", 0s));
    auto negative = registry.acquire(request("hid:a", -1s));

    ASSERT_FALSE(zero.has_value());
    EXPECT_EQ(zero.error().code, ErrorCode::InvalidArgument);
    ASSERT_FALSE(negative.has_value());
    EXPECT_EQ(negative.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(registry.acquire(request("hid:a")).has_value());
}

TEST_F(AccessRegistryTest, RejectsEmptyPurpose)
{
    auto req = request("hid:a");
    req.purpose.clear();

    auto lease = registry.acquire(std::move(req));

    ASSERT_FALSE(lease.has_value());
    EXPECT_EQ(lease.error().code, ErrorCode::InvalidArgument);
}

TEST_F(AccessRegistryTest, ReleasesInterface_WhenLeaseDestroyed)
{
    {
        auto lease = registry.acquire(request("hid:a"));
        ASSERT_TRUE(lease.has_value());
    }

    EXPECT_TRUE(registry.acquire(request("hid:a")).has_value());
}

TEST_F(AccessRegistryTest, KeepsInterface_WhenLeaseMoved)
{
    std::optional<DeviceLease> holder;
    {
        auto lease = registry.acquire(request("hid:a"));
        ASSERT_TRUE(lease.has_value());
        holder.emplace(std::move(*lease));
    }

    auto competing = registry.acquire(request("hid:a"));
    ASSERT_FALSE(competing.has_value());
    EXPECT_EQ(competing.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(holder->verify().has_value());

    holder.reset();
    EXPECT_TRUE(registry.acquire(request("hid:a")).has_value());
}

TEST_F(AccessRegistryTest, ReleasesHeldInterface_WhenLeaseMoveAssignedOver)
{
    auto a = registry.acquire(request("hid:a"));
    auto b = registry.acquire(request("hid:b"));
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    *a = std::move(*b);

    EXPECT_EQ(a->interfaceId().value(), "hid:b");
    EXPECT_TRUE(registry.acquire(request("hid:a")).has_value());
    EXPECT_FALSE(registry.acquire(request("hid:b")).has_value());
}

TEST_F(AccessRegistryTest, LeaseIsValid_JustBeforeDeadline)
{
    auto lease = registry.acquire(request("hid:a", 5s));
    ASSERT_TRUE(lease.has_value());

    clock.advance(5s - 1ns);

    EXPECT_TRUE(lease->verify().has_value());
}

TEST_F(AccessRegistryTest, VerifyFailsWithLeaseExpired_AtDeadline)
{
    auto lease = registry.acquire(request("hid:a", 5s));
    ASSERT_TRUE(lease.has_value());

    clock.advance(5s);

    const auto verified = lease->verify();
    ASSERT_FALSE(verified.has_value());
    EXPECT_EQ(verified.error().code, ErrorCode::LeaseExpired);
}

TEST_F(AccessRegistryTest, GrantsLease_WhenPreviousLeaseExpired)
{
    auto old = registry.acquire(request("hid:a", 5s));
    ASSERT_TRUE(old.has_value());
    clock.advance(5s);

    auto fresh = registry.acquire(request("hid:a"));

    ASSERT_TRUE(fresh.has_value());
    EXPECT_TRUE(fresh->verify().has_value());
    const auto oldVerified = old->verify();
    ASSERT_FALSE(oldVerified.has_value());
    EXPECT_EQ(oldVerified.error().code, ErrorCode::LeaseExpired);
}

TEST_F(AccessRegistryTest, RevokesLease_WhenDeviceRemoved)
{
    auto lease = registry.acquire(request("hid:a"));
    auto other = registry.acquire(request("hid:b"));
    ASSERT_TRUE(lease.has_value());
    ASSERT_TRUE(other.has_value());

    registry.deviceRemoved(lease->interfaceId());

    const auto verified = lease->verify();
    ASSERT_FALSE(verified.has_value());
    EXPECT_EQ(verified.error().code, ErrorCode::LeaseRevoked);
    EXPECT_TRUE(other->verify().has_value());
    EXPECT_TRUE(registry.acquire(request("hid:a")).has_value());
}

TEST_F(AccessRegistryTest, RevokesAllLeases_WhenSystemSuspending)
{
    auto a = registry.acquire(request("hid:a"));
    auto b = registry.acquire(request("hid:b"));
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    registry.systemSuspending();

    ASSERT_FALSE(a->verify().has_value());
    EXPECT_EQ(a->verify().error().code, ErrorCode::LeaseRevoked);
    ASSERT_FALSE(b->verify().has_value());
    EXPECT_EQ(b->verify().error().code, ErrorCode::LeaseRevoked);
    EXPECT_TRUE(registry.acquire(request("hid:a")).has_value());
    EXPECT_TRUE(registry.acquire(request("hid:b")).has_value());
}

TEST_F(AccessRegistryTest, DoesNotReleaseNewerLease_WhenStaleLeaseDestroyed)
{
    auto stale = std::make_unique<DeviceLease>(registry.acquire(request("hid:a")).value());
    registry.deviceRemoved(stale->interfaceId());
    auto fresh = registry.acquire(request("hid:a"));
    ASSERT_TRUE(fresh.has_value());

    stale.reset();

    EXPECT_TRUE(fresh->verify().has_value());
    auto competing = registry.acquire(request("hid:a"));
    ASSERT_FALSE(competing.has_value());
    EXPECT_EQ(competing.error().code, ErrorCode::DeviceBusy);
}

TEST(AccessRegistryLifetime, VerifyFailsWithLeaseRevoked_WhenRegistryDestroyed)
{
    ghub::fakes::FakeClock clock;
    auto registry = std::make_unique<AccessRegistry>(clock);
    auto lease = registry->acquire(request("hid:a"));
    ASSERT_TRUE(lease.has_value());

    registry.reset();

    const auto verified = lease->verify();
    ASSERT_FALSE(verified.has_value());
    EXPECT_EQ(verified.error().code, ErrorCode::LeaseRevoked);
}
