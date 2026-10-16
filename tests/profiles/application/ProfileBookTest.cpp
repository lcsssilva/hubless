#include "modules/profiles/application/ProfileBook.hpp"
#include "fakes/FakeProfilePorts.hpp"

#include <gtest/gtest.h>

using ghub::profiles::CameraControlSetting;
using ghub::profiles::Profile;
using ghub::profiles::ProfileBook;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;
using ghub::shared_kernel::ErrorCode;
using ghub::test::FakeCameraSettingsPort;
using ghub::test::FakeProfileRepository;

namespace {
const std::array<int, 10> kGains{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}

struct Fixture {
    FakeProfileRepository repository;
    FakeCameraSettingsPort camera;
    ProfileBook book{repository, camera};

    Fixture()
    {
        camera.current = std::vector<CameraControlSetting>{{0x009a0902, "Exposição", 300}, {0x00980900, "Brilho", 128}};
    }
};
} // namespace

TEST(ProfileBook, LoadFillsLibraryFromRepository)
{
    Fixture f;
    f.repository.loadResult = *ProfileLibrary::create({*Profile::create(nameOf("a"), {{1, "x", 1}}, std::nullopt)});
    ASSERT_TRUE(f.book.load().has_value());
    EXPECT_EQ(f.book.library().profiles().size(), 1U);
    EXPECT_FALSE(f.book.loadError().has_value());
}

TEST(ProfileBook, SaveCurrentStoresCameraAndEqualizerSortedById)
{
    Fixture f;
    ASSERT_TRUE(f.book.load().has_value());
    const auto report = f.book.saveCurrent("  Jogo ", kGains, false);
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(report->cameraIncluded);
    EXPECT_FALSE(report->cameraOmittedReason.has_value());
    EXPECT_TRUE(report->equalizerIncluded);
    ASSERT_EQ(f.repository.saveCalls, 1);
    const Profile* saved = f.book.library().find(nameOf("Jogo"));
    ASSERT_NE(saved, nullptr);
    ASSERT_EQ(saved->camera().size(), 2U);
    EXPECT_EQ(saved->camera()[0].id, 0x00980900U);
    EXPECT_EQ(saved->camera()[1].value, 300);
    ASSERT_TRUE(saved->equalizer().has_value());
    EXPECT_EQ(saved->equalizer()->values()[9], 10);
    EXPECT_EQ(f.repository.lastSaved.profiles().size(), 1U);
}

TEST(ProfileBook, MissingCameraOmitsSectionWithReason)
{
    Fixture f;
    f.camera.current = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "Brio 100 ausente");
    const auto report = f.book.saveCurrent("a", kGains, false);
    ASSERT_TRUE(report.has_value());
    EXPECT_FALSE(report->cameraIncluded);
    ASSERT_TRUE(report->cameraOmittedReason.has_value());
    EXPECT_NE(report->cameraOmittedReason->find("Brio 100 ausente"), std::string::npos);
    EXPECT_TRUE(report->equalizerIncluded);
    EXPECT_TRUE(f.book.library().find(nameOf("a"))->camera().empty());
}

TEST(ProfileBook, CameraWithoutControlsOmitsSection)
{
    // Câmera presente mas com todos os controles inativos/somente leitura (o dono já filtrou).
    Fixture f;
    f.camera.current = std::vector<CameraControlSetting>{};
    const auto report = f.book.saveCurrent("a", kGains, false);
    ASSERT_TRUE(report.has_value());
    EXPECT_FALSE(report->cameraIncluded);
    EXPECT_TRUE(report->cameraOmittedReason.has_value());
}

TEST(ProfileBook, WithoutAnySectionFailsAndWritesNothing)
{
    Fixture f;
    f.camera.current = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "ausente");
    const auto report = f.book.saveCurrent("a", std::nullopt, false);
    ASSERT_FALSE(report.has_value());
    EXPECT_EQ(report.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 0);
    EXPECT_TRUE(f.book.library().profiles().empty());
}

TEST(ProfileBook, EqualizerOnlyWhenCameraEmptyAndGainsGiven)
{
    Fixture f;
    f.camera.current = std::vector<CameraControlSetting>{};
    const auto report = f.book.saveCurrent("a", kGains, false);
    ASSERT_TRUE(report.has_value());
    EXPECT_FALSE(report->cameraIncluded);
    EXPECT_TRUE(report->equalizerIncluded);
}

TEST(ProfileBook, MoreThan64CameraControlsIsAnError)
{
    Fixture f;
    std::vector<CameraControlSetting> many;
    for (std::uint32_t i = 0; i < 65; ++i) {
        many.push_back({i, "c", 0});
    }
    f.camera.current = many;
    const auto report = f.book.saveCurrent("a", kGains, false);
    ASSERT_FALSE(report.has_value());
    EXPECT_EQ(report.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBook, InvalidNameFailsBeforeAnyIo)
{
    Fixture f;
    const auto report = f.book.saveCurrent("   ", kGains, false);
    ASSERT_FALSE(report.has_value());
    EXPECT_EQ(report.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.camera.readCalls, 0);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBook, ExistingNameNeedsReplace)
{
    Fixture f;
    ASSERT_TRUE(f.book.saveCurrent("a", kGains, false).has_value());
    const auto again = f.book.saveCurrent("a", kGains, false);
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 1);
    f.camera.current = std::vector<CameraControlSetting>{{1, "Brilho", 7}};
    ASSERT_TRUE(f.book.saveCurrent("a", kGains, true).has_value());
    EXPECT_EQ(f.repository.saveCalls, 2);
    EXPECT_EQ(f.book.library().profiles().size(), 1U);
    EXPECT_EQ(f.book.library().find(nameOf("a"))->camera()[0].value, 7);
}

TEST(ProfileBook, RepositoryFailureKeepsLibraryUnchanged)
{
    Fixture f;
    ASSERT_TRUE(f.book.saveCurrent("a", kGains, false).has_value());
    f.repository.saveResult = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "disco cheio");
    const auto report = f.book.saveCurrent("b", kGains, false);
    ASSERT_FALSE(report.has_value());
    EXPECT_EQ(report.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(f.book.library().profiles().size(), 1U);
    EXPECT_EQ(f.book.library().find(nameOf("b")), nullptr);
    // Substituição que falha também não altera o perfil existente.
    f.camera.current = std::vector<CameraControlSetting>{{1, "Brilho", 7}};
    EXPECT_FALSE(f.book.saveCurrent("a", kGains, true).has_value());
    EXPECT_EQ(f.book.library().find(nameOf("a"))->camera().size(), 2U);
}

TEST(ProfileBook, RemoveDeletesAndPersists)
{
    Fixture f;
    ASSERT_TRUE(f.book.saveCurrent("a", kGains, false).has_value());
    ASSERT_TRUE(f.book.remove(" a ").has_value());
    EXPECT_TRUE(f.book.library().profiles().empty());
    EXPECT_EQ(f.repository.saveCalls, 2);
    EXPECT_TRUE(f.repository.lastSaved.profiles().empty());
}

TEST(ProfileBook, RemoveOfUnknownNameFailsWithoutSaving)
{
    Fixture f;
    const auto result = f.book.remove("nada");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBook, RemoveRepositoryFailureKeepsLibrary)
{
    Fixture f;
    ASSERT_TRUE(f.book.saveCurrent("a", kGains, false).has_value());
    f.repository.saveResult = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "somente leitura");
    EXPECT_FALSE(f.book.remove("a").has_value());
    EXPECT_NE(f.book.library().find(nameOf("a")), nullptr);
}

TEST(ProfileBook, LoadErrorBlocksSaveAndRemoveWithoutCallingRepositorySave)
{
    Fixture f;
    f.repository.loadResult = ghub::shared_kernel::fail(ErrorCode::MalformedData, "version 2");
    const auto loaded = f.book.load();
    ASSERT_FALSE(loaded.has_value());
    ASSERT_TRUE(f.book.loadError().has_value());
    EXPECT_EQ(f.book.loadError()->code, ErrorCode::MalformedData);

    const auto saved = f.book.saveCurrent("a", kGains, false);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::MalformedData);
    EXPECT_FALSE(f.book.remove("a").has_value());
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBook, SuccessfulReloadClearsLoadError)
{
    Fixture f;
    f.repository.loadResult = ghub::shared_kernel::fail(ErrorCode::MalformedData, "ruim");
    ASSERT_FALSE(f.book.load().has_value());
    f.repository.loadResult = ProfileLibrary{};
    ASSERT_TRUE(f.book.load().has_value());
    EXPECT_FALSE(f.book.loadError().has_value());
    EXPECT_TRUE(f.book.saveCurrent("a", kGains, false).has_value());
}

namespace {
using ghub::profiles::ApplicationId;

// Livro com os perfis "a" (firefox) e "b" já carregados do repositório.
struct RulesFixture : Fixture {
    RulesFixture()
    {
        repository.loadResult = *ProfileLibrary::create(
            {*Profile::create(nameOf("a"), {{1, "x", 1}}, std::nullopt, {*ApplicationId::create("firefox")}),
             *Profile::create(nameOf("b"), {{1, "x", 2}}, std::nullopt)});
        EXPECT_TRUE(book.load().has_value());
    }
};
} // namespace

TEST(ProfileBook, SaveCurrentWithReplaceKeepsAssociatedApplications)
{
    RulesFixture f;
    const auto report = f.book.saveCurrent("a", kGains, true);
    ASSERT_TRUE(report.has_value());
    const Profile* replaced = f.book.library().find(nameOf("a"));
    ASSERT_NE(replaced, nullptr);
    ASSERT_EQ(replaced->applications().size(), 1U);
    EXPECT_EQ(replaced->applications()[0].value(), "firefox");
    ASSERT_NE(f.repository.lastSaved.find(nameOf("a")), nullptr);
    EXPECT_EQ(f.repository.lastSaved.find(nameOf("a"))->applications().size(), 1U);
}

TEST(ProfileBook, AssignApplicationPersists)
{
    RulesFixture f;
    ASSERT_TRUE(f.book.assignApplication(" b ", " Konsole ").has_value());
    ASSERT_NE(f.book.library().profileFor(*ApplicationId::create("konsole")), nullptr);
    EXPECT_EQ(f.repository.saveCalls, 1);
    EXPECT_NE(f.repository.lastSaved.profileFor(*ApplicationId::create("konsole")), nullptr);
}

TEST(ProfileBook, AssignApplicationOfAnotherProfileFailsWithoutSaving)
{
    RulesFixture f;
    const auto result = f.book.assignApplication("b", "firefox");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBook, AssignInvalidApplicationOrUnknownProfileFailsWithoutSaving)
{
    RulesFixture f;
    EXPECT_EQ(f.book.assignApplication("b", "com espaço").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.book.assignApplication("zz", "konsole").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBook, UnassignApplicationPersists)
{
    RulesFixture f;
    ASSERT_TRUE(f.book.unassignApplication("FireFox").has_value());
    EXPECT_EQ(f.book.library().profileFor(*ApplicationId::create("firefox")), nullptr);
    EXPECT_EQ(f.repository.saveCalls, 1);
    EXPECT_EQ(f.repository.lastSaved.profileFor(*ApplicationId::create("firefox")), nullptr);
}

TEST(ProfileBook, UnassignUnknownApplicationFailsWithoutSaving)
{
    RulesFixture f;
    EXPECT_EQ(f.book.unassignApplication("konsole").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBook, SetAutomaticSwitchingPersists)
{
    RulesFixture f;
    ASSERT_TRUE(f.book.setAutomaticSwitching(true).has_value());
    EXPECT_TRUE(f.book.library().automaticSwitching());
    EXPECT_EQ(f.repository.saveCalls, 1);
    EXPECT_TRUE(f.repository.lastSaved.automaticSwitching());
    ASSERT_TRUE(f.book.setAutomaticSwitching(false).has_value());
    EXPECT_FALSE(f.repository.lastSaved.automaticSwitching());
}

TEST(ProfileBook, RuleChangesWithRepositoryFailureKeepLibraryUnchanged)
{
    RulesFixture f;
    f.repository.saveResult = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "somente leitura");
    EXPECT_FALSE(f.book.assignApplication("b", "konsole").has_value());
    EXPECT_FALSE(f.book.unassignApplication("firefox").has_value());
    EXPECT_FALSE(f.book.setAutomaticSwitching(true).has_value());
    EXPECT_EQ(f.book.library().profileFor(*ApplicationId::create("konsole")), nullptr);
    EXPECT_NE(f.book.library().profileFor(*ApplicationId::create("firefox")), nullptr);
    EXPECT_FALSE(f.book.library().automaticSwitching());
}

TEST(ProfileBook, LoadErrorBlocksRuleChanges)
{
    Fixture f;
    f.repository.loadResult = ghub::shared_kernel::fail(ErrorCode::MalformedData, "version 3");
    ASSERT_FALSE(f.book.load().has_value());
    EXPECT_EQ(f.book.assignApplication("a", "konsole").error().code, ErrorCode::MalformedData);
    EXPECT_EQ(f.book.unassignApplication("konsole").error().code, ErrorCode::MalformedData);
    EXPECT_EQ(f.book.setAutomaticSwitching(true).error().code, ErrorCode::MalformedData);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

// ---- INC-017: duplicar, exportar, importar, padrão e persistente ----

using ghub::profiles::ImportedProfile;
using ghub::profiles::ProfileSection;
using ghub::test::FakeProfileTransfer;

namespace {
struct TransferFixture {
    FakeProfileRepository repository;
    FakeCameraSettingsPort camera;
    FakeProfileTransfer transfer;
    ProfileBook book{repository, camera, &transfer};

    TransferFixture()
    {
        repository.loadResult = *ProfileLibrary::create({*Profile::create(nameOf("a"), {{1, "x", 1}}, std::nullopt),
                                                         *Profile::create(nameOf("b"), {{1, "x", 2}}, std::nullopt)});
        (void)book.load();
    }
};
} // namespace

TEST(ProfileBookInc017, DuplicateSavesACopyAndRefusesExistingNames)
{
    TransferFixture f;
    ASSERT_TRUE(f.book.duplicate("a", " a copia ").has_value());
    ASSERT_NE(f.book.library().find(nameOf("a copia")), nullptr);
    EXPECT_EQ(f.repository.saveCalls, 1);
    EXPECT_EQ(f.book.duplicate("a", "b").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.book.duplicate("zz", "c").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.book.duplicate("a", "").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(f.repository.saveCalls, 1);
}

TEST(ProfileBookInc017, ExportSendsTheProfileToTheTransferAndSavesNothing)
{
    TransferFixture f;
    ASSERT_TRUE(f.book.exportProfile("b", "/tmp/x.json").has_value());
    ASSERT_EQ(f.transfer.exported.size(), 1U);
    EXPECT_EQ(f.transfer.exported[0].name().value(), "b");
    EXPECT_EQ(f.transfer.exportedPaths[0], "/tmp/x.json");
    EXPECT_EQ(f.repository.saveCalls, 0);
    EXPECT_FALSE(f.book.exportProfile("zz", "/tmp/y.json").has_value());
}

TEST(ProfileBookInc017, ImportAddsTheProfileStripsApplicationsAndReportsIgnoredFields)
{
    TransferFixture f;
    f.transfer.importResult = ImportedProfile{*Profile::create(nameOf("novo"), {{1, "x", 9}}, std::nullopt,
                                                               {*ghub::profiles::ApplicationId::create("steam")}),
                                              {"extra"}};
    const auto report = f.book.importProfile("/tmp/in.json", false);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->name, "novo");
    EXPECT_FALSE(report->replaced);
    ASSERT_EQ(report->ignoredFields.size(), 1U);
    EXPECT_EQ(report->ignoredFields[0], "extra");
    const Profile* imported = f.book.library().find(nameOf("novo"));
    ASSERT_NE(imported, nullptr);
    EXPECT_TRUE(imported->applications().empty());
    EXPECT_EQ(f.repository.saveCalls, 1);
    EXPECT_TRUE(f.camera.applied.empty()); // nada é aplicado ao importar
}

TEST(ProfileBookInc017, ImportOfExistingNameNeedsReplaceAndKeepsItsApplications)
{
    TransferFixture f;
    ASSERT_TRUE(f.book.assignApplication("a", "firefox").has_value());
    f.repository.saveCalls = 0;
    f.transfer.importResult = ImportedProfile{*Profile::create(nameOf("a"), {{1, "x", 77}}, std::nullopt), {}};
    const auto refused = f.book.importProfile("/tmp/in.json", false);
    ASSERT_FALSE(refused.has_value());
    EXPECT_NE(refused.error().detail.find("já existe"), std::string::npos);
    EXPECT_EQ(f.repository.saveCalls, 0);
    EXPECT_EQ(f.book.library().find(nameOf("a"))->camera()[0].value, 1);
    const auto replaced = f.book.importProfile("/tmp/in.json", true);
    ASSERT_TRUE(replaced.has_value());
    EXPECT_TRUE(replaced->replaced);
    EXPECT_EQ(f.book.library().find(nameOf("a"))->camera()[0].value, 77);
    EXPECT_EQ(f.book.library().find(nameOf("a"))->applications().size(), 1U);
}

TEST(ProfileBookInc017, ImportFailureLeavesTheLibraryAlone)
{
    TransferFixture f;
    f.transfer.importResult = ghub::shared_kernel::fail(ErrorCode::MalformedData, "versão desconhecida");
    const auto result = f.book.importProfile("/tmp/in.json", false);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
    EXPECT_EQ(f.book.library().profiles().size(), 2U);
    EXPECT_EQ(f.repository.saveCalls, 0);
}

TEST(ProfileBookInc017, DefaultProfileIsPersistedAndCanBeCleared)
{
    TransferFixture f;
    ASSERT_TRUE(f.book.setDefaultProfile("b").has_value());
    EXPECT_EQ(f.book.library().defaultProfile()->value(), "b");
    EXPECT_EQ(f.repository.lastSaved.defaultProfile()->value(), "b");
    EXPECT_FALSE(f.book.setDefaultProfile("zz").has_value());
    ASSERT_TRUE(f.book.setDefaultProfile(std::optional<std::string_view>{}).has_value());
    EXPECT_FALSE(f.book.library().defaultProfile().has_value());
}

TEST(ProfileBookInc017, PersistentEqualizerIsWrittenToAllProfilesOnSave)
{
    TransferFixture f;
    ASSERT_TRUE(f.book.setPersistent(ProfileSection::Equalizer, true).has_value());
    EXPECT_TRUE(f.repository.lastSaved.isPersistent(ProfileSection::Equalizer));
    ASSERT_TRUE(f.book.saveCurrent("a", kGains, true).has_value());
    EXPECT_EQ(f.book.library().find(nameOf("b"))->equalizer()->values()[9], 10);
    EXPECT_EQ(f.repository.lastSaved.find(nameOf("b"))->equalizer()->values()[9], 10);
}

TEST(ProfileBookInc017, NewOperationsAreRefusedAfterALoadError)
{
    FakeProfileRepository repository;
    FakeCameraSettingsPort camera;
    FakeProfileTransfer transfer;
    ProfileBook book{repository, camera, &transfer};
    repository.loadResult = ghub::shared_kernel::fail(ErrorCode::MalformedData, "ruim");
    (void)book.load();
    EXPECT_FALSE(book.duplicate("a", "b").has_value());
    EXPECT_FALSE(book.setDefaultProfile(std::optional<std::string_view>{}).has_value());
    EXPECT_FALSE(book.setPersistent(ProfileSection::Camera, true).has_value());
    EXPECT_FALSE(book.importProfile("/tmp/x", true).has_value());
    EXPECT_EQ(repository.saveCalls, 0);
}

TEST(ProfileBookInc017, WithoutTransferExportAndImportAreUnavailable)
{
    FakeProfileRepository repository;
    FakeCameraSettingsPort camera;
    ProfileBook book{repository, camera};
    EXPECT_EQ(book.importProfile("/tmp/x", false).error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(book.exportProfile("a", "/tmp/x").error().code, ErrorCode::BackendUnavailable);
}
