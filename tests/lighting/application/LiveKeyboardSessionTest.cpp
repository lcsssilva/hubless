#include "modules/lighting/application/LiveKeyboardSession.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeKeyboardLighting.hpp"
#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace {

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::LeaseHolder;
using ghub::device_access::PhysicalInterfaceId;
using ghub::lighting::application::StartLiveKeyboardSession;
using ghub::lighting::domain::EffectBrightness;
using ghub::lighting::domain::EffectPeriod;
using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::KeyboardEffect;
using ghub::lighting::domain::KeyboardZone;
using ghub::lighting::domain::KeyColorMap;
using ghub::lighting::domain::RgbColor;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;
using Calls = std::vector<std::string>;

PhysicalInterfaceId g915Interface()
{
    return PhysicalInterfaceId::create("hidpp:046d:407c").value(); // NOLINT(bugprone-unchecked-optional-access)
}

KeyColorMap oneKey()
{
    const auto key = G915KeyCatalog::findByName("Key: A");
    return KeyColorMap{}.with(*key, RgbColor{10, 20, 30}); // NOLINT(bugprone-unchecked-optional-access)
}

KeyboardEffect waveEffect()
{
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return KeyboardEffect::wave(*EffectPeriod::create(5000), *EffectBrightness::create(100));
}

class LiveKeyboardSessionTest : public ::testing::Test {
protected:
    LiveKeyboardSessionTest() { probe.setResult(std::vector<ExternalOwner>{}); }

    // Tenta tomar o lease do teclado como outro dono; sucesso = a interface estava livre.
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
    ghub::fakes::FakeKeyboardLighting keyboard;
    StartLiveKeyboardSession useCase{keyboard, registry, probe, g915Interface()};
};

TEST_F(LiveKeyboardSessionTest, ConstructingTheUseCaseTouchesNothing)
{
    EXPECT_EQ(probe.calls(), 0);
    EXPECT_TRUE(keyboard.calls().empty());
}

TEST_F(LiveKeyboardSessionTest, StartChecksProcessesThenOpensAndTakesControlUnderLease)
{
    auto session = useCase.start();

    ASSERT_TRUE(session.has_value()) << session.error().detail;
    EXPECT_TRUE((*session)->isOpen());
    EXPECT_EQ(probe.calls(), 1);
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin"}));
    EXPECT_FALSE(otherHolderCanAcquire());
}

TEST_F(LiveKeyboardSessionTest, ExternalOwnerOfReceiversMakesItBusyWithoutOpening)
{
    for (const auto owner :
         {ExternalOwner::OpenRgb, ExternalOwner::Solaar, ExternalOwner::Piper, ExternalOwner::Ratbagd}) {
        probe.setResult(std::vector<ExternalOwner>{owner});

        const auto session = useCase.start();

        ASSERT_FALSE(session.has_value());
        EXPECT_EQ(session.error().code, ErrorCode::DeviceBusy);
        EXPECT_NE(session.error().detail.find(std::string{ghub::device_access::processName(owner)}), std::string::npos);
    }
    EXPECT_TRUE(keyboard.calls().empty());
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(LiveKeyboardSessionTest, HeadsetControlDoesNotBlockTheKeyboard)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::HeadsetControl});

    const auto session = useCase.start();

    EXPECT_TRUE(session.has_value());
}

TEST_F(LiveKeyboardSessionTest, ProbeFailureFailsWithoutOpening)
{
    probe.setResult(std::unexpected(DomainError{.code = ErrorCode::BackendUnavailable, .detail = "/proc"}));

    const auto session = useCase.start();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(keyboard.calls().empty());
}

TEST_F(LiveKeyboardSessionTest, LeaseHeldByOtherMakesItBusyWithoutOpening)
{
    auto other = registry.acquire({
        .interfaceId = g915Interface(),
        .holder = LeaseHolder::create("outro").value(), // NOLINT(bugprone-unchecked-optional-access)
        .purpose = "teste",
        .maxDuration = 60s,
    });
    ASSERT_TRUE(other.has_value());

    const auto session = useCase.start();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(keyboard.calls().empty());
}

TEST_F(LiveKeyboardSessionTest, OpenFailureReleasesTheLeaseWithoutSendingAnything)
{
    keyboard.failOpen({.code = ErrorCode::BackendUnavailable, .detail = "teclado ausente"});

    const auto session = useCase.start();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().detail, "teclado ausente");
    EXPECT_EQ(keyboard.calls(), (Calls{"open"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(LiveKeyboardSessionTest, BeginFailureTriesOnceToGiveControlBack)
{
    keyboard.failBegin({.code = ErrorCode::BackendUnavailable, .detail = "sem resposta"});

    const auto session = useCase.start();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().detail, "sem resposta");
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(LiveKeyboardSessionTest, PaintSendsTheMapToTheKeyboard)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());

    const auto painted = (*session)->paint(oneKey());

    EXPECT_TRUE(painted.has_value());
    ASSERT_EQ(keyboard.painted().size(), 1U);
    EXPECT_EQ(keyboard.painted().front(), oneKey());
}

TEST_F(LiveKeyboardSessionTest, DestroyingTheSessionGivesControlBackOnceAndReleasesTheLease)
{
    {
        auto session = useCase.start();
        ASSERT_TRUE(session.has_value());
    }

    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(LiveKeyboardSessionTest, ExplicitEndIsNotRepeatedByLaterCallsOrTheDestructor)
{
    {
        auto session = useCase.start();
        ASSERT_TRUE(session.has_value());

        EXPECT_TRUE((*session)->end().has_value());
        EXPECT_FALSE((*session)->isOpen());
        EXPECT_TRUE((*session)->end().has_value());
        const auto painted = (*session)->paint(oneKey());
        ASSERT_FALSE(painted.has_value());
        EXPECT_EQ(painted.error().code, ErrorCode::InvalidArgument);
    }

    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
}

TEST_F(LiveKeyboardSessionTest, EndFailureIsReportedAndNotRetried)
{
    keyboard.failEnd({.code = ErrorCode::BackendUnavailable, .detail = "falhou"});
    {
        auto session = useCase.start();
        ASSERT_TRUE(session.has_value());

        const auto ended = (*session)->end();

        ASSERT_FALSE(ended.has_value());
        EXPECT_EQ(ended.error().detail, "falhou");
    }

    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
}

TEST_F(LiveKeyboardSessionTest, PaintFailureGivesControlBackOnceWithoutResending)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());
    keyboard.failPaint({.code = ErrorCode::BackendUnavailable, .detail = "erro HID++"});

    const auto painted = (*session)->paint(oneKey());

    ASSERT_FALSE(painted.has_value());
    EXPECT_EQ(painted.error().detail, "erro HID++");
    EXPECT_FALSE((*session)->isOpen());
    EXPECT_FALSE((*session)->paint(oneKey()).has_value());
    session->reset();
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "paint", "end"}));
}

TEST_F(LiveKeyboardSessionTest, EachPaintRenewsTheLeaseBeyondItsDuration)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());

    for (int i = 0; i < 4; ++i) {
        clock.advance(10s); // 40 s no total, além dos 15 s de um lease
        const auto painted = (*session)->paint(oneKey());
        ASSERT_TRUE(painted.has_value()) << painted.error().detail;
        EXPECT_FALSE(otherHolderCanAcquire());
    }
}

TEST_F(LiveKeyboardSessionTest, RevokedLeaseFailsThePaintWithoutWritingAndStillGivesControlBack)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());

    registry.systemSuspending();
    const auto painted = (*session)->paint(oneKey());

    ASSERT_FALSE(painted.has_value());
    EXPECT_EQ(painted.error().code, ErrorCode::LeaseRevoked);
    EXPECT_TRUE(keyboard.painted().empty());
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
}

TEST_F(LiveKeyboardSessionTest, ExpiredLeaseTakenByOtherStopsEverythingWithoutWriting)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());
    clock.advance(16s); // o lease da sessão expira e outro dono o toma
    auto other = registry.acquire({
        .interfaceId = g915Interface(),
        .holder = LeaseHolder::create("outro").value(), // NOLINT(bugprone-unchecked-optional-access)
        .purpose = "teste",
        .maxDuration = 60s,
    });
    ASSERT_TRUE(other.has_value());

    const auto painted = (*session)->paint(oneKey());
    session->reset();

    ASSERT_FALSE(painted.has_value());
    EXPECT_EQ(painted.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin"}));
}

TEST_F(LiveKeyboardSessionTest, ApplyEffectForwardsZoneAndEffectAndKeepsTheSessionOpen)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());

    const auto applied = (*session)->applyEffect(KeyboardZone::Keys, waveEffect());

    EXPECT_TRUE(applied.has_value());
    EXPECT_TRUE((*session)->isOpen());
    ASSERT_EQ(keyboard.saved().size(), 1U);
    EXPECT_EQ(keyboard.saved().front(), waveEffect());
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "save:keys:4"}));
    EXPECT_FALSE(otherHolderCanAcquire());
}

TEST_F(LiveKeyboardSessionTest, ApplyEffectFailureGivesControlBackOnceAndReportsTheOriginalError)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());
    keyboard.failSave({.code = ErrorCode::BackendUnavailable, .detail = "erro HID++"});

    const auto applied = (*session)->applyEffect(KeyboardZone::Keys, waveEffect());

    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().detail, "erro HID++");
    EXPECT_FALSE((*session)->isOpen());
    session->reset();
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "save:keys:4", "end"}));
}

TEST_F(LiveKeyboardSessionTest, ApplyEffectWithRevokedLeaseDoesNotWrite)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());

    registry.systemSuspending();
    const auto applied = (*session)->applyEffect(KeyboardZone::Keys, waveEffect());

    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::LeaseRevoked);
    EXPECT_TRUE(keyboard.saved().empty());
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
}

TEST_F(LiveKeyboardSessionTest, ApplyEffectRenewsTheLeaseBeyondItsDuration)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());

    for (int i = 0; i < 3; ++i) {
        clock.advance(10s);
        const auto applied = (*session)->applyEffect(KeyboardZone::Keys, waveEffect());
        ASSERT_TRUE(applied.has_value()) << applied.error().detail;
        EXPECT_FALSE(otherHolderCanAcquire());
    }
}

TEST_F(LiveKeyboardSessionTest, ApplyEffectAfterEndIsInvalidAndSendsNothing)
{
    auto session = useCase.start();
    ASSERT_TRUE(session.has_value());
    ASSERT_TRUE((*session)->end().has_value());

    const auto applied = (*session)->applyEffect(KeyboardZone::Keys, waveEffect());

    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
}

TEST_F(LiveKeyboardSessionTest, CheckControlWhenLostClosesWithoutSendingAndReleasesTheLease)
{
    auto started = useCase.start();
    ASSERT_TRUE(started.has_value());
    {
        auto& session = **started;
        keyboard.setControlLost(true);

        const auto lost = session.checkControl();

        ASSERT_TRUE(lost.has_value()) << lost.error().detail;
        EXPECT_TRUE(*lost);
        EXPECT_FALSE(session.isOpen());
        EXPECT_TRUE(otherHolderCanAcquire());
    }
    started->reset(); // o destrutor também não envia nada
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin"}));
}

TEST_F(LiveKeyboardSessionTest, CheckControlWhenKeptLeavesTheSessionOpen)
{
    auto started = useCase.start();
    ASSERT_TRUE(started.has_value());

    const auto lost = (*started)->checkControl();

    ASSERT_TRUE(lost.has_value());
    EXPECT_FALSE(*lost);
    EXPECT_TRUE((*started)->isOpen());
    EXPECT_FALSE(otherHolderCanAcquire());
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin"}));
}

TEST_F(LiveKeyboardSessionTest, CheckControlReadFailureIsReportedAndTheSessionStaysOpen)
{
    auto started = useCase.start();
    ASSERT_TRUE(started.has_value());
    keyboard.failControlCheck(true);

    const auto lost = (*started)->checkControl();

    ASSERT_FALSE(lost.has_value());
    EXPECT_EQ(lost.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE((*started)->isOpen());
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin"}));
}

TEST_F(LiveKeyboardSessionTest, CheckControlAfterEndIsFalseAndAsksNothing)
{
    auto started = useCase.start();
    ASSERT_TRUE(started.has_value());
    ASSERT_TRUE((*started)->end().has_value());
    const int before = keyboard.controlChecks();

    const auto lost = (*started)->checkControl();

    ASSERT_TRUE(lost.has_value());
    EXPECT_FALSE(*lost);
    EXPECT_EQ(keyboard.controlChecks(), before);
}

} // namespace
