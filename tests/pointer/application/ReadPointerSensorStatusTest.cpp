#include "modules/pointer/application/ReadPointerSensorStatus.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakePointerSensorSource.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

namespace ghub::pointer {
namespace {

using device_access::AccessRegistry;
using device_access::LeaseHolder;
using device_access::LeaseRequest;
using device_access::PhysicalInterfaceId;
using shared_kernel::ErrorCode;

LeaseRequest requestFor(const char* holder)
{
    return LeaseRequest{.interfaceId = PhysicalInterfaceId::create("hidpp:046d:c539").value(),
                        .holder = LeaseHolder::create(holder).value(),
                        .purpose = "teste",
                        .maxDuration = std::chrono::seconds{1}};
}

class ReadPointerSensorStatusTest : public ::testing::Test {
protected:
    fakes::FakeClock clock;
    AccessRegistry registry{clock};
    fakes::FakePointerSensorSource source;
    ReadPointerSensorStatus useCase{source, registry};
};

TEST_F(ReadPointerSensorStatusTest, ReturnsTheStatusTheSourceReads)
{
    PointerSensorStatus status;
    status.currentPeriodMs = 1;
    source.setResult(status);

    const auto result = useCase.execute();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->currentPeriodMs, 1);
    EXPECT_EQ(source.reads(), 1);
}

TEST_F(ReadPointerSensorStatusTest, HoldsTheSensorLeaseForTenSecondsUnderTheStatusHolder)
{
    std::optional<shared_kernel::Result<device_access::DeviceLease>> before;
    std::optional<shared_kernel::Result<device_access::DeviceLease>> after;
    source.setOnRead([&] {
        clock.advance(std::chrono::seconds{9});
        before = registry.acquire(requestFor("outro.modulo"));
        clock.advance(std::chrono::seconds{1});
        after = registry.acquire(requestFor("outro.modulo"));
    });

    ASSERT_TRUE(useCase.execute().has_value());

    ASSERT_TRUE(before.has_value());
    ASSERT_FALSE(before->has_value());
    EXPECT_EQ(before->error().code, ErrorCode::DeviceBusy);
    EXPECT_NE(before->error().detail.find("pointer.status"), std::string::npos) << before->error().detail;
    ASSERT_TRUE(after.has_value());
    EXPECT_TRUE(after->has_value()) << "aos 10 s o lease expirou";
}

TEST_F(ReadPointerSensorStatusTest, ReleasesTheLeaseAfterASuccessfulRead)
{
    ASSERT_TRUE(useCase.execute().has_value());

    EXPECT_TRUE(registry.acquire(requestFor("outro.modulo")).has_value());
}

TEST_F(ReadPointerSensorStatusTest, ReleasesTheLeaseAndPropagatesTheErrorWhenTheReadFails)
{
    source.setResult(shared_kernel::fail(ErrorCode::BackendUnavailable, "sem resposta"));

    const auto result = useCase.execute();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem resposta");
    EXPECT_TRUE(registry.acquire(requestFor("outro.modulo")).has_value());
}

TEST_F(ReadPointerSensorStatusTest, RefusesWithoutTouchingTheSourceWhenTheOtherHoldsTheLease)
{
    auto holder = registry.acquire(requestFor("pointer.onboard"));
    ASSERT_TRUE(holder.has_value());

    const auto result = useCase.execute();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_NE(result.error().detail.find("pointer.onboard"), std::string::npos) << result.error().detail;
    EXPECT_EQ(source.reads(), 0) << "o hidraw não pode ser tocado com o lease ocupado";
    EXPECT_TRUE(holder->verify().has_value());
}

} // namespace
} // namespace ghub::pointer
