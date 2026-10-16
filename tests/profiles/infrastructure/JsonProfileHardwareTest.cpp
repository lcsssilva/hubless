#include "modules/profiles/infrastructure/JsonProfileRepository.hpp"
#include "modules/profiles/infrastructure/JsonProfileTransfer.hpp"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

// INC-017 parte 2 (R10): formato JSON versão 4 do profiles.json (seções de hardware) e compatibilidade com v1-v3;
// formato do arquivo de exportação/importação (versão 2 só quando há seção de hardware).

using ghub::profiles::HardwareSections;
using ghub::profiles::HeadsetLightSetting;
using ghub::profiles::JsonProfileRepository;
using ghub::profiles::JsonProfileTransfer;
using ghub::profiles::KeyboardEffectSetting;
using ghub::profiles::PointerDpiSetting;
using ghub::profiles::Profile;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;
using ghub::profiles::ProfileSection;
using ghub::shared_kernel::ErrorCode;

namespace {

ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}

HardwareSections fullHardware()
{
    HardwareSections hardware;
    hardware.sidetone = 35;
    hardware.headsetLights = {
        {.zone = 0, .effect = 1, .red = 255, .green = 0, .blue = 10, .periodMs = 0, .brightness = 0},
        {.zone = 1, .effect = 2, .red = 1, .green = 2, .blue = 3, .periodMs = 4000, .brightness = 80}};
    hardware.keyboardEffect = KeyboardEffectSetting{.kind = "breathing", .red = 9, .green = 8, .blue = 7, .brightness = 55};
    hardware.pointerDpi =
        PointerDpiSetting{.reportRateHz = 500, .dpiStages = {400, 1600, 3200}, .defaultStage = 2, .dpiShiftStage = 1};
    return hardware;
}

ProfileLibrary hardwareLibrary()
{
    auto full = *Profile::create(nameOf("Tudo"), {{1, "Brilho", 10}}, std::nullopt, {}, fullHardware());
    HardwareSections only;
    only.sidetone = 0;
    only.pointerDpi = PointerDpiSetting{.reportRateHz = 1000, .dpiStages = {800}, .defaultStage = 0, .dpiShiftStage = std::nullopt};
    auto partial = *Profile::create(nameOf("Parcial"), {}, std::nullopt, {}, only);
    auto library = *ProfileLibrary::create({full, partial});
    library.setPersistent(ProfileSection::Sidetone, true);
    library.setPersistent(ProfileSection::KeyboardEffect, true);
    return library;
}

QByteArray v4(const QByteArray& profiles, const QByteArray& extraRootKeys = {}, const QByteArray& version = "4")
{
    return "{\"format\":\"ghub-linux-profiles\",\"version\":" + version + ",\"automaticSwitching\":false," +
           extraRootKeys + "\"profiles\":" + profiles + "}";
}

QByteArray oneProfile(const QByteArray& body)
{
    return "[{\"name\":\"A\"," + body + "}]";
}

class JsonProfileHardwareTest : public ::testing::Test {
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
    }
};

} // namespace

TEST_F(JsonProfileHardwareTest, RoundTripKeepsEveryHardwareSectionAndPersistentFlag)
{
    JsonProfileRepository repository(path);
    ASSERT_TRUE(repository.save(hardwareLibrary()).has_value());
    const auto loaded = JsonProfileRepository(path).load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    EXPECT_EQ(loaded->profiles(), hardwareLibrary().profiles());
    EXPECT_TRUE(loaded->isPersistent(ProfileSection::Sidetone));
    EXPECT_TRUE(loaded->isPersistent(ProfileSection::KeyboardEffect));
    EXPECT_FALSE(loaded->isPersistent(ProfileSection::PointerDpi));
}

TEST_F(JsonProfileHardwareTest, SavedDocumentIsVersion4WithTheDocumentedKeys)
{
    ASSERT_TRUE(JsonProfileRepository(path).save(hardwareLibrary()).has_value());
    const QByteArray text = readFile();
    EXPECT_TRUE(text.contains("\"version\": 4"));
    for (const char* key : {"\"sidetone\": 35", "\"headsetLights\"", "\"keyboardEffect\"", "\"pointerDpi\"",
                            "\"dpiShiftStage\": 1", "\"persistentSections\""}) {
        EXPECT_TRUE(text.contains(key)) << key;
    }
}

TEST_F(JsonProfileHardwareTest, ProfilesWithoutHardwareWriteNoHardwareKeys)
{
    auto library = *ProfileLibrary::create({*Profile::create(nameOf("A"), {{1, "x", 1}}, std::nullopt)});
    ASSERT_TRUE(JsonProfileRepository(path).save(library).has_value());
    const QByteArray text = readFile();
    for (const char* key : {"sidetone", "headsetLights", "keyboardEffect", "pointerDpi"}) {
        EXPECT_FALSE(text.contains(key)) << key;
    }
}

TEST_F(JsonProfileHardwareTest, OptionalShiftIsOmittedWhenAbsent)
{
    ASSERT_TRUE(JsonProfileRepository(path).save(hardwareLibrary()).has_value());
    const auto loaded = JsonProfileRepository(path).load();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->find(nameOf("Parcial"))->hardware().pointerDpi->dpiShiftStage.has_value());
}

// ---- compatibilidade: arquivos antigos continuam legíveis e não ganham seções ----

TEST_F(JsonProfileHardwareTest, ReadsV1V2AndV3FilesWithoutHardwareSections)
{
    const QByteArray cameraBody = "\"camera\":[{\"id\":1,\"name\":\"Brilho\",\"value\":10}]";
    writeFile("{\"format\":\"ghub-linux-profiles\",\"version\":1,\"profiles\":" + oneProfile(cameraBody) + "}");
    ASSERT_TRUE(JsonProfileRepository(path).load().has_value());
    writeFile("{\"format\":\"ghub-linux-profiles\",\"version\":2,\"automaticSwitching\":true,\"profiles\":" +
              oneProfile(cameraBody + ",\"applications\":[\"firefox\"]") + "}");
    const auto v2 = JsonProfileRepository(path).load();
    ASSERT_TRUE(v2.has_value());
    EXPECT_TRUE(v2->profiles()[0].hardware().empty());
    writeFile(v4(oneProfile(cameraBody), "\"defaultProfile\":\"A\",\"persistentSections\":[\"camera\"],", "3"));
    const auto v3 = JsonProfileRepository(path).load();
    ASSERT_TRUE(v3.has_value()) << v3.error().detail;
    EXPECT_TRUE(v3->profiles()[0].hardware().empty());
    EXPECT_TRUE(v3->isPersistent(ProfileSection::Camera));
}

TEST_F(JsonProfileHardwareTest, OldVersionsRefuseHardwareKeysAndKeysOfLaterVersions)
{
    const QByteArray hardwareBody = "\"sidetone\":40";
    expectMalformed("{\"format\":\"ghub-linux-profiles\",\"version\":1,\"profiles\":" + oneProfile(hardwareBody) + "}");
    expectMalformed(v4(oneProfile(hardwareBody), {}, "3"));
    expectMalformed(v4(oneProfile("\"camera\":[{\"id\":1,\"name\":\"x\",\"value\":1}]"),
                       "\"persistentSections\":[\"sidetone\"],", "3"));
}

TEST_F(JsonProfileHardwareTest, MigratingAnOldFileSavesAsVersion4KeepingTheProfile)
{
    writeFile(v4(oneProfile("\"camera\":[{\"id\":1,\"name\":\"x\",\"value\":1}]"), {}, "3"));
    JsonProfileRepository repository(path);
    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(repository.save(*loaded).has_value());
    EXPECT_TRUE(readFile().contains("\"version\": 4"));
    EXPECT_EQ(JsonProfileRepository(path).load()->profiles(), loaded->profiles());
}

TEST_F(JsonProfileHardwareTest, VersionFiveIsRefusedAndFileStaysIntact)
{
    const QByteArray original = v4("[]", {}, "5");
    writeFile(original);
    JsonProfileRepository repository(path);
    EXPECT_FALSE(repository.load().has_value());
    EXPECT_FALSE(repository.save(ProfileLibrary{}).has_value());
    EXPECT_EQ(readFile(), original);
}

// ---- entrada não confiável ----

TEST_F(JsonProfileHardwareTest, RejectsBadHardwareSections)
{
    const char* bodies[] = {
        "\"sidetone\":\"alto\"",
        "\"sidetone\":1.5",
        "\"sidetone\":3000000000",
        "\"headsetLights\":{}",
        "\"headsetLights\":[1]",
        "\"headsetLights\":[{\"zone\":0,\"effect\":1,\"red\":0,\"green\":0,\"blue\":0,\"periodMs\":0}]",
        "\"headsetLights\":[{\"zone\":0,\"effect\":1,\"red\":0,\"green\":0,\"blue\":0,\"periodMs\":0,\"brightness\":0,"
        "\"extra\":1}]",
        "\"headsetLights\":[{\"zone\":0,\"effect\":1,\"red\":0,\"green\":0,\"blue\":0,\"periodMs\":0,\"brightness\":0},"
        "{\"zone\":0,\"effect\":1,\"red\":0,\"green\":0,\"blue\":0,\"periodMs\":0,\"brightness\":0}]",
        "\"keyboardEffect\":[]",
        "\"keyboardEffect\":{\"kind\":1,\"red\":0,\"green\":0,\"blue\":0,\"brightness\":1}",
        "\"keyboardEffect\":{\"kind\":\"\",\"red\":0,\"green\":0,\"blue\":0,\"brightness\":1}",
        "\"keyboardEffect\":{\"kind\":\"wave\",\"red\":0,\"green\":0,\"blue\":0}",
        "\"pointerDpi\":{\"reportRateHz\":1000,\"dpiStages\":[],\"defaultStage\":0}",
        "\"pointerDpi\":{\"reportRateHz\":1000,\"dpiStages\":[800],\"defaultStage\":3}",
        "\"pointerDpi\":{\"reportRateHz\":1000,\"dpiStages\":[800],\"defaultStage\":0,\"dpiShiftStage\":\"1\"}",
        "\"pointerDpi\":{\"reportRateHz\":1000,\"dpiStages\":[800,\"x\"],\"defaultStage\":0}",
    };
    for (const char* body : bodies) {
        expectMalformed(v4(oneProfile(body)));
    }
}

TEST_F(JsonProfileHardwareTest, RejectsUnknownPersistentSectionAndAcceptsTheNewOnes)
{
    expectMalformed(v4(oneProfile("\"sidetone\":1"), "\"persistentSections\":[\"nope\"],"));
    expectMalformed(v4(oneProfile("\"sidetone\":1"), "\"persistentSections\":[\"sidetone\",\"sidetone\"],"));
    writeFile(v4(oneProfile("\"sidetone\":1"),
                 "\"persistentSections\":[\"sidetone\",\"headsetLights\",\"keyboardEffect\",\"pointerDpi\"],"));
    const auto loaded = JsonProfileRepository(path).load();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    for (const auto section : {ProfileSection::Sidetone, ProfileSection::HeadsetLights, ProfileSection::KeyboardEffect,
                               ProfileSection::PointerDpi}) {
        EXPECT_TRUE(loaded->isPersistent(section));
    }
}

// ---- exportação/importação de um perfil ----

class JsonProfileTransferHardwareTest : public ::testing::Test {
protected:
    QTemporaryDir dir;
    std::string file(const char* name) const { return dir.filePath(name).toStdString(); }

    void write(const char* name, const QByteArray& content)
    {
        QFile out(dir.filePath(name));
        ASSERT_TRUE(out.open(QIODevice::WriteOnly));
        out.write(content);
    }
};

TEST_F(JsonProfileTransferHardwareTest, ExportThenImportKeepsHardwareSections)
{
    JsonProfileTransfer transfer;
    const auto profile = *Profile::create(nameOf("Tudo"), {{1, "Brilho", 10}}, std::nullopt, {}, fullHardware());
    ASSERT_TRUE(transfer.exportProfile(profile, file("p.json")).has_value());
    const auto imported = transfer.importProfile(file("p.json"));
    ASSERT_TRUE(imported.has_value()) << imported.error().detail;
    EXPECT_EQ(imported->profile, profile);
    EXPECT_TRUE(imported->ignoredFields.empty());
}

TEST_F(JsonProfileTransferHardwareTest, ProfileWithoutHardwareStillExportsAsVersion1)
{
    JsonProfileTransfer transfer;
    const auto profile = *Profile::create(nameOf("A"), {{1, "Brilho", 10}}, std::nullopt);
    ASSERT_TRUE(transfer.exportProfile(profile, file("p.json")).has_value());
    QFile in(dir.filePath("p.json"));
    ASSERT_TRUE(in.open(QIODevice::ReadOnly));
    EXPECT_TRUE(in.readAll().contains("\"version\": 1"));
}

TEST_F(JsonProfileTransferHardwareTest, Version1FileWithHardwareKeysListsThemAsIgnored)
{
    write("old.json",
          "{\"format\":\"ghub-linux-profile\",\"version\":1,\"profile\":{\"name\":\"A\",\"camera\":[{\"id\":1,"
          "\"name\":\"x\",\"value\":1}],\"sidetone\":50}}");
    const auto imported = JsonProfileTransfer().importProfile(file("old.json"));
    ASSERT_TRUE(imported.has_value()) << imported.error().detail;
    EXPECT_TRUE(imported->profile.hardware().empty());
    ASSERT_EQ(imported->ignoredFields.size(), 1U);
    EXPECT_EQ(imported->ignoredFields[0], "sidetone");
}

TEST_F(JsonProfileTransferHardwareTest, Version2RejectsInvalidSectionsAndListsUnknownNestedKeys)
{
    write("bad.json",
          "{\"format\":\"ghub-linux-profile\",\"version\":2,\"profile\":{\"name\":\"A\",\"pointerDpi\":{"
          "\"reportRateHz\":1000,\"dpiStages\":[],\"defaultStage\":0}}}");
    EXPECT_EQ(JsonProfileTransfer().importProfile(file("bad.json")).error().code, ErrorCode::MalformedData);
    write("extra.json",
          "{\"format\":\"ghub-linux-profile\",\"version\":2,\"profile\":{\"name\":\"A\",\"keyboardEffect\":{"
          "\"kind\":\"wave\",\"red\":1,\"green\":2,\"blue\":3,\"brightness\":50,\"sparkle\":true}}}");
    const auto imported = JsonProfileTransfer().importProfile(file("extra.json"));
    ASSERT_TRUE(imported.has_value()) << imported.error().detail;
    EXPECT_EQ(imported->profile.hardware().keyboardEffect->kind, "wave");
    ASSERT_EQ(imported->ignoredFields.size(), 1U);
    EXPECT_EQ(imported->ignoredFields[0], "keyboardEffect.sparkle");
}
