#include "modules/lighting/application/SaveKeyboardEffect.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeKeyboardLighting.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace {

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::LeaseHolder;
using ghub::device_access::PhysicalInterfaceId;
using ghub::lighting::application::SaveKeyboardEffect;
using ghub::lighting::domain::EffectBrightness;
using ghub::lighting::domain::EffectPeriod;
using ghub::lighting::domain::KeyboardEffect;
using ghub::lighting::domain::KeyboardEffectKind;
using ghub::lighting::domain::KeyboardZone;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;
using Calls = std::vector<std::string>;

PhysicalInterfaceId g915Interface()
{
    return PhysicalInterfaceId::create("hidpp:046d:407c").value(); // NOLINT(bugprone-unchecked-optional-access)
}

KeyboardEffect wave()
{
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    return KeyboardEffect::wave(EffectPeriod::create(5000).value(), EffectBrightness::create(100).value());
    // NOLINTEND(bugprone-unchecked-optional-access)
}

const std::string waveCall = "save:keys:" + std::to_string(static_cast<int>(KeyboardEffectKind::Wave));

class SaveKeyboardEffectTest : public ::testing::Test {
protected:
    SaveKeyboardEffectTest() { probe.setResult(std::vector<ExternalOwner>{}); }

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
    SaveKeyboardEffect useCase{keyboard, registry, probe, g915Interface()};
};

TEST_F(SaveKeyboardEffectTest, ConstructingTheUseCaseTouchesNothing)
{
    EXPECT_EQ(probe.calls(), 0);
    EXPECT_TRUE(keyboard.calls().empty());
}

TEST_F(SaveKeyboardEffectTest, SavesInTheExactOrderAndReleasesTheLease)
{
    const auto saved = useCase.save(KeyboardZone::Keys, wave());

    ASSERT_TRUE(saved.has_value()) << saved.error().detail;
    EXPECT_EQ(probe.calls(), 1);
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", waveCall, "end"}));
    ASSERT_EQ(keyboard.saved().size(), 1U);
    EXPECT_EQ(keyboard.saved().front(), wave());
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(SaveKeyboardEffectTest, TheLogoZoneIsPassedThrough)
{
    ASSERT_TRUE(useCase.save(KeyboardZone::Logo, KeyboardEffect::off()).has_value());

    EXPECT_EQ(keyboard.calls().at(2), "save:logo:0");
}

TEST_F(SaveKeyboardEffectTest, ExternalOwnerMakesItBusyWithoutOpening)
{
    for (const auto owner :
         {ExternalOwner::OpenRgb, ExternalOwner::Solaar, ExternalOwner::Piper, ExternalOwner::Ratbagd}) {
        probe.setResult(std::vector<ExternalOwner>{owner});

        const auto saved = useCase.save(KeyboardZone::Keys, wave());

        ASSERT_FALSE(saved.has_value());
        EXPECT_EQ(saved.error().code, ErrorCode::DeviceBusy);
    }
    EXPECT_TRUE(keyboard.calls().empty());
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(SaveKeyboardEffectTest, HeadsetControlDoesNotBlockTheKeyboard)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::HeadsetControl});

    EXPECT_TRUE(useCase.save(KeyboardZone::Keys, wave()).has_value());
}

TEST_F(SaveKeyboardEffectTest, ProbeFailureFailsWithoutOpening)
{
    probe.setResult(std::unexpected(DomainError{.code = ErrorCode::BackendUnavailable, .detail = "/proc"}));

    const auto saved = useCase.save(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(keyboard.calls().empty());
}

TEST_F(SaveKeyboardEffectTest, LeaseHeldByOtherMakesItBusyWithoutOpening)
{
    auto other = registry.acquire({
        .interfaceId = g915Interface(),
        .holder = LeaseHolder::create("outro").value(), // NOLINT(bugprone-unchecked-optional-access)
        .purpose = "teste",
        .maxDuration = 60s,
    });
    ASSERT_TRUE(other.has_value());

    const auto saved = useCase.save(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(keyboard.calls().empty());
}

TEST_F(SaveKeyboardEffectTest, OpenFailureStopsAndReleasesTheLease)
{
    keyboard.failOpen({.code = ErrorCode::BackendUnavailable, .detail = "teclado ausente"});

    const auto saved = useCase.save(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().detail, "teclado ausente");
    EXPECT_EQ(keyboard.calls(), (Calls{"open"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(SaveKeyboardEffectTest, BeginFailureDoesNotSaveButTriesToGiveControlBackOnce)
{
    keyboard.failBegin({.code = ErrorCode::BackendUnavailable, .detail = "begin falhou"});

    const auto saved = useCase.save(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().detail, "begin falhou");
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", "end"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(SaveKeyboardEffectTest, SaveFailureReturnsItsErrorAndEndsExactlyOnceWithoutResending)
{
    keyboard.failSave({.code = ErrorCode::InvalidArgument, .detail = "save falhou"});
    keyboard.failEnd({.code = ErrorCode::BackendUnavailable, .detail = "end falhou"});

    const auto saved = useCase.save(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(saved.error().detail, "save falhou");
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", waveCall, "end"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

TEST_F(SaveKeyboardEffectTest, EndFailureAloneIsReturned)
{
    keyboard.failEnd({.code = ErrorCode::BackendUnavailable, .detail = "end falhou"});

    const auto saved = useCase.save(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().detail, "end falhou");
    EXPECT_EQ(keyboard.calls(), (Calls{"open", "begin", waveCall, "end"}));
    EXPECT_TRUE(otherHolderCanAcquire());
}

} // namespace
