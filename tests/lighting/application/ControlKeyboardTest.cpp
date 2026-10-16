#include "modules/lighting/application/ControlKeyboard.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeKeyboardControl.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace {

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::LeaseHolder;
using ghub::device_access::PhysicalInterfaceId;
using ghub::lighting::application::ControlKeyboard;
using ghub::lighting::domain::GameModeKeys;
using ghub::lighting::domain::OnboardSlot;
using ghub::lighting::domain::OnboardSlots;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;
using Calls = std::vector<std::string>;

PhysicalInterfaceId g915Interface()
{
    return PhysicalInterfaceId::create("hidpp:046d:407c").value(); // NOLINT(bugprone-unchecked-optional-access)
}

OnboardSlot slot(std::uint8_t number)
{
    return *OnboardSlot::create(number); // NOLINT(bugprone-unchecked-optional-access)
}

class ControlKeyboardTest : public ::testing::Test {
protected:
    ControlKeyboardTest()
    {
        probe.setResult(std::vector<ExternalOwner>{});
        control.setSlots(OnboardSlots{.enabled = {slot(1), slot(3)}, .active = slot(1)});
    }

    bool otherHolderCanAcquire()
    {
        auto lease = registry.acquire({
            .interfaceId = g915Interface(),
            .holder = LeaseHolder::create("outro").value(), // NOLINT(bugprone-unchecked-optional-access)
            .purpose = "teste",
            .maxDuration = 1s,
        });
        return lease.has_value();
    }

    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeExternalProcessProbe probe;
    ghub::fakes::FakeKeyboardControl control;
    ControlKeyboard useCase{control, registry, probe, g915Interface()};
};

} // namespace

TEST_F(ControlKeyboardTest, ConstructingTheUseCaseTouchesNothing)
{
    EXPECT_EQ(probe.calls(), 0);
    EXPECT_TRUE(control.calls().empty());
}

TEST_F(ControlKeyboardTest, SetGameModeSendsTheKeysUnderTheLeaseAndReleasesIt)
{
    bool heldDuringCall = false;
    control.setOnCall([&] { heldDuringCall = !otherHolderCanAcquire(); });

    const auto done = useCase.setGameMode(GameModeKeys::suggested());

    ASSERT_TRUE(done.has_value()) << done.error().detail;
    EXPECT_EQ(control.calls(), (Calls{"open", "game:e3e765"}));
    EXPECT_TRUE(heldDuringCall);
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(ControlKeyboardTest, ExternalOwnerMakesEveryOperationBusyWithoutOpening)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::OpenRgb});

    const auto game = useCase.setGameMode(GameModeKeys::suggested());
    const auto slots = useCase.listSlots();
    const auto selected = useCase.selectSlot(slot(1));

    for (const auto* result : {&game}) {
        ASSERT_FALSE(result->has_value());
        EXPECT_EQ(result->error().code, ErrorCode::DeviceBusy);
    }
    ASSERT_FALSE(slots.has_value());
    EXPECT_EQ(slots.error().code, ErrorCode::DeviceBusy);
    ASSERT_FALSE(selected.has_value());
    EXPECT_EQ(selected.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(control.calls().empty());
}

TEST_F(ControlKeyboardTest, LeaseHeldByTheLiveSessionMakesItBusyWithoutOpening)
{
    auto live = registry.acquire({
        .interfaceId = g915Interface(),
        .holder = LeaseHolder::create("lighting.hidpp").value(), // NOLINT(bugprone-unchecked-optional-access)
        .purpose = "sessão ao vivo de iluminação",
        .maxDuration = 15s,
    });
    ASSERT_TRUE(live.has_value());

    const auto done = useCase.setGameMode(GameModeKeys::suggested());

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(control.calls().empty());
}

TEST_F(ControlKeyboardTest, OpenFailureIsReportedAndReleasesTheLease)
{
    control.failOpen(DomainError{.code = ErrorCode::BackendUnavailable, .detail = "sem teclado"});

    const auto done = useCase.setGameMode(GameModeKeys::suggested());

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(ControlKeyboardTest, GameModeFailureIsReportedWithoutRetry)
{
    control.failGameMode(DomainError{.code = ErrorCode::BackendUnavailable, .detail = "sem resposta"});

    const auto done = useCase.setGameMode(GameModeKeys::suggested());

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().detail, "sem resposta");
    EXPECT_EQ(control.calls(), (Calls{"open", "game:e3e765"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(ControlKeyboardTest, ListSlotsReturnsTheEnabledOnesUnderTheLease)
{
    const auto slots = useCase.listSlots();

    ASSERT_TRUE(slots.has_value());
    EXPECT_EQ(slots->enabled, (std::vector<OnboardSlot>{slot(1), slot(3)}));
    EXPECT_EQ(slots->active, slot(1));
    EXPECT_EQ(control.calls(), (Calls{"open", "slots"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(ControlKeyboardTest, SelectSlotReadsTheDirectoryThenSwitchesInTheSameSession)
{
    const auto done = useCase.selectSlot(slot(3));

    ASSERT_TRUE(done.has_value()) << done.error().detail;
    EXPECT_EQ(control.calls(), (Calls{"open", "slots", "select:3"}));
}

TEST_F(ControlKeyboardTest, SelectSlotRefusesASlotThatIsNotEnabledWithoutSending)
{
    const auto done = useCase.selectSlot(slot(2));

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(control.calls(), (Calls{"open", "slots"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(ControlKeyboardTest, SelectSlotFailureIsReported)
{
    control.failSelect(DomainError{.code = ErrorCode::BackendUnavailable, .detail = "erro 5"});

    const auto done = useCase.selectSlot(slot(1));

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().detail, "erro 5");
}
