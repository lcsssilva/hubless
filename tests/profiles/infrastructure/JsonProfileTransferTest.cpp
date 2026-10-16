#include "modules/profiles/infrastructure/JsonProfileTransfer.hpp"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>

#include <sys/stat.h>

using ghub::profiles::ApplicationId;
using ghub::profiles::EqualizerGains;
using ghub::profiles::JsonProfileTransfer;
using ghub::profiles::Profile;
using ghub::profiles::ProfileName;
using ghub::shared_kernel::ErrorCode;

namespace {

ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}

class JsonProfileTransferTest : public ::testing::Test {
protected:
    QTemporaryDir dir;
    JsonProfileTransfer transfer;

    [[nodiscard]] std::string pathOf(const char* file) const { return dir.filePath(file).toStdString(); }

    void write(const char* file, const QByteArray& content) const
    {
        QFile out(dir.filePath(file));
        ASSERT_TRUE(out.open(QIODevice::WriteOnly));
        out.write(content);
    }

    [[nodiscard]] QByteArray read(const char* file) const
    {
        QFile in(dir.filePath(file));
        EXPECT_TRUE(in.open(QIODevice::ReadOnly));
        return in.readAll();
    }
};

const QByteArray kValid =
    "{\"format\":\"ghub-linux-profile\",\"version\":1,\"profile\":{\"name\":\"Jogo\","
    "\"camera\":[{\"id\":1,\"name\":\"Brilho\",\"value\":7}],\"equalizer\":[0,1,2,3,4,5,6,7,8,9]}}";

} // namespace

TEST_F(JsonProfileTransferTest, ExportThenImportRoundTripsWithoutApplications)
{
    const auto profile =
        *Profile::create(nameOf("Câmera ✓"), {{9, "Gain", -3}},
                         *EqualizerGains::create({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}), {*ApplicationId::create("steam")});
    ASSERT_TRUE(transfer.exportProfile(profile, pathOf("out.json")).has_value());
    EXPECT_FALSE(read("out.json").contains("steam"));
    const auto imported = transfer.importProfile(pathOf("out.json"));
    ASSERT_TRUE(imported.has_value()) << imported.error().detail;
    EXPECT_EQ(imported->profile.name(), profile.name());
    EXPECT_EQ(imported->profile.camera(), profile.camera());
    EXPECT_EQ(imported->profile.equalizer(), profile.equalizer());
    EXPECT_TRUE(imported->profile.applications().empty());
    EXPECT_TRUE(imported->ignoredFields.empty());
}

TEST_F(JsonProfileTransferTest, ExportedFileIs0600)
{
    const auto profile = *Profile::create(nameOf("a"), {{1, "x", 1}}, std::nullopt);
    ASSERT_TRUE(transfer.exportProfile(profile, pathOf("out.json")).has_value());
    struct stat info{};
    ASSERT_EQ(::stat(pathOf("out.json").c_str(), &info), 0);
    EXPECT_EQ(info.st_mode & 0777U, 0600U);
}

TEST_F(JsonProfileTransferTest, ExportToAnUnwritablePathIsBackendUnavailable)
{
    const auto profile = *Profile::create(nameOf("a"), {{1, "x", 1}}, std::nullopt);
    EXPECT_EQ(transfer.exportProfile(profile, pathOf("sem/pasta/out.json")).error().code,
              ErrorCode::BackendUnavailable);
}

TEST_F(JsonProfileTransferTest, ImportReadsAValidFile)
{
    write("in.json", kValid);
    const auto imported = transfer.importProfile(pathOf("in.json"));
    ASSERT_TRUE(imported.has_value()) << imported.error().detail;
    EXPECT_EQ(imported->profile.name().value(), "Jogo");
    EXPECT_EQ(imported->profile.camera()[0].value, 7);
}

TEST_F(JsonProfileTransferTest, ImportRefusesFilesLargerThan64KiB)
{
    QByteArray big = kValid;
    big.append(QByteArray(64 * 1024, ' '));
    write("big.json", big);
    const auto imported = transfer.importProfile(pathOf("big.json"));
    ASSERT_FALSE(imported.has_value());
    EXPECT_EQ(imported.error().code, ErrorCode::MalformedData);
    EXPECT_NE(imported.error().detail.find("64 KiB"), std::string::npos);
}

TEST_F(JsonProfileTransferTest, ImportAcceptsAFileOfExactly64KiB)
{
    QByteArray padded = kValid;
    padded.append(QByteArray(64 * 1024 - padded.size(), ' '));
    ASSERT_EQ(padded.size(), 64 * 1024);
    write("edge.json", padded);
    EXPECT_TRUE(transfer.importProfile(pathOf("edge.json")).has_value());
}

TEST_F(JsonProfileTransferTest, ImportRefusesMissingFileAsBackendUnavailable)
{
    EXPECT_EQ(transfer.importProfile(pathOf("nao-existe.json")).error().code, ErrorCode::BackendUnavailable);
}

TEST_F(JsonProfileTransferTest, ImportRefusesInvalidJsonWrongFormatAndUnknownVersion)
{
    write("a.json", "{ nope");
    EXPECT_EQ(transfer.importProfile(pathOf("a.json")).error().code, ErrorCode::MalformedData);
    write("b.json", "[]");
    EXPECT_EQ(transfer.importProfile(pathOf("b.json")).error().code, ErrorCode::MalformedData);
    write("c.json", QByteArray(kValid).replace("ghub-linux-profile\"", "outro\""));
    EXPECT_EQ(transfer.importProfile(pathOf("c.json")).error().code, ErrorCode::MalformedData);
    write("d.json", QByteArray(kValid).replace("\"version\":1", "\"version\":2"));
    EXPECT_EQ(transfer.importProfile(pathOf("d.json")).error().code, ErrorCode::MalformedData);
    write("e.json", QByteArray(kValid).replace("\"version\":1", "\"version\":\"1\""));
    EXPECT_EQ(transfer.importProfile(pathOf("e.json")).error().code, ErrorCode::MalformedData);
    write("f.json", "{\"format\":\"ghub-linux-profile\",\"version\":1}");
    EXPECT_EQ(transfer.importProfile(pathOf("f.json")).error().code, ErrorCode::MalformedData);
}

TEST_F(JsonProfileTransferTest, ImportRefusesInvalidValues)
{
    write("n.json", QByteArray(kValid).replace("\"Jogo\"", "\"\""));
    EXPECT_EQ(transfer.importProfile(pathOf("n.json")).error().code, ErrorCode::MalformedData);
    write("g.json", QByteArray(kValid).replace("[0,1,2,3,4,5,6,7,8,9]", "[0,1]"));
    EXPECT_EQ(transfer.importProfile(pathOf("g.json")).error().code, ErrorCode::MalformedData);
    write("v.json", QByteArray(kValid).replace("\"value\":7", "\"value\":1.5"));
    EXPECT_EQ(transfer.importProfile(pathOf("v.json")).error().code, ErrorCode::MalformedData);
    write("s.json", "{\"format\":\"ghub-linux-profile\",\"version\":1,\"profile\":{\"name\":\"x\"}}");
    EXPECT_EQ(transfer.importProfile(pathOf("s.json")).error().code, ErrorCode::MalformedData); // sem seção
}

TEST_F(JsonProfileTransferTest, ImportIgnoresAndListsUnknownFieldsAndApplications)
{
    write("x.json", "{\"format\":\"ghub-linux-profile\",\"version\":1,\"novo\":true,\"profile\":{\"name\":\"Jogo\","
                    "\"camera\":[{\"id\":1,\"name\":\"B\",\"value\":7,\"extra\":1}],\"sidetone\":50,"
                    "\"applications\":[\"steam\"]}}");
    const auto imported = transfer.importProfile(pathOf("x.json"));
    ASSERT_TRUE(imported.has_value()) << imported.error().detail;
    EXPECT_TRUE(imported->profile.applications().empty());
    const auto& ignored = imported->ignoredFields;
    EXPECT_NE(std::ranges::find(ignored, "novo"), ignored.end());
    EXPECT_NE(std::ranges::find(ignored, "sidetone"), ignored.end());
    EXPECT_NE(std::ranges::find(ignored, "applications"), ignored.end());
    EXPECT_NE(std::ranges::find(ignored, "camera[].extra"), ignored.end());
}
