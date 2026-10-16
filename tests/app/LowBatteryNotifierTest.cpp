#include "LowBatteryNotifier.hpp"
#include "AppSettings.hpp"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

#include <memory>
#include <vector>

using ghub::app::AppSettings;
using ghub::app::LowBatteryNotifier;

namespace {

QVariantMap device(const QString& name, int percent, bool charging = false)
{
    return {{QStringLiteral("name"), name},
            {QStringLiteral("summary"), QStringLiteral("x")},
            {QStringLiteral("percent"), percent},
            {QStringLiteral("charging"), charging}};
}

class LowBatteryNotifierTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        settings = std::make_unique<AppSettings>(dir->filePath(QStringLiteral("settings.json")));
        settings->load();
        shown.clear();
        logged.clear();
        showResult = true;
        notifier = std::make_unique<LowBatteryNotifier>(
            *settings,
            [this](const QString& title, const QString& message) {
                shown.push_back({title, message});
                return showResult;
            },
            [this](const QString& message) { logged.push_back(message); });
    }

    void cleanup()
    {
        notifier.reset();
        settings.reset();
        dir.reset();
    }

    void disabledByDefault_NeverNotifies()
    {
        notifier->onDevicesChanged({device(QStringLiteral("G915"), 3)});
        QVERIFY(shown.empty());
    }

    void enabled_NotifiesOncePerDischargeWithTheDisplayName()
    {
        QVERIFY(settings->setLowBatteryAlertEnabled(true));
        QVERIFY(settings->setNickname(QStringLiteral("G915"), QStringLiteral("Teclado")));

        notifier->onDevicesChanged({device(QStringLiteral("G915"), 20)});
        QVERIFY(shown.empty());
        notifier->onDevicesChanged({device(QStringLiteral("G915"), 15)});
        notifier->onDevicesChanged({device(QStringLiteral("G915"), 14)});

        QCOMPARE(shown.size(), size_t{1});
        QCOMPARE(shown.front().second, QStringLiteral("Teclado: bateria em 15 %"));
    }

    void chargingRearmsAndEachDeviceHasItsOwnState()
    {
        QVERIFY(settings->setLowBatteryAlertEnabled(true));

        notifier->onDevicesChanged({device(QStringLiteral("G915"), 10), device(QStringLiteral("G703"), 10)});
        QCOMPARE(shown.size(), size_t{2});
        notifier->onDevicesChanged({device(QStringLiteral("G915"), 11, true), device(QStringLiteral("G703"), 9)});
        notifier->onDevicesChanged({device(QStringLiteral("G915"), 10), device(QStringLiteral("G703"), 9)});

        QCOMPARE(shown.size(), size_t{3});
        QVERIFY(shown.back().second.startsWith(QStringLiteral("G915")));
    }

    void thresholdFromSettingsIsUsed()
    {
        QVERIFY(settings->setLowBatteryAlertEnabled(true));
        QVERIFY(settings->setLowBatteryThreshold(30));

        notifier->onDevicesChanged({device(QStringLiteral("G733"), 30)});

        QCOMPARE(shown.size(), size_t{1});
    }

    void unknownLevel_IsIgnored()
    {
        QVERIFY(settings->setLowBatteryAlertEnabled(true));
        notifier->onDevicesChanged({device(QStringLiteral("G915"), -1)});
        QVERIFY(shown.empty());
    }

    void unavailableTray_LogsInsteadOfNotifying()
    {
        QVERIFY(settings->setLowBatteryAlertEnabled(true));
        showResult = false;

        notifier->onDevicesChanged({device(QStringLiteral("G915"), 5)});

        QCOMPARE(logged.size(), size_t{1});
        QVERIFY2(logged.front().contains(QStringLiteral("bandeja")), qPrintable(logged.front()));
    }

    void turningTheOptionOnReevaluatesTheLastReading()
    {
        notifier->onDevicesChanged({device(QStringLiteral("G915"), 8)});
        QVERIFY(shown.empty());

        QVERIFY(settings->setLowBatteryAlertEnabled(true));

        QCOMPARE(shown.size(), size_t{1});
    }

private:
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<LowBatteryNotifier> notifier;
    std::vector<std::pair<QString, QString>> shown;
    std::vector<QString> logged;
    bool showResult = true;
};

} // namespace

QTEST_GUILESS_MAIN(LowBatteryNotifierTest)
#include "LowBatteryNotifierTest.moc"
