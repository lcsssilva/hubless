#include "modules/pointer/application/G703OnboardSettings.hpp"
#include "../domain/G703SectorFixtures.hpp"
#include "FakeOnboardMemory.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace ghub::pointer {
namespace {

using device_access::AccessRegistry;
using device_access::ExternalOwner;
using device_access::LeaseHolder;
using device_access::LeaseRequest;
using device_access::PhysicalInterfaceId;
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
    return G703ProfileSettings::make(1000, {7050, 1600}, 0).value();
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

// --- recusas antes de tocar no dispositivo ---

TEST_F(G703OnboardSettingsTest, WriteRefusesWhenExternalOwnerRunningWithoutOpeningTheNode)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Ratbagd});
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(result.error().detail, "em uso por ratbagd");
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G703OnboardSettingsTest, WriteRefusesWhenProbeFailsWithoutOpeningTheNode)
{
    probe.setResult(shared_kernel::fail(ErrorCode::BackendUnavailable, "sem /proc"));
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "sem /proc");
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G703OnboardSettingsTest, WriteRefusesWhenLeaseIsHeldByAnotherOwner)
{
    const auto other = holdLease();
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G703OnboardSettingsTest, ReadAndRestoreAlsoRefuseWhenExternalOwnerRunning)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Piper});
    EXPECT_EQ(useCase().read().error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(useCase().restoreBackup().error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(G703OnboardSettingsTest, OpensTheNodeOnlyDuringTheOperationAndReleasesTheLease)
{
    // Um resultado de sucesso qualquer: a leitura feliz entra no próximo ciclo; aqui só o ciclo de vida.
    (void)useCase().write(twoStages());
    EXPECT_EQ(connector.state.opens, 1);
    EXPECT_EQ(connector.state.liveMemories, 0);
    EXPECT_TRUE(holdLease().verify().has_value()); // o lease foi liberado
}

// --- recusas pelo estado do mouse: nenhuma escrita ---

TEST_F(G703OnboardSettingsTest, RefusesWhenSectorSizeIsImplausible)
{
    connector.state.layout.sectorSize = 8;
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703OnboardSettingsTest, RefusesWhenDescribeFails)
{
    connector.state.describeError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "sem resposta"};
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem resposta");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703OnboardSettingsTest, RefusesWhenMouseIsNotInOnboardMode)
{
    connector.state.mode = 2;
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "mouse fora do modo onboard");
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703OnboardSettingsTest, RefusesActiveSectorOutsideOneToProfileCount)
{
    for (const std::uint16_t current : {std::uint16_t{0}, std::uint16_t{6}, std::uint16_t{0x0101}}) {
        connector.state.currentSector = current;
        const auto result = useCase().write(twoStages());
        ASSERT_FALSE(result.has_value()) << current;
        EXPECT_EQ(result.error().code, ErrorCode::MalformedData) << current;
    }
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703OnboardSettingsTest, RefusesActiveSectorDisabledInTheDirectory)
{
    connector.state.currentSector = 2;
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703OnboardSettingsTest, RefusesDirectoryWithInvalidCrc)
{
    connector.state.sectors[0][2] = 0x00; // estraga a entrada sem refazer o CRC
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703OnboardSettingsTest, RefusesActiveSectorWithInvalidCrc)
{
    connector.state.sectors[1][0] = 0x02; // estraga a taxa sem refazer o CRC
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703OnboardSettingsTest, RefusesSectorWhoseLengthDiffersFromTheLayout)
{
    connector.state.sectors[1].pop_back();
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703OnboardSettingsTest, ReadRefusesWithTheSameSafetyChecks)
{
    connector.state.mode = 2;
    EXPECT_EQ(useCase().read().error().detail, "mouse fora do modo onboard");
    connector.state.mode = 1;
    connector.state.currentSector = 2;
    EXPECT_EQ(useCase().read().error().code, ErrorCode::MalformedData);
}

// --- leitura ---

TEST_F(G703OnboardSettingsTest, ReadReturnsTheActiveProfileWithoutWritingAnything)
{
    const auto snapshot = useCase().read();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->sector, 1U);
    EXPECT_EQ(snapshot->settings, G703ProfileSettings::make(1000, {7050}, 0).value());
    EXPECT_FALSE(snapshot->backupExists);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
    EXPECT_EQ(connector.state.liveMemories, 0);
}

TEST_F(G703OnboardSettingsTest, ReadReportsAnExistingBackup)
{
    backup.stored[1] = sector0001();
    const auto snapshot = useCase().read();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_TRUE(snapshot->backupExists);
}

TEST_F(G703OnboardSettingsTest, ReadFailsWhenTheBackupStoreFails)
{
    backup.loadError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "disco"};
    const auto snapshot = useCase().read();
    ASSERT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error().detail, "disco");
}

// --- gravação ---

TEST_F(G703OnboardSettingsTest, WriteFollowsTheExactSequenceAndBacksUpTheOriginalBytesFirst)
{
    backup.log = &connector.state.calls;
    const auto result = useCase().write(twoStages());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(connector.state.calls, (std::vector<std::string>{"open", "describe", "mode", "current", "read:0",
                                                               "read:1", "backup:1", "write:1", "read:1"}));
    EXPECT_EQ(backup.stored.at(1), sector0001());
}

TEST_F(G703OnboardSettingsTest, WriteStoresTheEditedSectorAndReturnsWhatWasReadBack)
{
    const auto expected = twoStages().applyTo(*OnboardSector::fromBytes(sector0001()));
    ASSERT_TRUE(expected.has_value());
    const auto result = useCase().write(twoStages());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, twoStages());
    EXPECT_EQ(connector.state.sectors.at(1), expected->bytes());
    EXPECT_EQ(connector.state.count("write:1"), 1);
    EXPECT_EQ(connector.state.sectors.at(0), directory()); // o diretório nunca é tocado
    EXPECT_EQ(connector.state.liveMemories, 0);
}

TEST_F(G703OnboardSettingsTest, WriteKeepsAnExistingBackupInsteadOfOverwritingIt)
{
    const Bytes older = sector0002();
    backup.stored[1] = older;
    ASSERT_TRUE(useCase().write(twoStages()).has_value());
    EXPECT_EQ(backup.saveCalls, 1);
    EXPECT_EQ(backup.stored.at(1), older);
}

TEST_F(G703OnboardSettingsTest, WriteDoesNotWriteWhenTheBackupFails)
{
    backup.saveError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "disco cheio"};
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "disco cheio");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703OnboardSettingsTest, WriteWithUnchangedSettingsSendsNothing)
{
    const auto current = G703ProfileSettings::make(1000, {7050}, 0).value();
    const auto result = useCase().write(current);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, current);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703OnboardSettingsTest, WriteRefusesSettingsThatWouldDangleTheShiftIndex)
{
    connector.state.sectors[1] = OnboardSector::fromBytes([] {
                                     auto raw = sector0001();
                                     raw[2] = 3;
                                     return raw;
                                 }())
                                     ->withCrc()
                                     .bytes();
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703OnboardSettingsTest, WriteFailureIsReportedWithASingleAttemptAndNoRereadOrRetry)
{
    connector.state.writeError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "fn7 falhou"};
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "fn7 falhou");
    EXPECT_EQ(connector.state.count("write:1"), 1);
    EXPECT_EQ(connector.state.count("read:1"), 1); // só a leitura anterior à escrita
    EXPECT_EQ(connector.state.sectors.at(1), sector0001());
}

TEST_F(G703OnboardSettingsTest, WriteReportsADifferingRereadAndDoesNotRewrite)
{
    connector.state.postWriteOverride = sector0002();
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(result.error().detail, "mouse com dados diferentes do enviado — use Restaurar cópia");
    EXPECT_EQ(connector.state.count("write:1"), 1);
}

TEST_F(G703OnboardSettingsTest, WriteDoesNotWriteWhenTheLeaseExpiredAfterTheBackup)
{
    backup.onSave = [this] { clock.advance(std::chrono::seconds{16}); };
    const auto result = useCase().write(twoStages());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::LeaseExpired);
    EXPECT_TRUE(wroteNothing());
}

// --- restauração da cópia ---

class G703RestoreTest : public G703OnboardSettingsTest {
protected:
    G703RestoreTest()
    {
        // O mouse está com a edição; a cópia guarda o original.
        connector.state.sectors[1] = twoStages().applyTo(*OnboardSector::fromBytes(sector0001()))->bytes();
        backup.stored[1] = sector0001();
    }
};

TEST_F(G703RestoreTest, RestoresTheBackupWithTheSameWriteAndVerifySequence)
{
    backup.log = &connector.state.calls;
    const auto result = useCase().restoreBackup();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, G703ProfileSettings::make(1000, {7050}, 0).value());
    EXPECT_EQ(connector.state.sectors.at(1), sector0001());
    EXPECT_EQ(connector.state.calls, (std::vector<std::string>{"open", "describe", "mode", "current", "read:0",
                                                               "read:1", "write:1", "read:1"}));
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(G703RestoreTest, RefusesWhenThereIsNoBackup)
{
    backup.stored.clear();
    const auto result = useCase().restoreBackup();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "sem cópia de segurança");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703RestoreTest, RefusesBackupWithInvalidCrc)
{
    backup.stored[1][0] = 0x02;
    const auto result = useCase().restoreBackup();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
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

TEST_F(G703RestoreTest, RefusesBackupWithTheWrongSize)
{
    backup.stored[1].pop_back();
    const auto result = useCase().restoreBackup();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703RestoreTest, DoesNotWriteWhenTheMouseAlreadyHoldsTheBackup)
{
    connector.state.sectors[1] = sector0001();
    const auto result = useCase().restoreBackup();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703RestoreTest, ReportsADifferingRereadWithoutRewriting)
{
    connector.state.postWriteOverride = sector0002();
    const auto result = useCase().restoreBackup();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "mouse com dados diferentes do enviado — use Restaurar cópia");
    EXPECT_EQ(connector.state.count("write:1"), 1);
}

TEST_F(G703RestoreTest, ReportsBackupStoreFailureWithoutWriting)
{
    backup.loadError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "disco"};
    const auto result = useCase().restoreBackup();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "disco");
    EXPECT_TRUE(wroteNothing());
}

} // namespace
} // namespace ghub::pointer
