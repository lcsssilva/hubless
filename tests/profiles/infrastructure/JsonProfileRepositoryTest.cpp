#include "modules/profiles/infrastructure/JsonProfileRepository.hpp"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <sys/stat.h>
#include <unistd.h>

using ghub::profiles::EqualizerGains;
using ghub::profiles::JsonProfileRepository;
using ghub::profiles::Profile;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;
using ghub::shared_kernel::ErrorCode;

namespace {

unsigned modeOf(const QString& file)
{
    struct stat info{};
    EXPECT_EQ(::stat(QFile::encodeName(file).constData(), &info), 0);
    return info.st_mode & 0777U;
}

ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}

ProfileLibrary sampleLibrary()
{
    auto full = *Profile::create(nameOf("Câmera + EQ ✓"), {{0x009a0902, "Exposição", 300}, {0x00980900, "Brilho", -5}},
                                 *EqualizerGains::create({1, -2, 3, 4, 5, 6, 7, 8, 9, 13}));
    auto cameraOnly = *Profile::create(nameOf("Só câmera"), {{4294967295U, "x", -9007199254740991LL}}, std::nullopt);
    auto eqOnly = *Profile::create(nameOf("Só EQ"), {}, *EqualizerGains::create(std::vector<int>(10, 0)));
    return *ProfileLibrary::create({full, cameraOnly, eqOnly});
}

QByteArray validDocument(const QByteArray& profiles = "[]", const QByteArray& version = "1",
                         const QByteArray& format = "\"ghub-linux-profiles\"")
{
    return "{\"format\":" + format + ",\"version\":" + version + ",\"profiles\":" + profiles + "}";
}

QByteArray validV2(const QByteArray& profiles = "[]", const QByteArray& automaticSwitching = "false",
                   const QByteArray& version = "2")
{
    return "{\"format\":\"ghub-linux-profiles\",\"version\":" + version +
           ",\"automaticSwitching\":" + automaticSwitching + ",\"profiles\":" + profiles + "}";
}

// Arquivo real da versão 1, gravado no teste físico da parte 1 (INC-012).
const QByteArray kRealV1 =
    "{\"format\":\"ghub-linux-profiles\",\"profiles\":[{\"camera\":[{\"id\":9963776,\"name\":\"Brightness\",\"value\":"
    "60},"
    "{\"id\":9963777,\"name\":\"Contrast\",\"value\":128},{\"id\":9963778,\"name\":\"Saturation\",\"value\":128},"
    "{\"id\":9963788,\"name\":\"White Balance, "
    "Automatic\",\"value\":1},{\"id\":9963795,\"name\":\"Gain\",\"value\":175},"
    "{\"id\":9963800,\"name\":\"Power Line "
    "Frequency\",\"value\":2},{\"id\":9963803,\"name\":\"Sharpness\",\"value\":128},"
    "{\"id\":9963804,\"name\":\"Backlight Compensation\",\"value\":1},{\"id\":10094849,\"name\":\"Auto "
    "Exposure\",\"value\":3},"
    "{\"id\":10094851,\"name\":\"Exposure, Dynamic Framerate\",\"value\":1}],"
    "\"equalizer\":[0,12,0,0,0,0,0,0,0,0],\"name\":\"A\"}],\"version\":1}";

class JsonProfileRepositoryTest : public ::testing::Test {
protected:
    QTemporaryDir dir;
    QString path = dir.filePath("ghub-linux/profiles.json");

    void writeFile(const QByteArray& content)
    {
        ASSERT_TRUE(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

    QByteArray readFile() const
    {
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }

    void expectMalformed(const QByteArray& content)
    {
        writeFile(content);
        JsonProfileRepository repository(path);
        const auto loaded = repository.load();
        ASSERT_FALSE(loaded.has_value()) << content.toStdString();
        EXPECT_EQ(loaded.error().code, ErrorCode::MalformedData);
        EXPECT_FALSE(loaded.error().detail.empty());
    }
};

} // namespace

TEST_F(JsonProfileRepositoryTest, DefaultPathIsUnderXdgConfigHome)
{
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    EXPECT_EQ(JsonProfileRepository::defaultFilePath(), dir.path() + "/ghub-linux/profiles.json");
}

TEST_F(JsonProfileRepositoryTest, MissingFileGivesEmptyLibrary)
{
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->profiles().empty());
}

TEST_F(JsonProfileRepositoryTest, RoundTripKeepsEverythingAndOrder)
{
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(sampleLibrary()).has_value());
    JsonProfileRepository reader(path);
    const auto loaded = reader.load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    EXPECT_EQ(loaded->profiles(), sampleLibrary().profiles());
}

TEST_F(JsonProfileRepositoryTest, SavedDocumentHasDocumentedShape)
{
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(sampleLibrary()).has_value());
    const QByteArray text = readFile();
    EXPECT_TRUE(text.contains("\"format\": \"ghub-linux-profiles\""));
    EXPECT_TRUE(text.contains("\"version\": 3"));
    EXPECT_TRUE(text.contains("\"automaticSwitching\": false"));
    EXPECT_TRUE(text.contains("\"equalizer\""));
}

TEST_F(JsonProfileRepositoryTest, FileIs0600AndCreatedDirectoryIs0700)
{
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(sampleLibrary()).has_value());
    EXPECT_EQ(modeOf(path), 0600U);
    EXPECT_EQ(modeOf(QFileInfo(path).absolutePath()), 0700U);
}

TEST_F(JsonProfileRepositoryTest, SaveReplacesPreviousContent)
{
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(sampleLibrary()).has_value());
    ASSERT_TRUE(repository.save(ProfileLibrary{}).has_value());
    JsonProfileRepository reader(path);
    EXPECT_TRUE(reader.load()->profiles().empty());
}

TEST_F(JsonProfileRepositoryTest, RejectsFileLargerThanOneMiB)
{
    QByteArray big = validDocument();
    big.append(QByteArray(1024 * 1024 + 1 - big.size(), ' '));
    ASSERT_EQ(big.size(), 1024 * 1024 + 1);
    expectMalformed(big);
}

TEST_F(JsonProfileRepositoryTest, AcceptsFileOfExactlyOneMiB)
{
    QByteArray exact = validDocument();
    exact.append(QByteArray(1024 * 1024 - exact.size(), ' '));
    writeFile(exact);
    JsonProfileRepository repository(path);
    EXPECT_TRUE(repository.load().has_value());
}

TEST_F(JsonProfileRepositoryTest, RejectsTruncatedJson)
{
    expectMalformed(validDocument().chopped(5));
}

TEST_F(JsonProfileRepositoryTest, RejectsEmptyFile)
{
    expectMalformed("");
}

TEST_F(JsonProfileRepositoryTest, RejectsRootThatIsNotObject)
{
    expectMalformed("[]");
}

TEST_F(JsonProfileRepositoryTest, RejectsFutureAndZeroVersions)
{
    expectMalformed(validV2("[]", "false", "4"));
    expectMalformed(validV2("[]", "false", "5"));
    expectMalformed(validDocument("[]", "0"));
}

TEST_F(JsonProfileRepositoryTest, RejectsVersionAsString)
{
    expectMalformed(validDocument("[]", "\"1\""));
}

TEST_F(JsonProfileRepositoryTest, RejectsOtherFormat)
{
    expectMalformed(validDocument("[]", "1", "\"outro\""));
}

TEST_F(JsonProfileRepositoryTest, RejectsMissingProfilesKey)
{
    expectMalformed("{\"format\":\"ghub-linux-profiles\",\"version\":1}");
}

TEST_F(JsonProfileRepositoryTest, RejectsWrongTypes)
{
    expectMalformed(validDocument("[{\"name\":5,\"camera\":[{\"id\":1,\"name\":\"a\",\"value\":1}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":{}}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":[{\"id\":1,\"name\":\"a\",\"value\":\"1\"}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":[{\"id\":\"1\",\"name\":\"a\",\"value\":1}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":[{\"id\":1,\"name\":7,\"value\":1}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,\"0\"]}]"));
    expectMalformed(validDocument("[\"a\"]"));
}

TEST_F(JsonProfileRepositoryTest, RejectsNonIntegerOrOutOfRangeNumbers)
{
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":[{\"id\":1,\"name\":\"a\",\"value\":1.5}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":[{\"id\":-1,\"name\":\"a\",\"value\":1}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":[{\"id\":4294967296,\"name\":\"a\",\"value\":1}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"camera\":[{\"id\":1,\"name\":\"a\",\"value\":1e300}]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,1.5]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,3000000000]}]"));
}

TEST_F(JsonProfileRepositoryTest, RejectsDomainViolations)
{
    // sem seção
    expectMalformed(validDocument("[{\"name\":\"a\"}]"));
    // nome vazio e com controle
    expectMalformed(validDocument("[{\"name\":\"  \",\"equalizer\":[0,0,0,0,0,0,0,0,0,0]}]"));
    expectMalformed(validDocument("[{\"name\":\"a\\nb\",\"equalizer\":[0,0,0,0,0,0,0,0,0,0]}]"));
    // 9 ganhos
    expectMalformed(validDocument("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0]}]"));
    // id repetido
    expectMalformed(validDocument(
        "[{\"name\":\"a\",\"camera\":[{\"id\":1,\"name\":\"a\",\"value\":1},{\"id\":1,\"name\":\"b\",\"value\":2}]}]"));
}

TEST_F(JsonProfileRepositoryTest, RejectsDuplicateNameInFile)
{
    expectMalformed(validDocument("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,0]},"
                                  "{\"name\":\"a\",\"equalizer\":[1,0,0,0,0,0,0,0,0,0]}]"));
}

TEST_F(JsonProfileRepositoryTest, RejectsMoreThan50Profiles)
{
    QByteArray profiles = "[";
    for (int i = 0; i < 51; ++i) {
        profiles += (i ? "," : "") + QByteArray("{\"name\":\"p") + QByteArray::number(i) +
                    "\",\"equalizer\":[0,0,0,0,0,0,0,0,0,0]}";
    }
    expectMalformed(validDocument(profiles + "]"));
}

TEST_F(JsonProfileRepositoryTest, RejectsUnknownKeysInsteadOfDroppingThemSilently)
{
    expectMalformed(validDocument("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,0],\"sidetone\":3}]"));
    expectMalformed("{\"format\":\"ghub-linux-profiles\",\"version\":1,\"profiles\":[],\"extra\":1}");
}

TEST_F(JsonProfileRepositoryTest, SaveRefusedAfterFailedLoadLeavesFileByteForByteEqual)
{
    const QByteArray original = validV2("[]", "false", "4"); // versão futura que o app não entende
    writeFile(original);
    JsonProfileRepository repository(path);
    ASSERT_FALSE(repository.load().has_value());
    const auto saved = repository.save(sampleLibrary());
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(readFile(), original);
}

TEST_F(JsonProfileRepositoryTest, SaveWorksAgainAfterSuccessfulReload)
{
    writeFile(validV2("[]", "false", "4"));
    JsonProfileRepository repository(path);
    ASSERT_FALSE(repository.load().has_value());
    writeFile(validDocument());
    ASSERT_TRUE(repository.load().has_value());
    EXPECT_TRUE(repository.save(sampleLibrary()).has_value());
}

TEST_F(JsonProfileRepositoryTest, UnreadableExistingFileIsReportedAndBlocksSave)
{
    if (::geteuid() == 0) {
        GTEST_SKIP() << "root ignora permissões de arquivo";
    }
    const QByteArray original = validDocument();
    writeFile(original);
    ASSERT_TRUE(QFile::setPermissions(path, QFileDevice::Permissions{}));
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().code, ErrorCode::BackendUnavailable);
    EXPECT_FALSE(repository.save(sampleLibrary()).has_value());
    ASSERT_TRUE(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    EXPECT_EQ(readFile(), original);
}

TEST_F(JsonProfileRepositoryTest, WriteFailureIsBackendUnavailable)
{
    if (::geteuid() == 0) {
        GTEST_SKIP() << "root ignora permissões de diretório";
    }
    ASSERT_TRUE(QDir().mkpath(QFileInfo(path).absolutePath()));
    ASSERT_TRUE(QFile::setPermissions(QFileInfo(path).absolutePath(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    JsonProfileRepository repository(path);
    const auto saved = repository.save(sampleLibrary());
    QFile::setPermissions(QFileInfo(path).absolutePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::BackendUnavailable);
}

TEST_F(JsonProfileRepositoryTest, ValueBeyondExactDoubleRangeCannotBeSaved)
{
    auto huge = *Profile::create(nameOf("g"), {{1, "x", 9007199254740993LL}}, std::nullopt);
    JsonProfileRepository repository(path);
    const auto saved = repository.save(*ProfileLibrary::create({huge}));
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::InvalidArgument);
    EXPECT_FALSE(QFileInfo::exists(path));
}

TEST_F(JsonProfileRepositoryTest, RoundTripKeepsApplicationsAndAutomaticSwitching)
{
    auto library =
        *ProfileLibrary::create({*Profile::create(nameOf("a"), {{1, "x", 1}}, std::nullopt,
                                                  {*ghub::profiles::ApplicationId::create("konsole"),
                                                   *ghub::profiles::ApplicationId::create("org.mozilla.firefox")}),
                                 *Profile::create(nameOf("b"), {}, *EqualizerGains::create(std::vector<int>(10, 0)))});
    library.setAutomaticSwitching(true);
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(library).has_value());
    EXPECT_TRUE(readFile().contains("\"applications\""));
    JsonProfileRepository reader(path);
    const auto loaded = reader.load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    EXPECT_EQ(loaded->profiles(), library.profiles());
    EXPECT_TRUE(loaded->automaticSwitching());
}

TEST_F(JsonProfileRepositoryTest, ReadsV2WithoutApplicationsKeyAsEmpty)
{
    writeFile(validV2("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,0]}]", "true"));
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    EXPECT_TRUE(loaded->profiles()[0].applications().empty());
    EXPECT_TRUE(loaded->automaticSwitching());
}

TEST_F(JsonProfileRepositoryTest, V2RequiresAutomaticSwitchingAsBoolean)
{
    expectMalformed("{\"format\":\"ghub-linux-profiles\",\"version\":2,\"profiles\":[]}");
    expectMalformed(validV2("[]", "1"));
    expectMalformed(validV2("[]", "\"true\""));
}

TEST_F(JsonProfileRepositoryTest, V1RefusesAutomaticSwitchingAndApplicationsKeys)
{
    expectMalformed("{\"format\":\"ghub-linux-profiles\",\"version\":1,\"automaticSwitching\":false,\"profiles\":[]}");
    expectMalformed(validDocument("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,0],\"applications\":[]}]"));
}

TEST_F(JsonProfileRepositoryTest, RejectsInvalidApplicationsInV2)
{
    const QByteArray eq = "\"equalizer\":[0,0,0,0,0,0,0,0,0,0]";
    expectMalformed(validV2("[{\"name\":\"a\"," + eq + ",\"applications\":[\"com espaço\"]}]"));
    expectMalformed(validV2("[{\"name\":\"a\"," + eq + ",\"applications\":[\"\"]}]"));
    expectMalformed(validV2("[{\"name\":\"a\"," + eq + ",\"applications\":[5]}]"));
    expectMalformed(validV2("[{\"name\":\"a\"," + eq + ",\"applications\":\"x\"}]"));
    expectMalformed(validV2("[{\"name\":\"a\"," + eq + ",\"applications\":[\"x\",\"x\"]}]"));
    expectMalformed(validV2("[{\"name\":\"a\"," + eq + ",\"applications\":[\"x\"]},{\"name\":\"b\"," + eq +
                            ",\"applications\":[\"x\"]}]"));
    expectMalformed(validV2("[{\"name\":\"a\"," + eq + ",\"extra\":1}]"));
    expectMalformed(validV2("[]", "false").replace("\"profiles\"", "\"extra\":1,\"profiles\""));
}

TEST_F(JsonProfileRepositoryTest, Rejects33ApplicationsInAProfile)
{
    QByteArray applications = "[";
    for (int i = 0; i < 33; ++i) {
        applications += (i ? "," : "") + QByteArray("\"app") + QByteArray::number(i) + "\"";
    }
    expectMalformed(
        validV2("[{\"name\":\"a\",\"equalizer\":[0,0,0,0,0,0,0,0,0,0],\"applications\":" + applications + "]}]"));
}

TEST_F(JsonProfileRepositoryTest, RealV1FileMigratesWithoutRulesAndIsNotRewrittenOnLoad)
{
    writeFile(kRealV1);
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    ASSERT_EQ(loaded->profiles().size(), 1U);
    EXPECT_EQ(loaded->profiles()[0].name().value(), "A");
    EXPECT_EQ(loaded->profiles()[0].camera().size(), 10U);
    EXPECT_EQ(loaded->profiles()[0].equalizer()->values()[1], 12);
    EXPECT_TRUE(loaded->profiles()[0].applications().empty());
    EXPECT_FALSE(loaded->automaticSwitching());
    EXPECT_EQ(readFile(), kRealV1);
}

TEST_F(JsonProfileRepositoryTest, SavingAfterV1MigrationWritesV3KeepingTheProfile)
{
    writeFile(kRealV1);
    JsonProfileRepository repository(path);
    auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(repository.save(*loaded).has_value());
    EXPECT_TRUE(readFile().contains("\"version\": 3"));
    JsonProfileRepository reader(path);
    const auto again = reader.load();
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(again->profiles(), loaded->profiles());
}

TEST_F(JsonProfileRepositoryTest, VersionFourIsRefusedAndFileStaysByteForByteIntact)
{
    const QByteArray original = validV2("[]", "false", "4");
    writeFile(original);
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().code, ErrorCode::MalformedData);
    EXPECT_FALSE(repository.save(ProfileLibrary{}).has_value());
    EXPECT_EQ(readFile(), original);
}

// ---- INC-017: versão 3 (perfil padrão e seções persistentes) ----

namespace {
QByteArray validV3(const QByteArray& profiles, const QByteArray& extraRootKeys = {})
{
    return "{\"format\":\"ghub-linux-profiles\",\"version\":3,\"automaticSwitching\":false," + extraRootKeys +
           "\"profiles\":" + profiles + "}";
}
const QByteArray kOneProfile = "[{\"name\":\"A\",\"camera\":[{\"id\":1,\"name\":\"x\",\"value\":1}]}]";
} // namespace

TEST_F(JsonProfileRepositoryTest, RoundTripKeepsDefaultProfileAndPersistentSections)
{
    auto library = sampleLibrary();
    ASSERT_TRUE(library.setDefaultProfile(nameOf("Só EQ")).has_value());
    library.setPersistent(ghub::profiles::ProfileSection::Equalizer, true);
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(library).has_value());
    const QByteArray text = readFile();
    EXPECT_TRUE(text.contains("\"defaultProfile\": \"Só EQ\""));
    EXPECT_TRUE(text.contains("\"persistentSections\""));
    JsonProfileRepository reader(path);
    const auto loaded = reader.load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    EXPECT_EQ(loaded->defaultProfile()->value(), "Só EQ");
    EXPECT_TRUE(loaded->isPersistent(ghub::profiles::ProfileSection::Equalizer));
    EXPECT_FALSE(loaded->isPersistent(ghub::profiles::ProfileSection::Camera));
}

TEST_F(JsonProfileRepositoryTest, DefaultKeysAreOmittedWhenUnused)
{
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(sampleLibrary()).has_value());
    const QByteArray text = readFile();
    EXPECT_FALSE(text.contains("defaultProfile"));
    EXPECT_FALSE(text.contains("persistentSections"));
}

TEST_F(JsonProfileRepositoryTest, ReadsV3WithoutOptionalKeys)
{
    writeFile(validV3(kOneProfile));
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    EXPECT_FALSE(loaded->defaultProfile().has_value());
}

TEST_F(JsonProfileRepositoryTest, RejectsBadDefaultProfileAndPersistentSections)
{
    expectMalformed(validV3(kOneProfile, "\"defaultProfile\":\"B\","));                       // inexistente
    expectMalformed(validV3(kOneProfile, "\"defaultProfile\":7,"));                           // tipo errado
    expectMalformed(validV3(kOneProfile, "\"persistentSections\":[\"sidetone\"],"));          // seção desconhecida
    expectMalformed(validV3(kOneProfile, "\"persistentSections\":\"camera\","));              // tipo errado
    expectMalformed(validV3(kOneProfile, "\"persistentSections\":[\"camera\",\"camera\"],")); // repetida
}

TEST_F(JsonProfileRepositoryTest, V2StillRefusesV3OnlyKeys)
{
    expectMalformed("{\"format\":\"ghub-linux-profiles\",\"version\":2,\"automaticSwitching\":false,"
                    "\"defaultProfile\":\"A\",\"profiles\":" +
                    kOneProfile + "}");
}

TEST_F(JsonProfileRepositoryTest, V2FileIsMigratedWithoutDefaultAndSavedAsV3)
{
    writeFile(validV2(kOneProfile));
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    ASSERT_TRUE(repository.save(*loaded).has_value());
    EXPECT_TRUE(readFile().contains("\"version\": 3"));
}
