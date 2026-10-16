#include "TrayMenuModel.hpp"
#include "AppSettings.hpp"

#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

using ghub::app::AppSettings;
using ghub::app::TrayMenuEntry;
using ghub::app::TrayMenuModel;
using Kind = ghub::app::TrayMenuEntry::Kind;

namespace {

QVariantMap device(const QString& name, int percent)
{
    return {{QStringLiteral("name"), name},
            {QStringLiteral("summary"), QStringLiteral("s")},
            {QStringLiteral("percent"), percent},
            {QStringLiteral("charging"), false}};
}

class TrayMenuModelTest : public QObject {
    Q_OBJECT

private slots:
    void layout_DevicesSeparatorProfilesSeparatorOpenQuit()
    {
        QTemporaryDir dir;
        AppSettings settings{dir.filePath(QStringLiteral("s.json"))};
        settings.load();

        const auto entries =
            TrayMenuModel::build({device(QStringLiteral("G915"), 80), device(QStringLiteral("G703"), -1)}, settings,
                                 {QStringLiteral("Jogos"), QStringLiteral("Trabalho")});

        QCOMPARE(entries.size(), qsizetype{7});
        QCOMPARE(entries[0].kind, Kind::Device);
        QCOMPARE(entries[0].text, QStringLiteral("G915 — 80 %"));
        QVERIFY(!entries[0].enabled);
        QCOMPARE(entries[1].kind, Kind::Separator);
        QCOMPARE(entries[2].kind, Kind::Profile);
        QCOMPARE(entries[2].text, QStringLiteral("Jogos"));
        QCOMPARE(entries[2].profile, QStringLiteral("Jogos"));
        QVERIFY(entries[2].enabled);
        QCOMPARE(entries[3].profile, QStringLiteral("Trabalho"));
        QCOMPARE(entries[4].kind, Kind::Separator);
        QCOMPARE(entries[5].kind, Kind::Open);
        QCOMPARE(entries[5].text, QStringLiteral("Abrir"));
        QCOMPARE(entries[6].kind, Kind::Quit);
        QCOMPARE(entries[6].text, QStringLiteral("Sair"));
    }

    void nicknameReplacesTheCatalogName()
    {
        QTemporaryDir dir;
        AppSettings settings{dir.filePath(QStringLiteral("s.json"))};
        settings.load();
        QVERIFY(settings.setNickname(QStringLiteral("G915"), QStringLiteral("Teclado")));

        const auto entries = TrayMenuModel::build({device(QStringLiteral("G915"), 5)}, settings, {});

        QCOMPARE(entries[0].text, QStringLiteral("Teclado — 5 %"));
    }

    void noDevicesAndNoProfiles_ShowsDisabledPlaceholderAndOnlyOpenQuit()
    {
        QTemporaryDir dir;
        AppSettings settings{dir.filePath(QStringLiteral("s.json"))};
        settings.load();

        const auto entries = TrayMenuModel::build({}, settings, {});

        QCOMPARE(entries.size(), qsizetype{4});
        QCOMPARE(entries[0].kind, Kind::NoProfiles);
        QVERIFY(!entries[0].enabled);
        QCOMPARE(entries[1].kind, Kind::Separator);
        QCOMPARE(entries[2].kind, Kind::Open);
        QCOMPARE(entries[3].kind, Kind::Quit);
    }
};

} // namespace

QTEST_GUILESS_MAIN(TrayMenuModelTest)
#include "TrayMenuModelTest.moc"
