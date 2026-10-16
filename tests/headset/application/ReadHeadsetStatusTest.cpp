#include "modules/headset/application/ReadHeadsetStatus.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeHeadsetStatusSource.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

namespace ghub::headset {
namespace {

using device_access::AccessRegistry;
using device_access::LeaseHolder;
using device_access::LeaseRequest;
using device_access::PhysicalInterfaceId;
using shared_kernel::ErrorCode;

LeaseRequest requestFor(const char* holder)
{
    return LeaseRequest{.interfaceId = PhysicalInterfaceId::create("hidpp:046d:0afe").value(),
                        .holder = LeaseHolder::create(holder).value(),
                        .purpose = "teste",
                        .maxDuration = std::chrono::seconds{1}};
}

class ReadHeadsetStatusTest : public ::testing::Test {
protected:
    fakes::FakeClock clock;
    AccessRegistry registry{clock};
    fakes::FakeHeadsetStatusSource source;
    ReadHeadsetStatus useCase{source, registry};
};

TEST_F(ReadHeadsetStatusTest, ReturnsTheStatusTheSourceReads)
{
    HeadsetStatus status;
    status.equalizer = EqualizerInfo{.bandCount = 10, .unexplained = {0x0C, 0x01}};
    source.setResult(status);

    const auto result = useCase.execute();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->equalizer->bandCount, 10);
    EXPECT_EQ(source.reads(), 1);
}

TEST_F(ReadHeadsetStatusTest, HoldsTheHeadsetLeaseForTenSecondsUnderTheStatusHolder)
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
    EXPECT_NE(before->error().detail.find("headset.status"), std::string::npos) << before->error().detail;
    ASSERT_TRUE(after.has_value());
    EXPECT_TRUE(after->has_value()) << "aos 10 s o lease expirou";
}

TEST_F(ReadHeadsetStatusTest, ReleasesTheLeaseAfterASuccessfulRead)
{
    ASSERT_TRUE(useCase.execute().has_value());

    EXPECT_TRUE(registry.acquire(requestFor("outro.modulo")).has_value());
}

TEST_F(ReadHeadsetStatusTest, ReleasesTheLeaseAndPropagatesTheErrorWhenTheReadFails)
{
    source.setResult(shared_kernel::fail(ErrorCode::BackendUnavailable, "sem resposta"));

    const auto result = useCase.execute();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem resposta");
    EXPECT_TRUE(registry.acquire(requestFor("outro.modulo")).has_value());
}

TEST_F(ReadHeadsetStatusTest, RefusesWithoutTouchingTheSourceWhenTheOtherHoldsTheLease)
{
    auto holder = registry.acquire(requestFor("headset.settings"));
    ASSERT_TRUE(holder.has_value());

    const auto result = useCase.execute();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_NE(result.error().detail.find("headset.settings"), std::string::npos) << result.error().detail;
    EXPECT_EQ(source.reads(), 0) << "o hidraw não pode ser tocado com o lease ocupado";
    EXPECT_TRUE(holder->verify().has_value());
}

} // namespace
} // namespace ghub::headset
