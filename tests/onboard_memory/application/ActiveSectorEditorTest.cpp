#include "modules/onboard_memory/application/ActiveSectorEditor.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace ghub::onboard_memory {
namespace {

using device_access::AccessRegistry;
using device_access::ExternalOwner;
using device_access::LeaseHolder;
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

// Edição de teste: muda o byte 5 (qualquer byte fora do CRC serve; o editor não conhece o significado).
ActiveSectorEditor::Edit setByteFive(std::uint8_t value)
{
    return [value](std::span<const std::uint8_t> original) -> shared_kernel::Result<Bytes> {
        Bytes edited{original.begin(), original.end()};
        edited.at(5) = value;
        return edited;
    };
}

ActiveSectorEditor::Edit identity()
{
    return [](std::span<const std::uint8_t> original) -> shared_kernel::Result<Bytes> {
        return Bytes{original.begin(), original.end()};
    };
}

// Rejeita cópias com byte 0 == 3 (o "módulo dono" não entende esse perfil).
shared_kernel::Result<void> rejectsRateThree(std::span<const std::uint8_t> bytes)
{
    if (bytes[0] == 3) {
        return shared_kernel::fail(ErrorCode::MalformedData, "intervalo 3");
    }
    return {};
}

class ActiveSectorEditorTest : public ::testing::Test {
protected:
    ActiveSectorEditorTest()
    {
        connector.state.sectors[0] = directory();
        connector.state.sectors[1] = sector0001();
        connector.state.sectors[2] = sector0002();
    }

    ActiveSectorEditor editor()
    {
        return ActiveSectorEditor{
            probe, registry, connector, backup, {.interfaceId = "hidpp:046d:c539", .leaseHolder = "teste.hidpp"}};
    }

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

TEST_F(ActiveSectorEditorTest, WriteRefusesWhenExternalOwnerRunningWithoutOpeningTheNode)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Ratbagd});
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(result.error().detail, "em uso por ratbagd");
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(ActiveSectorEditorTest, WriteRefusesWhenProbeFailsWithoutOpeningTheNode)
{
    probe.setResult(shared_kernel::fail(ErrorCode::BackendUnavailable, "sem /proc"));
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem /proc");
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(ActiveSectorEditorTest, WriteRefusesWhenLeaseIsHeldByAnotherOwner)
{
    const auto other = holdLease();
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(ActiveSectorEditorTest, ReadAndRestoreAlsoRefuseWhenExternalOwnerRunning)
{
    probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Piper});
    EXPECT_EQ(editor().read().error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(editor().restoreBackup(rejectsRateThree).error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(connector.state.opens, 0);
}

TEST_F(ActiveSectorEditorTest, OpensTheNodeOnlyDuringTheOperationAndReleasesTheLease)
{
    ASSERT_TRUE(editor().write(setByteFive(9)).has_value());
    EXPECT_EQ(connector.state.opens, 1);
    EXPECT_EQ(connector.state.liveMemories, 0);
    EXPECT_TRUE(holdLease().verify().has_value());
}

TEST_F(ActiveSectorEditorTest, UsesTheLeaseIdentityGivenByTheOwningModule)
{
    ActiveSectorEditor other{
        probe, registry, connector, backup, {.interfaceId = "hidpp:046d:c547", .leaseHolder = "x"}};
    const auto held = holdLease(); // c539 ocupado não impede c547
    EXPECT_TRUE(other.read().has_value());
}

// --- recusas pelo estado do dispositivo: nenhuma escrita ---

TEST_F(ActiveSectorEditorTest, RefusesWhenSectorSizeIsImplausible)
{
    connector.state.layout.sectorSize = 8;
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(ActiveSectorEditorTest, RefusesWhenDescribeFails)
{
    connector.state.describeError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "sem resposta"};
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem resposta");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(ActiveSectorEditorTest, RefusesWhenNotInOnboardMode)
{
    connector.state.mode = 2;
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "mouse fora do modo onboard");
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(ActiveSectorEditorTest, RefusesActiveSectorOutsideOneToProfileCount)
{
    for (const std::uint16_t current : {std::uint16_t{0}, std::uint16_t{6}, std::uint16_t{0x0101}}) {
        connector.state.currentSector = current;
        const auto result = editor().write(setByteFive(9));
        ASSERT_FALSE(result.has_value()) << current;
        EXPECT_EQ(result.error().code, ErrorCode::MalformedData) << current;
    }
    EXPECT_TRUE(wroteNothing());
}

TEST_F(ActiveSectorEditorTest, RefusesActiveSectorDisabledInTheDirectory)
{
    connector.state.currentSector = 2;
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(ActiveSectorEditorTest, RefusesDirectoryOrSectorWithInvalidCrc)
{
    connector.state.sectors[0][2] = 0x00;
    EXPECT_EQ(editor().write(setByteFive(9)).error().code, ErrorCode::MalformedData);
    connector.state.sectors[0] = directory();
    connector.state.sectors[1][0] = 0x02;
    EXPECT_EQ(editor().write(setByteFive(9)).error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(ActiveSectorEditorTest, RefusesSectorWhoseLengthDiffersFromTheLayout)
{
    connector.state.sectors[1].pop_back();
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

// --- leitura ---

TEST_F(ActiveSectorEditorTest, ReadReturnsTheActiveSectorWithoutWritingAnything)
{
    const auto snapshot = editor().read();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->number, 1U);
    EXPECT_EQ(snapshot->sector.bytes(), sector0001());
    EXPECT_FALSE(snapshot->backupExists);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
    EXPECT_EQ(connector.state.liveMemories, 0);
}

TEST_F(ActiveSectorEditorTest, ReadReportsAnExistingBackupAndFailsWhenTheStoreFails)
{
    backup.stored[1] = sector0001();
    EXPECT_TRUE(editor().read()->backupExists);
    backup.loadError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "disco"};
    const auto snapshot = editor().read();
    ASSERT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error().detail, "disco");
}

TEST_F(ActiveSectorEditorTest, ReadRefusesWithTheSameSafetyChecks)
{
    connector.state.mode = 2;
    EXPECT_EQ(editor().read().error().detail, "mouse fora do modo onboard");
    connector.state.mode = 1;
    connector.state.currentSector = 2;
    EXPECT_EQ(editor().read().error().code, ErrorCode::MalformedData);
}

// --- gravação ---

TEST_F(ActiveSectorEditorTest, WriteFollowsTheExactSequenceAndBacksUpTheOriginalBytesFirst)
{
    backup.log = &connector.state.calls;
    ASSERT_TRUE(editor().write(setByteFive(9)).has_value());
    EXPECT_EQ(connector.state.calls, (std::vector<std::string>{"open", "describe", "mode", "current", "read:0",
                                                               "read:1", "backup:1", "write:1", "read:1"}));
    EXPECT_EQ(backup.stored.at(1), sector0001());
}

TEST_F(ActiveSectorEditorTest, WriteStoresTheEditedSectorWithRecomputedCrcAndReturnsWhatWasReadBack)
{
    const auto result = editor().write(setByteFive(9));
    ASSERT_TRUE(result.has_value());
    Bytes expected = sector0001();
    expected[5] = 9;
    expected = OnboardSector::fromBytes(std::move(expected))->withCrc().bytes();
    EXPECT_EQ(connector.state.sectors.at(1), expected);
    EXPECT_EQ(result->bytes(), expected);
    EXPECT_TRUE(result->hasValidCrc());
    EXPECT_EQ(connector.state.count("write:1"), 1);
    EXPECT_EQ(connector.state.sectors.at(0), directory()); // o diretório nunca é tocado
}

TEST_F(ActiveSectorEditorTest, WriteKeepsAnExistingBackupInsteadOfOverwritingIt)
{
    const Bytes older = sector0002();
    backup.stored[1] = older;
    ASSERT_TRUE(editor().write(setByteFive(9)).has_value());
    EXPECT_EQ(backup.saveCalls, 1);
    EXPECT_EQ(backup.stored.at(1), older);
}

TEST_F(ActiveSectorEditorTest, WriteDoesNotWriteWhenTheBackupFails)
{
    backup.saveError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "disco cheio"};
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "disco cheio");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(ActiveSectorEditorTest, WriteWithAnEditThatChangesNothingSendsNothing)
{
    const auto result = editor().write(identity());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes(), sector0001());
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(ActiveSectorEditorTest, WritePropagatesAnEditRefusalWithoutBackupOrWrite)
{
    const auto result = editor().write([](std::span<const std::uint8_t>) -> shared_kernel::Result<Bytes> {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "recusado pelo módulo dono");
    });
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(ActiveSectorEditorTest, WriteRefusesAnEditThatChangesTheSectorSize)
{
    const auto result = editor().write([](std::span<const std::uint8_t> original) -> shared_kernel::Result<Bytes> {
        Bytes shorter{original.begin(), original.end() - 1};
        shorter[5] = 1;
        return shorter;
    });
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(wroteNothing());
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(ActiveSectorEditorTest, WriteFailureIsReportedWithASingleAttemptAndNoRereadOrRetry)
{
    connector.state.writeError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "fn7 falhou"};
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "fn7 falhou");
    EXPECT_EQ(connector.state.count("write:1"), 1);
    EXPECT_EQ(connector.state.count("read:1"), 1);
    EXPECT_EQ(connector.state.sectors.at(1), sector0001());
}

TEST_F(ActiveSectorEditorTest, WriteReportsADifferingRereadAndDoesNotRewrite)
{
    connector.state.postWriteOverride = sector0002();
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(result.error().detail, "mouse com dados diferentes do enviado — use Restaurar cópia");
    EXPECT_EQ(connector.state.count("write:1"), 1);
}

TEST_F(ActiveSectorEditorTest, WriteDoesNotWriteWhenTheLeaseExpiredAfterTheBackup)
{
    backup.onSave = [this] { clock.advance(std::chrono::seconds{16}); };
    const auto result = editor().write(setByteFive(9));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::LeaseExpired);
    EXPECT_TRUE(wroteNothing());
}

// --- restauração da cópia ---

class RestoreTest : public ActiveSectorEditorTest {
protected:
    RestoreTest()
    {
        Bytes edited = sector0001();
        edited[5] = 9;
        connector.state.sectors[1] = OnboardSector::fromBytes(std::move(edited))->withCrc().bytes();
        backup.stored[1] = sector0001();
    }
};

TEST_F(RestoreTest, RestoresTheBackupWithTheSameWriteAndVerifySequence)
{
    backup.log = &connector.state.calls;
    const auto result = editor().restoreBackup(rejectsRateThree);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes(), sector0001());
    EXPECT_EQ(connector.state.sectors.at(1), sector0001());
    EXPECT_EQ(connector.state.calls, (std::vector<std::string>{"open", "describe", "mode", "current", "read:0",
                                                               "read:1", "write:1", "read:1"}));
    EXPECT_EQ(backup.saveCalls, 0);
}

TEST_F(RestoreTest, RefusesWhenThereIsNoBackup)
{
    backup.stored.clear();
    const auto result = editor().restoreBackup(rejectsRateThree);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "sem cópia de segurança");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(RestoreTest, RefusesBackupWithInvalidCrcOrWrongSize)
{
    backup.stored[1][0] = 0x02;
    EXPECT_EQ(editor().restoreBackup(rejectsRateThree).error().code, ErrorCode::MalformedData);
    backup.stored[1] = sector0001();
    backup.stored[1].pop_back();
    EXPECT_EQ(editor().restoreBackup(rejectsRateThree).error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(wroteNothing());
}

// CRC válido não basta: uma cópia que o módulo dono não entende não é gravada.
TEST_F(RestoreTest, RefusesBackupThatTheOwningModuleRejectsEvenWithValidCrc)
{
    backup.stored[1][0] = 0x03;
    backup.stored[1] = OnboardSector::fromBytes(backup.stored[1])->withCrc().bytes();
    const auto result = editor().restoreBackup(rejectsRateThree);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(result.error().detail, "cópia de segurança ilegível: intervalo 3");
    EXPECT_TRUE(wroteNothing());
}

TEST_F(RestoreTest, DoesNotWriteWhenTheDeviceAlreadyHoldsTheBackup)
{
    connector.state.sectors[1] = sector0001();
    ASSERT_TRUE(editor().restoreBackup(rejectsRateThree).has_value());
    EXPECT_TRUE(wroteNothing());
}

TEST_F(RestoreTest, ReportsADifferingRereadWithoutRewriting)
{
    connector.state.postWriteOverride = sector0002();
    const auto result = editor().restoreBackup(rejectsRateThree);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "mouse com dados diferentes do enviado — use Restaurar cópia");
    EXPECT_EQ(connector.state.count("write:1"), 1);
}

TEST_F(RestoreTest, ReportsBackupStoreFailureWithoutWriting)
{
    backup.loadError = shared_kernel::DomainError{ErrorCode::BackendUnavailable, "disco"};
    const auto result = editor().restoreBackup(rejectsRateThree);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "disco");
    EXPECT_TRUE(wroteNothing());
}

} // namespace
} // namespace ghub::onboard_memory
