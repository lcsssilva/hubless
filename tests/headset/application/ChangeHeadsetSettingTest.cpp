#include "modules/headset/application/ChangeHeadsetSetting.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeHeadsetSettingsWriter.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::headset::ChangeHeadsetSetting;
using ghub::headset::HeadsetLights;
using ghub::headset::SidetoneLevel;
using ghub::shared_kernel::ErrorCode;
using ghub::shared_kernel::fail;

namespace {

// Relógio que avança `step` a cada leitura (zero por padrão): simula o lease expirando entre a aquisição e a
// conferência, sem dormir.
class SteppingClock final : public ghub::shared_kernel::Clock {
public:
    [[nodiscard]] ghub::shared_kernel::Instant now() const override
    {
        const auto current = now_;
        now_ += step;
        return current;
    }

    std::chrono::seconds step{0};

private:
    mutable ghub::shared_kernel::Instant now_{};
};

class ChangeHeadsetSettingTest : public ::testing::Test {
protected:
    SteppingClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeExternalProcessProbe probe;
    ghub::fakes::FakeHeadsetSettingsWriter writer;
    PhysicalInterfaceId interfaceId = PhysicalInterfaceId::create("hidpp:046d:0afe").value();
    ChangeHeadsetSetting useCase{writer, probe, registry, interfaceId};

    ChangeHeadsetSettingTest() { probe.setResult(std::vector<ExternalOwner>{}); }

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

SidetoneLevel level(int value)
{
    return SidetoneLevel::create(value).value();
}

} // namespace

TEST_F(ChangeHeadsetSettingTest, SetSidetoneCallsTheWriterOnceAndReturnsTheAppliedValue)
{
    const auto result = useCase.setSidetone(level(40));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value(), 40);
    EXPECT_EQ(writer.calls(), 1);
    EXPECT_EQ(writer.sidetoneValues(), (std::vector<int>{40}));
}

TEST_F(ChangeHeadsetSettingTest, SetLightsCallsTheWriterOnceAndReturnsTheAppliedState)
{
    const auto result = useCase.setLights(HeadsetLights::Off);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, HeadsetLights::Off);
    EXPECT_EQ(writer.lightsValues(), (std::vector<bool>{false}));
    EXPECT_EQ(writer.calls(), 1);
}

TEST_F(ChangeHeadsetSettingTest, EachExternalOwnerBlocksWithItsNameWithoutCallingTheWriter)
{
    const std::vector<std::pair<ExternalOwner, const char*>> owners{
        {ExternalOwner::OpenRgb, "openrgb"},
        {ExternalOwner::Solaar, "solaar"},
        {ExternalOwner::Piper, "piper"},
        {ExternalOwner::Ratbagd, "ratbagd"},
        {ExternalOwner::HeadsetControl, "headsetcontrol"},
    };
    for (const auto& [owner, name] : owners) {
        probe.setResult(std::vector<ExternalOwner>{owner});

        const auto sidetone = useCase.setSidetone(level(10));
        const auto lights = useCase.setLights(HeadsetLights::On);

        ASSERT_FALSE(sidetone.has_value()) << name;
        EXPECT_EQ(sidetone.error().code, ErrorCode::DeviceBusy) << name;
        EXPECT_EQ(sidetone.error().detail, std::string{"em uso por "} + name);
        ASSERT_FALSE(lights.has_value()) << name;
        EXPECT_EQ(lights.error().code, ErrorCode::DeviceBusy) << name;
    }
    EXPECT_EQ(writer.calls(), 0);
}

TEST_F(ChangeHeadsetSettingTest, ProbeFailureIsReportedWithoutCallingTheWriter)
{
    probe.setResult(fail(ErrorCode::BackendUnavailable, "sem /proc"));

    const auto result = useCase.setSidetone(level(10));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(writer.calls(), 0);
}

TEST_F(ChangeHeadsetSettingTest, BusyLeaseIsDeviceBusyWithoutCallingTheWriter)
{
    auto foreign = registry.acquire(LeaseRequest{.interfaceId = interfaceId,
                                                 .holder = LeaseHolder::create("battery.headsetcontrol").value(),
                                                 .purpose = "teste",
                                                 .maxDuration = std::chrono::seconds{60}});
    ASSERT_TRUE(foreign.has_value());

    const auto result = useCase.setLights(HeadsetLights::On);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(writer.calls(), 0);
}

TEST_F(ChangeHeadsetSettingTest, LeaseExpiredBeforeTheCallIsReportedWithoutCallingTheWriter)
{
    clock.step = std::chrono::seconds{20}; // > 15 s do lease

    const auto result = useCase.setSidetone(level(10));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::LeaseExpired);
    EXPECT_EQ(writer.calls(), 0);
}

TEST_F(ChangeHeadsetSettingTest, HoldsTheLeaseDuringTheWriteAndReleasesAfterwards)
{
    bool acquiredDuringWrite = true;
    writer.setOnCall([&] { acquiredDuringWrite = canAcquireLease(); });

    ASSERT_TRUE(useCase.setSidetone(level(10)).has_value());

    EXPECT_FALSE(acquiredDuringWrite);
    EXPECT_TRUE(canAcquireLease());
}

TEST_F(ChangeHeadsetSettingTest, ReleasesTheLeaseWhenTheWriterFails)
{
    writer.setResult(fail(ErrorCode::BackendUnavailable, "headset não encontrado"));

    const auto result = useCase.setLights(HeadsetLights::On);

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(canAcquireLease());
}

TEST_F(ChangeHeadsetSettingTest, WriterErrorIsPassedThroughWithoutRetry)
{
    writer.setResult(fail(ErrorCode::MalformedData, "json inválido"));

    const auto result = useCase.setSidetone(level(10));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(result.error().detail, "json inválido");
    EXPECT_EQ(writer.calls(), 1);
}

TEST_F(ChangeHeadsetSettingTest, LeaseIsOwnedByHeadsetHeadsetcontrolAndExpiresAfterFifteenSeconds)
{
    // O lease expira em 15 s: depois disso uma nova aquisição só é possível se ele já foi liberado, o que
    // acontece no fim da operação. Aqui conferimos o prazo pelo lado do tempo: dentro do callback o lease
    // segue válido com 14 s e vence com 16 s.
    bool heldAt14 = false;
    bool heldAt16 = true;
    writer.setOnCall([&] {
        clock.step = std::chrono::seconds{14};
        (void)clock.now();
        heldAt14 = !canAcquireLease();
        clock.step = std::chrono::seconds{2};
        (void)clock.now();
        heldAt16 = !canAcquireLease();
        clock.step = std::chrono::seconds{0};
    });

    ASSERT_TRUE(useCase.setSidetone(level(10)).has_value());

    EXPECT_TRUE(heldAt14);
    EXPECT_FALSE(heldAt16);
}
