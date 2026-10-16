#include "modules/pointer/application/G703OnboardSettings.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

// O caminho seguro genérico (recusas, cópia, releitura, restauração) é coberto em
// tests/onboard_memory/application/ActiveSectorEditorTest.cpp; aqui só o que é do G703: identidade do lease,
// interpretação do setor e o byte a byte das edições (INC-020 R3).
namespace ghub::pointer {
namespace {

using device_access::AccessRegistry;
using device_access::ExternalOwner;
using device_access::LeaseHolder;
using device_access::PhysicalInterfaceId;
using onboard_memory::OnboardSector;
using shared_kernel::ErrorCode;
using Bytes = std::vector<std::uint8_t>;

Bytes sector0001()
{
    return {fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
}
Bytes sector0002()
{
    return {fixtures::kSector0002.begin(), fixtures::kSector0002.end()};
}
Bytes directory()
{
    return {fixtures::kDirectory0000.begin(), fixtures::kDirectory0000.end()};
}

G703ProfileSettings twoStages()
{
    return G703ProfileSettings::make(1000, {7050, 1600}, 0, 0, LedSetting::off(), LedSetting::off()).value();
}

class G703OnboardSettingsTest : public ::testing::Test {
protected:
    G703OnboardSettingsTest()
    {
        connector.state.sectors[0] = directory();
        connector.state.sectors[1] = sector0001();
        connector.state.sectors[2] = sector0002();
    }

    G703OnboardSettings useCase() { return G703OnboardSettings{probe, registry, connector, backup}; }

    // Segura o lease do G703 em outro dono.
    [[nodiscard]] device_access::DeviceLease holdLease()
    {
        return registry
            .acquire({.interfaceId = PhysicalInterfaceId::create("hidpp:046d:c539").value(),
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

    fakes::FakeClock clock;
    AccessRegistry registry{clock};
    fakes::FakeExternalProcessProbe probe;
    fakes::FakeOnboardMemoryConnector connector;
    fakes::FakeOnboardBackupStore backup;
};

TEST_F(G703OnboardSettingsTest, UsesTheG703ReceiverLeaseSoAnotherHolderBlocksEveryOperation)
{
    const auto other = holdLease();
    EXPECT_EQ(useCase().write(twoStages()).error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(useCase().read().error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(useCase().restoreBackup().error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G703OnboardSettingsTest, RefusesWhenExternalOwnerRunningWithoutOpeningTheNode)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Ratbagd});
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "em uso por ratbagd");
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G703OnboardSettingsTest, ReadReturnsTheInterpretedActiveProfileWithoutWritingAnything)
{
    const auto snapshot = useCase().read();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->sector, 1U);
    EXPECT_EQ(snapshot->settings,
              G703ProfileSettings::make(1000, {7050}, 0, 0, LedSetting::off(), LedSetting::off()).value());
    EXPECT_FALSE(snapshot->backupExists);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703OnboardSettingsTest, ReadFailsWhenTheActiveSectorIsNotAG703Profile)
{
    Bytes raw = sector0001();
    raw[0] = 3; // intervalo inválido, com CRC refeito
    connector.state.sectors[1] = OnboardSector::fromBytes(std::move(raw))->withCrc().bytes();
    const auto snapshot = useCase().read();
    ASSERT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error().code, ErrorCode::MalformedData);
}

TEST_F(G703OnboardSettingsTest, WriteStoresTheEditedSectorWithValidCrcAndReturnsWhatWasReadBack)
{
    const auto expected = twoStages().applyTo(sector0001());
    ASSERT_TRUE(expected.has_value());
    const auto result = useCase().write(twoStages());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, twoStages());
    EXPECT_EQ(connector.state.sectors.at(1), OnboardSector::fromBytes(*expected)->withCrc().bytes());
    EXPECT_EQ(connector.state.count("write:1"), 1);
    EXPECT_EQ(backup.stored.at(1), sector0001());
    EXPECT_EQ(connector.state.sectors.at(0), directory());
}

TEST_F(G703OnboardSettingsTest, WriteWithUnchangedSettingsSendsNothing)
{
    const auto current = G703ProfileSettings::read(sector0001()).value();
    const auto result = useCase().write(current);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, current);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

// INC-020 R1-R3: estágios, padrão, shift, taxa e os dois LEDs gravados juntos; nenhum outro byte do setor muda.
TEST_F(G703OnboardSettingsTest, WriteOfStagesShiftRateAndLedsChangesNoOtherByteOfTheRealSector)
{
    const auto edit = G703ProfileSettings::make(500, {400, 1600, 3200}, 2, 1, LedSetting::fixed(0, 255, 255),
                                                *LedSetting::cycle(5000, 80));
    ASSERT_TRUE(edit.has_value());
    const auto result = useCase().write(*edit);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, *edit);

    const auto& after = connector.state.sectors.at(1);
    const auto before = sector0001();
    ASSERT_EQ(after.size(), before.size());
    for (std::size_t i = 0; i < before.size() - 2; ++i) {
        const bool editable = i <= 12 || (i >= 0xD0 && i < 0xD0 + 22);
        if (!editable) {
            EXPECT_EQ(after[i], before[i]) << "byte " << i;
        }
    }
    EXPECT_TRUE(OnboardSector::fromBytes(after)->hasValidCrc());
}

TEST_F(G703OnboardSettingsTest, WriteOfTheValuesAlreadyOnTheMouseSendsNothing)
{
    const auto current = G703ProfileSettings::read(sector0001()).value();
    const auto same = G703ProfileSettings::make(current.reportRateHz(), current.dpiStages(), current.defaultStage(),
                                                current.dpiShiftStage(), LedSetting::off(), LedSetting::off());
    ASSERT_TRUE(same.has_value());
    ASSERT_TRUE(useCase().write(*same).has_value());
    EXPECT_TRUE(wroteNothing());
}

// --- restauração da cópia ---

class G703RestoreTest : public G703OnboardSettingsTest {
protected:
    G703RestoreTest()
    {
        // O mouse está com a edição; a cópia guarda o original.
        connector.state.sectors[1] = OnboardSector::fromBytes(*twoStages().applyTo(sector0001()))->withCrc().bytes();
        backup.stored[1] = sector0001();
    }
};

TEST_F(G703RestoreTest, RestoresTheBackupWithTheSameWriteAndVerifySequence)
{
    backup.log = &connector.state.calls;
    const auto result = useCase().restoreBackup();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, G703ProfileSettings::read(sector0001()).value());
    EXPECT_EQ(connector.state.sectors.at(1), sector0001());
    EXPECT_EQ(connector.state.calls, (std::vector<std::string>{"open", "describe", "mode", "current", "read:0",
                                                               "read:1", "write:1", "read:1"}));
    EXPECT_EQ(backup.saveCalls, 0);
}

// CRC válido não basta: uma cópia que o domínio não lê (report rate 3 ms) não é gravada.
TEST_F(G703RestoreTest, RefusesBackupThatTheDomainCannotReadEvenWithValidCrc)
{
    backup.stored[1][0] = 0x03;
    backup.stored[1] = OnboardSector::fromBytes(backup.stored[1])->withCrc().bytes();
    const auto result = useCase().restoreBackup();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703RestoreTest, RefusesWhenThereIsNoBackup)
{
    backup.stored.clear();
    const auto result = useCase().restoreBackup();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem cópia de segurança");
    EXPECT_TRUE(wroteNothing());
}

} // namespace
} // namespace ghub::pointer
