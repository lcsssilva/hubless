#include "AppSettings.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

#include <memory>

using ghub::app::AppSettings;

namespace {

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

void writeAll(const QString& path, const QByteArray& content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

class AppSettingsTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        path = dir->filePath(QStringLiteral("ghub-linux/settings.json"));
    }

    void defaults_AlertOffThresholdFifteenNoNicknames_NothingWritten()
    {
        AppSettings settings{path};
        settings.load();

        QVERIFY(!settings.lowBatteryAlertEnabled());
        QCOMPARE(settings.lowBatteryThreshold(), 15);
        QVERIFY(settings.nicknames().isEmpty());
        QVERIFY(!settings.loadFailed());
        QVERIFY(!QFileInfo::exists(path));
    }

    void defaultFilePath_IsUnderXdgConfigHome()
    {
        qputenv("XDG_CONFIG_HOME", dir->path().toUtf8());
        QCOMPARE(AppSettings::defaultFilePath(), dir->path() + QStringLiteral("/ghub-linux/settings.json"));
    }

    void setters_PersistWith0600AndSurviveAReload()
    {
        AppSettings settings{path};
        settings.load();
        QSignalSpy changed{&settings, &AppSettings::changed};

        QVERIFY(settings.setLowBatteryAlertEnabled(true));
        QVERIFY(settings.setLowBatteryThreshold(30));
        QVERIFY(settings.setNickname(QStringLiteral("G915"), QStringLiteral("  Teclado da mesa ")));
        QCOMPARE(changed.count(), 3);

        struct stat info{};
        QCOMPARE(::stat(QFile::encodeName(path).constData(), &info), 0);
        QCOMPARE(info.st_mode & 0777U, 0600U);

        AppSettings reloaded{path};
        reloaded.load();
        QVERIFY(reloaded.lowBatteryAlertEnabled());
        QCOMPARE(reloaded.lowBatteryThreshold(), 30);
        QCOMPARE(reloaded.displayName(QStringLiteral("G915")), QStringLiteral("Teclado da mesa"));
        QCOMPARE(reloaded.displayName(QStringLiteral("G703")), QStringLiteral("G703"));
    }

    void invalidValues_AreRefusedAndNothingChanges()
    {
        AppSettings settings{path};
        settings.load();

        QVERIFY(!settings.setLowBatteryThreshold(4));
        QVERIFY(!settings.setLowBatteryThreshold(51));
        QCOMPARE(settings.lowBatteryThreshold(), 15);
        QVERIFY(!settings.setNickname(QStringLiteral("G915"), QString()));
        QVERIFY(!settings.setNickname(QStringLiteral("G915"), QString(33, QLatin1Char('x'))));
        QVERIFY(!settings.setNickname(QStringLiteral("G915"), QStringLiteral("a\nb")));
        QVERIFY(!settings.setNickname(QStringLiteral("Mouse desconhecido"), QStringLiteral("x")));
        QVERIFY(settings.nicknames().isEmpty());
        QVERIFY(settings.status().startsWith(QStringLiteral("Falha")));
        QVERIFY(!QFileInfo::exists(path));
    }

    void clearNickname_RestoresTheCatalogName()
    {
        AppSettings settings{path};
        settings.load();
        QVERIFY(settings.setNickname(QStringLiteral("Brio 100"), QStringLiteral("Webcam")));
        QCOMPARE(settings.displayName(QStringLiteral("Brio 100")), QStringLiteral("Webcam"));

        QVERIFY(settings.clearNickname(QStringLiteral("Brio 100")));
        QCOMPARE(settings.displayName(QStringLiteral("Brio 100")), QStringLiteral("Brio 100"));
        AppSettings reloaded{path};
        reloaded.load();
        QVERIFY(reloaded.nicknames().isEmpty());
    }

    void corruptFile_FailsLoadKeepsDefaultsAndIsNeverOverwritten()
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        writeAll(path, "{ nao e json");
        AppSettings settings{path};
        settings.load();

        QVERIFY(settings.loadFailed());
        QVERIFY(!settings.lowBatteryAlertEnabled());
        QVERIFY(!settings.setLowBatteryAlertEnabled(true));
        QCOMPARE(readAll(path), QByteArray("{ nao e json"));
    }

    void invalidContents_FailTheLoad()
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        const char* documents[] = {
            R"({"format":"outro","version":1})",
            R"({"format":"ghub-linux-settings","version":2})",
            R"({"format":"ghub-linux-settings","version":1,"lowBatteryAlert":{"enabled":true,"threshold":99}})",
            R"({"format":"ghub-linux-settings","version":1,"lowBatteryAlert":{"enabled":"sim","threshold":20}})",
            R"({"format":"ghub-linux-settings","version":1,"nicknames":{"G915":""}})",
            R"({"format":"ghub-linux-settings","version":1,"nicknames":{"Mouse X":"a"}})",
            R"({"format":"ghub-linux-settings","version":1,"extra":1})",
        };
        for (const char* document : documents) {
            writeAll(path, document);
            AppSettings settings{path};
            settings.load();
            QVERIFY2(settings.loadFailed(), document);
        }
    }

    void oversizedFile_FailsTheLoad()
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        writeAll(path, QByteArray(AppSettings::maximumFileBytes + 1, ' '));
        AppSettings settings{path};
        settings.load();
        QVERIFY(settings.loadFailed());
    }

    void exportAndRestore_RoundTripTheWholeDocument()
    {
        AppSettings source{path};
        source.load();
        QVERIFY(source.setLowBatteryAlertEnabled(true));
        QVERIFY(source.setNickname(QStringLiteral("G733"), QStringLiteral("Fone")));
        const QByteArray document = source.toDocument();

        AppSettings target{dir->filePath(QStringLiteral("outro/settings.json"))};
        target.load();
        QVERIFY(AppSettings::validateDocument(document).isEmpty());
        QVERIFY(target.restoreFromDocument(document));
        QVERIFY(target.lowBatteryAlertEnabled());
        QCOMPARE(target.displayName(QStringLiteral("G733")), QStringLiteral("Fone"));
        QVERIFY(!AppSettings::validateDocument("{}").isEmpty());
    }

private:
    std::unique_ptr<QTemporaryDir> dir;
    QString path;
};

} // namespace

QTEST_GUILESS_MAIN(AppSettingsTest)
#include "AppSettingsTest.moc"
