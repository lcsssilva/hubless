#include "ConfigBackup.hpp"
#include "AppSettings.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

#include <memory>

using ghub::app::AppSettings;
using ghub::app::ConfigBackup;

namespace {

const QByteArray kProfiles = R"({"format":"ghub-linux-profiles","version":3,"automaticSwitching":false,"profiles":[]})";

unsigned modeOf(const QString& path)
{
    struct stat info{};
    return ::stat(QFile::encodeName(path).constData(), &info) == 0 ? (info.st_mode & 0777U) : 0U;
}

void writeAll(const QString& path, const QByteArray& content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QString validateProfiles(const QByteArray& document)
{
    return document.contains("ghub-linux-profiles") ? QString() : QStringLiteral("perfis inválidos");
}

class ConfigBackupTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        profilesPath = dir->filePath(QStringLiteral("config/profiles.json"));
        backupDir = dir->filePath(QStringLiteral("data/backup"));
        settings = std::make_unique<AppSettings>(dir->filePath(QStringLiteral("config/settings.json")));
        settings->load();
        backup = std::make_unique<ConfigBackup>(backupDir, profilesPath, *settings, validateProfiles);
    }

    void cleanup()
    {
        backup.reset();
        settings.reset();
        dir.reset();
    }

    void construction_WritesNothingAndListsNothing()
    {
        QVERIFY(backup->backups().isEmpty());
        QVERIFY(!QFileInfo::exists(backupDir));
    }

    void defaultDirectory_IsUnderXdgDataHome()
    {
        qputenv("XDG_DATA_HOME", dir->path().toUtf8());
        QCOMPARE(ConfigBackup::defaultDirectory(), dir->path() + QStringLiteral("/ghub-linux/backup"));
    }

    void makeBackup_WritesProfilesAndSettingsWith0600InA0700Directory()
    {
        writeAll(profilesPath, kProfiles);
        QVERIFY(settings->setNickname(QStringLiteral("G915"), QStringLiteral("Teclado")));

        QVERIFY(backup->makeBackup());

        QCOMPARE(backup->backups().size(), 1);
        const QString name = backup->backups().front();
        QVERIFY2(name.startsWith(QStringLiteral("config-")) && name.endsWith(QStringLiteral(".json")),
                 qPrintable(name));
        const QString file = backupDir + QLatin1Char('/') + name;
        QCOMPARE(modeOf(file), 0600U);
        QCOMPARE(modeOf(backupDir), 0700U);
        const QJsonObject root = QJsonDocument::fromJson(readAll(file)).object();
        QCOMPARE(root.value(QStringLiteral("format")).toString(), QStringLiteral("ghub-linux-backup"));
        QCOMPARE(root.value(QStringLiteral("version")).toInt(), 1);
        QVERIFY(root.value(QStringLiteral("profiles")).isObject());
        QCOMPARE(root.value(QStringLiteral("settings"))
                     .toObject()
                     .value(QStringLiteral("nicknames"))
                     .toObject()
                     .value(QStringLiteral("G915"))
                     .toString(),
                 QStringLiteral("Teclado"));
        QVERIFY(backup->status().contains(name));
    }

    void makeBackup_WithoutProfilesFile_StoresNullAndStillWorks()
    {
        QVERIFY(backup->makeBackup());
        const QString file = backupDir + QLatin1Char('/') + backup->backups().front();
        QVERIFY(QJsonDocument::fromJson(readAll(file)).object().value(QStringLiteral("profiles")).isNull());
    }

    void makeBackup_UnreadableProfilesFile_FailsWithoutCreatingAFile()
    {
        writeAll(profilesPath, "{ quebrado");
        QVERIFY(!backup->makeBackup());
        QVERIFY(backup->status().startsWith(QStringLiteral("Falha")));
        QVERIFY(backup->backups().isEmpty());
    }

    void twoBackupsInTheSameSecond_DoNotOverwriteEachOther()
    {
        QVERIFY(backup->makeBackup());
        QVERIFY(backup->makeBackup());
        QCOMPARE(backup->backups().size(), 2);
    }

    void backups_AreListedNewestFirstAndIgnoreForeignFiles()
    {
        writeAll(backupDir + QStringLiteral("/config-20260101-100000.json"), "{}");
        writeAll(backupDir + QStringLiteral("/config-20260301-100000.json"), "{}");
        writeAll(backupDir + QStringLiteral("/outro.txt"), "x");
        backup->refresh();
        QCOMPARE(backup->backups(), (QStringList{QStringLiteral("config-20260301-100000.json"),
                                                 QStringLiteral("config-20260101-100000.json")}));
    }

    void restore_WritesProfilesAndSettingsBackAndSignals()
    {
        writeAll(profilesPath, kProfiles);
        QVERIFY(settings->setLowBatteryAlertEnabled(true));
        QVERIFY(backup->makeBackup());
        const QString name = backup->backups().front();
        writeAll(profilesPath,
                 R"({"format":"ghub-linux-profiles","version":3,"automaticSwitching":true,"profiles":[]})");
        QVERIFY(settings->setLowBatteryAlertEnabled(false));
        QSignalSpy restored{backup.get(), &ConfigBackup::restored};

        QVERIFY(backup->restore(name));

        QCOMPARE(restored.count(), 1);
        QVERIFY(settings->lowBatteryAlertEnabled());
        const QJsonObject profiles = QJsonDocument::fromJson(readAll(profilesPath)).object();
        QCOMPARE(profiles.value(QStringLiteral("automaticSwitching")).toBool(), false);
        QCOMPARE(modeOf(profilesPath), 0600U);
    }

    void restore_RefusesBadNamesAndMissingFilesWithoutTouchingAnything()
    {
        writeAll(profilesPath, kProfiles);
        QVERIFY(settings->setNickname(QStringLiteral("G915"), QStringLiteral("Teclado")));
        QSignalSpy restored{backup.get(), &ConfigBackup::restored};

        const QStringList bad{QStringLiteral("../profiles.json"),
                              QStringLiteral("/etc/passwd"),
                              QStringLiteral("config-x.json/../../a"),
                              QStringLiteral("outro.json"),
                              QStringLiteral("config-20260101-100000.json"),
                              QString()};
        for (const QString& name : bad) {
            QVERIFY2(!backup->restore(name), qPrintable(name));
        }

        QCOMPARE(restored.count(), 0);
        QCOMPARE(readAll(profilesPath), kProfiles);
        QCOMPARE(settings->displayName(QStringLiteral("G915")), QStringLiteral("Teclado"));
    }

    void restore_RefusesInvalidBackupsAndKeepsCurrentFiles()
    {
        writeAll(profilesPath, kProfiles);
        const QString good = backupDir + QStringLiteral("/config-20260101-100000.json");
        const QStringList documents{
            QStringLiteral("{ quebrado"),
            QStringLiteral(R"({"format":"outro","version":1,"profiles":null,"settings":{}})"),
            QStringLiteral(R"({"format":"ghub-linux-backup","version":2,"profiles":null,"settings":{}})"),
            QStringLiteral(R"({"format":"ghub-linux-backup","version":1,"profiles":{"x":1},"settings":{}})"),
            QStringLiteral(R"({"format":"ghub-linux-backup","version":1,"profiles":null,"settings":{"format":"x"}})"),
        };
        for (const QString& document : documents) {
            writeAll(good, document.toUtf8());
            QVERIFY2(!backup->restore(QStringLiteral("config-20260101-100000.json")), qPrintable(document));
            QCOMPARE(readAll(profilesPath), kProfiles);
        }
        QVERIFY(backup->status().startsWith(QStringLiteral("Falha")));
    }

private:
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<ConfigBackup> backup;
    QString profilesPath;
    QString backupDir;
};

} // namespace

QTEST_GUILESS_MAIN(ConfigBackupTest)
#include "ConfigBackupTest.moc"
