#include "modules/lighting/application/RunSoftwareAnimation.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeKeyboardLighting.hpp"
#include "modules/lighting/domain/AnimationFrames.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::PhysicalInterfaceId;
using ghub::lighting::application::RunSoftwareAnimation;
using ghub::lighting::application::StartLiveKeyboardSession;
using ghub::lighting::application::TickOutcome;
using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::KeyColorMap;
using ghub::lighting::domain::RgbColor;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;

PhysicalInterfaceId g915Interface()
{
    return PhysicalInterfaceId::create("hidpp:046d:407c").value(); // NOLINT(bugprone-unchecked-optional-access)
}

class RunSoftwareAnimationTest : public ::testing::Test {
protected:
    RunSoftwareAnimationTest()
    {
        probe.setResult(std::vector<ExternalOwner>{});
        session = useCase.start().value(); // NOLINT(bugprone-unchecked-optional-access)
    }

    // Fonte de quadros que registra o tempo recebido e pinta tudo com a cor atual de `color`.
    RunSoftwareAnimation::FrameSource source()
    {
        return [this](std::int64_t elapsedMs) {
            elapsed.push_back(elapsedMs);
            return frame;
        };
    }

    static KeyColorMap everyKey(RgbColor color)
    {
        KeyColorMap map;
        for (const auto& key : G915KeyCatalog::keys()) {
            map = map.with(key.id, color);
        }
        return map;
    }

    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeExternalProcessProbe probe;
    ghub::fakes::FakeKeyboardLighting keyboard;
    StartLiveKeyboardSession useCase{keyboard, registry, probe, g915Interface()};
    std::unique_ptr<ghub::lighting::application::LiveKeyboardSession> session;
    KeyColorMap frame = everyKey({255, 0, 0});
    std::vector<std::int64_t> elapsed;
};

TEST_F(RunSoftwareAnimationTest, DefaultRateIsTenFramesPerSecond)
{
    const RunSoftwareAnimation run{*session, clock, source()};
    EXPECT_EQ(run.framesPerSecond(), 10U);
}

TEST_F(RunSoftwareAnimationTest, RateIsClampedBetweenOneAndTwenty)
{
    EXPECT_EQ((RunSoftwareAnimation{*session, clock, source(), 50}.framesPerSecond()), 20U);
    EXPECT_EQ((RunSoftwareAnimation{*session, clock, source(), 0}.framesPerSecond()), 1U);
    EXPECT_EQ((RunSoftwareAnimation{*session, clock, source(), 20}.framesPerSecond()), 20U);
}

TEST_F(RunSoftwareAnimationTest, ConstructingSendsNothing)
{
    const RunSoftwareAnimation run{*session, clock, source()};
    EXPECT_TRUE(keyboard.painted().empty());
    EXPECT_TRUE(elapsed.empty());
}

TEST_F(RunSoftwareAnimationTest, FirstTickPaintsTheWholeFrameWithElapsedZero)
{
    RunSoftwareAnimation run{*session, clock, source()};

    const auto outcome = run.tick();

    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(*outcome, TickOutcome::Sent);
    ASSERT_EQ(keyboard.painted().size(), 1U);
    EXPECT_EQ(keyboard.painted().front(), frame);
    ASSERT_EQ(elapsed.size(), 1U);
    EXPECT_EQ(elapsed.front(), 0);
}

TEST_F(RunSoftwareAnimationTest, TickBeforeTheIntervalIsSkippedWithoutEvaluatingOrSending)
{
    RunSoftwareAnimation run{*session, clock, source()};
    ASSERT_TRUE(run.tick().has_value());
    frame = everyKey({0, 255, 0});

    EXPECT_EQ(run.tick().value(), TickOutcome::Skipped);
    clock.advance(99ms);
    EXPECT_EQ(run.tick().value(), TickOutcome::Skipped);

    EXPECT_EQ(keyboard.painted().size(), 1U);
    EXPECT_EQ(elapsed.size(), 1U);
}

TEST_F(RunSoftwareAnimationTest, TickAtTheIntervalSendsAndPassesElapsedTime)
{
    RunSoftwareAnimation run{*session, clock, source()};
    ASSERT_TRUE(run.tick().has_value());
    frame = everyKey({0, 255, 0});

    clock.advance(100ms);
    EXPECT_EQ(run.tick().value(), TickOutcome::Sent);

    ASSERT_EQ(keyboard.painted().size(), 2U);
    EXPECT_EQ(elapsed.back(), 100);
}

TEST_F(RunSoftwareAnimationTest, MaximumRateSpacesFramesBy50Ms)
{
    RunSoftwareAnimation run{*session, clock, source(), 20};
    ASSERT_TRUE(run.tick().has_value());
    frame = everyKey({0, 255, 0});

    clock.advance(49ms);
    EXPECT_EQ(run.tick().value(), TickOutcome::Skipped);
    clock.advance(1ms);
    EXPECT_EQ(run.tick().value(), TickOutcome::Sent);
}

TEST_F(RunSoftwareAnimationTest, SendsOnlyTheKeysThatChanged)
{
    RunSoftwareAnimation run{*session, clock, source()};
    ASSERT_TRUE(run.tick().has_value());
    const auto changedKey = G915KeyCatalog::keys()[5].id;
    frame = frame.with(changedKey, RgbColor{1, 2, 3});

    clock.advance(100ms);
    ASSERT_EQ(run.tick().value(), TickOutcome::Sent);

    ASSERT_EQ(keyboard.painted().size(), 2U);
    const auto& delta = keyboard.painted().back();
    EXPECT_EQ(delta.size(), 1U);
    EXPECT_EQ(delta.colorOf(changedKey), (RgbColor{1, 2, 3}));
}

TEST_F(RunSoftwareAnimationTest, IdenticalFrameSendsNothing)
{
    RunSoftwareAnimation run{*session, clock, source()};
    ASSERT_TRUE(run.tick().has_value());

    clock.advance(100ms);
    EXPECT_EQ(run.tick().value(), TickOutcome::Unchanged);

    EXPECT_EQ(keyboard.painted().size(), 1U);
}

TEST_F(RunSoftwareAnimationTest, LongPauseDoesNotCatchUpWithBurstOfFrames)
{
    RunSoftwareAnimation run{*session, clock, source()};
    ASSERT_TRUE(run.tick().has_value());
    frame = everyKey({0, 255, 0});

    clock.advance(10s);
    EXPECT_EQ(run.tick().value(), TickOutcome::Sent);
    EXPECT_EQ(run.tick().value(), TickOutcome::Skipped);

    EXPECT_EQ(keyboard.painted().size(), 2U);
}

TEST_F(RunSoftwareAnimationTest, StopReturnsControlToFirmwareOnceAndLaterTicksSendNothing)
{
    RunSoftwareAnimation run{*session, clock, source()};
    ASSERT_TRUE(run.tick().has_value());

    ASSERT_TRUE(run.stop().has_value());
    ASSERT_TRUE(run.stop().has_value());
    clock.advance(1s);
    const auto after = run.tick();

    ASSERT_FALSE(after.has_value());
    EXPECT_EQ(after.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(keyboard.painted().size(), 1U);
    int ends = 0;
    for (const auto& call : keyboard.calls()) {
        ends += call == "end" ? 1 : 0;
    }
    EXPECT_EQ(ends, 1);
    EXPECT_FALSE(session->isOpen());
}

TEST_F(RunSoftwareAnimationTest, PaintFailureIsReportedAndTheSessionIsClosed)
{
    RunSoftwareAnimation run{*session, clock, source()};
    keyboard.failPaint({ErrorCode::BackendUnavailable, "sem teclado"});

    const auto outcome = run.tick();

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().code, ErrorCode::BackendUnavailable);
    EXPECT_FALSE(session->isOpen());
    clock.advance(1s);
    EXPECT_FALSE(run.tick().has_value());
    EXPECT_EQ(keyboard.painted().size(), 1U);
}

} // namespace
