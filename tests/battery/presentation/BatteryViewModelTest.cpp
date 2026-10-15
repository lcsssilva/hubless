#include "modules/battery/presentation/BatteryViewModel.hpp"
#include "fakes/FakeBatterySource.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include <chrono>
#include <vector>

using namespace std::chrono_literals;
using ghub::battery::BatteryLevel;
using ghub::battery::BatteryReading;
using ghub::battery::BatteryViewModel;
using ghub::battery::ChargeState;
using ghub::battery::GetBatteryStatus;
using ghub::battery::SourcedBatteryReading;
using ghub::device_inventory::UsbId;

namespace {

constexpr UsbId g915{.vendorId = 0x046D, .productId = 0x407C};
constexpr UsbId g703{.vendorId = 0x046D, .productId = 0x4086};
constexpr UsbId g733{.vendorId = 0x046D, .productId = 0x0AFE};

SourcedBatteryReading reading(UsbId device, ChargeState state, int percent)
{
    return {.device = device, .reading = BatteryReading{state, BatteryLevel::fromPercent(percent).value()}};
}

QVariantMap item(const BatteryViewModel& viewModel, qsizetype index)
{
    return viewModel.devices().at(index).toMap();
}

class BatteryViewModelTest : public QObject {
    Q_OBJECT

private slots:
    void exposesDevices_AfterConstruction()
    {
        source.setResult(std::vector{
            reading(g915, ChargeState::Discharging, 44),
            reading(g703, ChargeState::Charging, 95),
            reading(g733, ChargeState::Discharging, 60),
        });

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(viewModel.devices().size(), 3);
        QCOMPARE(item(viewModel, 0).value("name").toString(), QStringLiteral("G915"));
        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("44% · descarregando"));
        QCOMPARE(item(viewModel, 1).value("name").toString(), QStringLiteral("G703"));
        QCOMPARE(item(viewModel, 1).value("summary").toString(), QStringLiteral("95% · carregando"));
        QCOMPARE(item(viewModel, 2).value("name").toString(), QStringLiteral("G733"));
        QCOMPARE(item(viewModel, 2).value("summary").toString(), QStringLiteral("60% · descarregando"));
    }

    void summarizesNotDetected_WhenDeviceAbsent()
    {
        source.setResult(std::vector{reading(g915, ChargeState::Discharging, 44)});

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(item(viewModel, 1).value("summary").toString(), QStringLiteral("não detectado"));
    }

    void summarizesUnavailable_WhenReadingFailed()
    {
        source.setResult(ghub::shared_kernel::fail(ghub::shared_kernel::ErrorCode::BackendUnavailable, "sysfs"));

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("leitura indisponível"));
        QCOMPARE(item(viewModel, 1).value("summary").toString(), QStringLiteral("leitura indisponível"));
    }

    void hidesPercent_WhenChargeStateUnknown()
    {
        source.setResult(std::vector{reading(g915, ChargeState::Unknown, 44)});

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("estado desconhecido"));
    }

    void namesEveryKnownChargeState()
    {
        source.setResult(
            std::vector{reading(g915, ChargeState::Full, 100), reading(g703, ChargeState::NotCharging, 80)});

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("100% · carga completa"));
        QCOMPARE(item(viewModel, 1).value("summary").toString(), QStringLiteral("80% · sem carregar"));
    }

    void showsStateOnly_WhenLevelUnavailable()
    {
        source.setResult(std::vector<SourcedBatteryReading>{
            {.device = g915, .reading = BatteryReading{ChargeState::Charging, std::nullopt}},
        });

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("carregando"));
    }

    void exposesPercent_WhenLevelKnown()
    {
        source.setResult(std::vector{reading(g915, ChargeState::Discharging, 44)});

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(item(viewModel, 0).value("percent").toInt(), 44);
    }

    void exposesMinusOnePercent_WhenNoLevel()
    {
        source.setResult(std::vector<SourcedBatteryReading>{
            {.device = g915, .reading = BatteryReading{ChargeState::Charging, std::nullopt}},
        });

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QCOMPARE(item(viewModel, 0).value("percent").toInt(), -1);
        QVERIFY(item(viewModel, 0).contains("percent"));
        QCOMPARE(item(viewModel, 1).value("percent").toInt(), -1); // G703 não detectado
    }

    void exposesMinusOnePercent_WhenUnavailable()
    {
        source.setResult(ghub::shared_kernel::fail(ghub::shared_kernel::ErrorCode::BackendUnavailable, "sysfs"));

        const BatteryViewModel viewModel{getBatteryStatus, 1h};

        QVERIFY(item(viewModel, 0).contains("percent"));
        QCOMPARE(item(viewModel, 0).value("percent").toInt(), -1);
    }

    void notifies_WhenRefreshChangesDevices()
    {
        source.setResult(std::vector{reading(g915, ChargeState::Discharging, 44)});
        BatteryViewModel viewModel{getBatteryStatus, 1h};
        QSignalSpy changed(&viewModel, &BatteryViewModel::devicesChanged);
        source.setResult(std::vector{reading(g915, ChargeState::Discharging, 43)});

        viewModel.refresh();

        QCOMPARE(changed.count(), 1);
        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("43% · descarregando"));
    }

    void staysQuiet_WhenRefreshFindsNoChange()
    {
        source.setResult(std::vector{reading(g915, ChargeState::Discharging, 44)});
        BatteryViewModel viewModel{getBatteryStatus, 1h};
        QSignalSpy changed(&viewModel, &BatteryViewModel::devicesChanged);

        viewModel.refresh();

        QCOMPARE(changed.count(), 0);
    }

    void refreshesPeriodically()
    {
        source.setResult(std::vector{reading(g915, ChargeState::Discharging, 44)});
        BatteryViewModel viewModel{getBatteryStatus, 10ms};
        QSignalSpy changed(&viewModel, &BatteryViewModel::devicesChanged);
        source.setResult(std::vector{reading(g915, ChargeState::Charging, 45)});

        QVERIFY(changed.wait(5000));
        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("45% · carregando"));
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    ghub::fakes::FakeBatterySource source;
    GetBatteryStatus getBatteryStatus{{&source}};
};

} // namespace

QTEST_GUILESS_MAIN(BatteryViewModelTest)
#include "BatteryViewModelTest.moc"
