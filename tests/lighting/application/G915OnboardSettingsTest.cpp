#include "modules/lighting/application/G915OnboardSettings.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G915SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

// O caminho seguro genérico (recusas, cópia, releitura, restauração) é coberto em
// tests/onboard_memory/application/ActiveSectorEditorTest.cpp; aqui só o que é do G915: identidade do lease,
// interpretação do setor, a cópia como referência de "Original" e o byte a byte das edições (INC-019 R4, INC-022).
namespace ghub::lighting::application {
namespace {

using device_access::AccessRegistry;
using device_access::ExternalOwner;
using device_access::LeaseHolder;
using device_access::PhysicalInterfaceId;
using domain::G915ProfileSettings;
using domain::GKeyBinding;
using domain::KeyboardEffect;
using onboard_memory::OnboardSector;
using shared_kernel::ErrorCode;
namespace fx = fixtures::g915;
using Bytes = std::vector<std::uint8_t>;

template <std::size_t N> Bytes bytes(const std::array<std::uint8_t, N>& array)
{
    return {array.begin(), array.end()};
}

domain::KeyId key(const char* name)
{
    return *domain::G915KeyCatalog::findByName(name); // NOLINT(bugprone-unchecked-optional-access)
}

// Cópia que muda o setor ativo do teclado na segunda consulta (depois do read() do caso de uso): simula o usuário
// trocando o perfil entre a leitura e a gravação.
class SwitchingBackup final : public onboard_memory::OnboardBackupStore {
public:
    SwitchingBackup(fakes::FakeOnboardBackupStore& inner, fakes::FakeOnboardState& state) : inner_(&inner), state_(&state)
    {
    }
    shared_kernel::Result<std::optional<Bytes>> load(std::uint16_t sector) override
    {
        if (++loads_ == 2) {
            auto changed = state_->sectors[1];
            changed[0x22] ^= 0x01;
            state_->sectors[1] = OnboardSector::fromBytes(std::move(changed))->withCrc().bytes(); // CRC válido
        }
        return inner_->load(sector);
    }
    shared_kernel::Result<void> saveIfAbsent(std::uint16_t sector, std::span<const std::uint8_t> data) override
    {
        return inner_->saveIfAbsent(sector, data);
    }

private:
    fakes::FakeOnboardBackupStore* inner_;
    fakes::FakeOnboardState* state_;
    int loads_ = 0;
};

class G915OnboardSettingsTest : public ::testing::Test {
protected:
    G915OnboardSettingsTest()
    {
        connector.state.layout = {.sectorSize = 255, .profileCount = 3};
        connector.state.sectors[0] = bytes(fx::kDirectory);
        connector.state.sectors[1] = bytes(fx::kBeforeC04M1);
        connector.state.sectors[2] = bytes(fx::kBeforeC04M2);
        connector.state.sectors[3] = bytes(fx::kBeforeC04M3);
    }

    G915OnboardSettings useCase() { return G915OnboardSettings{probe, registry, connector, backup}; }

    [[nodiscard]] device_access::DeviceLease holdLease()
    {
        return registry
            .acquire({.interfaceId = PhysicalInterfaceId::create("hidpp:046d:407c").value(),
                      .holder = LeaseHolder::create("outro").value(),
                      .purpose = "teste",
                      .maxDuration = std::chrono::seconds{60}})
            .value();
    }

    [[nodiscard]] bool wroteNothing() const
    {
        return std::ranges::none_of(connector.state.calls,
                                    [](const std::string& call) { return call.starts_with("write:"); });
    }

    [[nodiscard]] G915ProfileSettings current() const
    {
        return G915ProfileSettings::read(bytes(fx::kBeforeC04M1)).value();
    }

    [[nodiscard]] G915ProfileSettings withG1(GKeyBinding binding) const
    {
        auto gKeys = current().gKeys();
        gKeys.at(0) = binding;
        return G915ProfileSettings::make(current().logo(), current().keys(), gKeys).value();
    }

    fakes::FakeClock clock;
    AccessRegistry registry{clock};
    fakes::FakeExternalProcessProbe probe;
    fakes::FakeOnboardMemoryConnector connector;
    fakes::FakeOnboardBackupStore backup;
};

} // namespace

TEST_F(G915OnboardSettingsTest, UsesTheKeyboardLeaseSoTheLiveModeOrAnotherHolderBlocksEveryOperation)
{
    const auto other = holdLease();

    EXPECT_EQ(useCase().write(current()).error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(useCase().read().error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(useCase().restoreBackup().error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G915OnboardSettingsTest, RefusesWhenAnExternalOwnerIsRunningWithoutOpeningTheNode)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Solaar});

    const auto result = useCase().write(current());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "em uso por solaar");
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G915OnboardSettingsTest, ReadReturnsTheInterpretedActiveProfileWithoutWritingAnything)
{
    const auto snapshot = useCase().read();

    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->sector, 1U);
    EXPECT_EQ(snapshot->settings, current());
    EXPECT_FALSE(snapshot->backupExists);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G915OnboardSettingsTest, ReadFailsWhenTheActiveSectorIsNotAG915Profile)
{
    Bytes raw = bytes(fx::kBeforeC04M1);
    raw[0] = 0x07;
    connector.state.sectors[1] = OnboardSector::fromBytes(std::move(raw))->withCrc().bytes();

    const auto snapshot = useCase().read();

    ASSERT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error().code, ErrorCode::MalformedData);
}

TEST_F(G915OnboardSettingsTest, WritingG1AsCtrlPlusCStoresExactly80020106WithValidCrcAndKeepsEverythingElse)
{
    const auto binding = GKeyBinding::create(GKeyBinding::kCtrl, key("Key: C")).value();

    const auto result = useCase().write(withG1(binding));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->gKeys().at(0), binding);
    const auto& after = connector.state.sectors.at(1);
    auto expected = bytes(fx::kBeforeC04M1);
    expected[0x22] = 0x01;
    expected[0x23] = 0x06;
    EXPECT_EQ(after, OnboardSector::fromBytes(expected)->withCrc().bytes());
    EXPECT_TRUE(OnboardSector::fromBytes(after)->hasValidCrc());
    EXPECT_EQ(connector.state.count("write:1"), 1);
    EXPECT_EQ(connector.state.count("write:2"), 0);
    EXPECT_EQ(connector.state.count("write:0"), 0);
    EXPECT_EQ(backup.stored.at(1), bytes(fx::kBeforeC04M1)) << "cópia automática do setor antes da gravação";
    EXPECT_EQ(connector.state.sectors.at(0), bytes(fx::kDirectory));
}

TEST_F(G915OnboardSettingsTest, WritingTheLightingEffectsReproducesTheBlockTheGHubWrote)
{
    const auto edited = G915ProfileSettings::make(
                            KeyboardEffect::cycle(domain::EffectPeriod::create(5000).value(),
                                                  domain::EffectBrightness::create(100).value()),
                            KeyboardEffect::wave(domain::EffectPeriod::create(5000).value(),
                                                 domain::EffectBrightness::create(100).value()),
                            current().gKeys())
                            .value();

    const auto result = useCase().write(edited);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->logo(), edited.logo());
    EXPECT_EQ(result->keys(), edited.keys());
    const auto& after = connector.state.sectors.at(1);
    const auto target = bytes(fx::kAfterC06M1);
    EXPECT_EQ((Bytes{after.begin() + 0xD0, after.begin() + 0xFC}), (Bytes{target.begin() + 0xD0, target.begin() + 0xFC}));
}

TEST_F(G915OnboardSettingsTest, WritingTheValuesAlreadyOnTheKeyboardSendsNothing)
{
    const auto result = useCase().write(current());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, current());
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G915OnboardSettingsTest, OriginalBindingUsesTheBackupAsReference)
{
    // O teclado está com o perfil do G HUB (G1 = Ctrl+C); a cópia guarda o M1 original (G1 = F1).
    connector.state.sectors[1] = bytes(fx::kAfterC06M1);
    backup.stored[1] = bytes(fx::kBeforeC04M1);
    const auto now = G915ProfileSettings::read(bytes(fx::kAfterC06M1)).value();
    auto gKeys = now.gKeys();
    gKeys.at(0) = GKeyBinding::original();

    const auto result = useCase().write(G915ProfileSettings::make(now.logo(), now.keys(), gKeys).value());

    ASSERT_TRUE(result.has_value());
    const auto& after = connector.state.sectors.at(1);
    EXPECT_EQ((Bytes{after.begin() + 0x20, after.begin() + 0x24}), (Bytes{0x80, 0x02, 0x00, 0x3A}));
    EXPECT_EQ(backup.stored.at(1), bytes(fx::kBeforeC04M1)) << "a cópia original não é sobrescrita";
}

TEST_F(G915OnboardSettingsTest, OriginalBindingWithoutBackupMeansTheCurrentValue)
{
    const auto result = useCase().write(withG1(GKeyBinding::original()));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G915OnboardSettingsTest, RefusesToWriteWhenTheActiveProfileChangedBetweenTheReadAndTheWrite)
{
    SwitchingBackup switching{backup, connector.state};
    const G915OnboardSettings racing{probe, registry, connector, switching};

    const auto result = racing.write(withG1(GKeyBinding::create(GKeyBinding::kShift, key("Key: Z")).value()));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "o perfil ativo do teclado mudou desde a leitura; leia de novo");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G915OnboardSettingsTest, RawBindingThatIsNotInTheProfileIsRefusedBeforeWriting)
{
    const auto result = useCase().write(withG1(GKeyBinding::raw({0x90, 0x0D, 0xFF, 0x01})));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

// --- restauração da cópia ---

class G915RestoreTest : public G915OnboardSettingsTest {
protected:
    G915RestoreTest()
    {
        // O teclado está com a edição; a cópia guarda o original.
        connector.state.sectors[1] = bytes(fx::kAfterC06M1);
        backup.stored[1] = bytes(fx::kBeforeC04M1);
    }
};

TEST_F(G915RestoreTest, RestoresTheBackupWithTheSameWriteAndVerifySequence)
{
    backup.log = &connector.state.calls;

    const auto result = useCase().restoreBackup();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, current());
    EXPECT_EQ(connector.state.sectors.at(1), bytes(fx::kBeforeC04M1));
    EXPECT_EQ(connector.state.calls, (std::vector<std::string>{"open", "describe", "mode", "current", "read:0",
                                                               "read:1", "write:1", "read:1"}));
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G915RestoreTest, RefusesABackupThatIsNotAG915Profile)
{
    Bytes foreign = bytes(fx::kBeforeC04M1);
    foreign[0] = 0x09;
    backup.stored[1] = OnboardSector::fromBytes(std::move(foreign))->withCrc().bytes();

    const auto result = useCase().restoreBackup();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G915RestoreTest, WithoutBackupNothingIsWritten)
{
    backup.stored.clear();

    const auto result = useCase().restoreBackup();

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(wroteNothing());
}

} // namespace ghub::lighting::application
