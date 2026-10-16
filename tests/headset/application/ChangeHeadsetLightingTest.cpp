#include "modules/headset/application/ChangeHeadsetLighting.hpp"
#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::headset::ChangeHeadsetLighting;
using ghub::headset::HeadsetLighting;
using ghub::headset::HeadsetLightingWriter;
using ghub::headset::LightingEffect;
using ghub::headset::LightingZone;
using ghub::shared_kernel::ErrorCode;

namespace {

class RecordingWriter final : public HeadsetLightingWriter {
public:
    [[nodiscard]] ghub::shared_kernel::Result<void> applyLighting(HeadsetLighting lighting) override
    {
        applied.push_back(lighting);
        return result;
    }

    std::vector<HeadsetLighting> applied;
    ghub::shared_kernel::Result<void> result;
};

class ChangeHeadsetLightingTest : public ::testing::Test {
protected:
    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    RecordingWriter writer;
    PhysicalInterfaceId interfaceId = PhysicalInterfaceId::create("hidpp:046d:0afe").value();
    ChangeHeadsetLighting useCase{writer, registry, interfaceId};
    HeadsetLighting lighting =
        HeadsetLighting::create(LightingZone::Upper, LightingEffect::Breathing, {0, 255, 255}, 5000, 100).value();

    [[nodiscard]] bool canAcquireLease()
    {
        return registry
            .acquire(LeaseRequest{.interfaceId = interfaceId,
                                  .holder = LeaseHolder::create("outro").value(),
                                  .purpose = "teste",
                                  .maxDuration = std::chrono::seconds{1}})
            .has_value();
    }
};

TEST_F(ChangeHeadsetLightingTest, AppliesOnceUnderTheLeaseAndReleasesIt)
{
    const auto done = useCase.apply(lighting);

    ASSERT_TRUE(done.has_value());
    EXPECT_EQ(*done, lighting);
    EXPECT_EQ(writer.applied.size(), 1U);
    EXPECT_TRUE(canAcquireLease()) << "o lease foi liberado";
}

TEST_F(ChangeHeadsetLightingTest, BusyLeaseFailsWithTheOwnerWithoutWriting)
{
    const auto held = registry.acquire(LeaseRequest{.interfaceId = interfaceId,
                                                    .holder = LeaseHolder::create("headset.status").value(),
                                                    .purpose = "leitura",
                                                    .maxDuration = std::chrono::seconds{5}});
    ASSERT_TRUE(held.has_value());

    const auto done = useCase.apply(lighting);

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(writer.applied.empty());
}

TEST_F(ChangeHeadsetLightingTest, WriterFailureIsReturnedAndTheLeaseReleased)
{
    writer.result = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "headset desligado ou fora de alcance");

    const auto done = useCase.apply(lighting);

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().detail, "headset desligado ou fora de alcance");
    EXPECT_EQ(writer.applied.size(), 1U) << "sem retentativa";
    EXPECT_TRUE(canAcquireLease());
}

} // namespace
