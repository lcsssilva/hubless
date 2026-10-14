#include "modules/device_inventory/presentation/DeviceInventoryViewModel.hpp"
#include "fakes/FakeDeviceDiscovery.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include <chrono>
#include <vector>

using namespace std::chrono_literals;
using ghub::device_inventory::DeviceInventoryViewModel;
using ghub::device_inventory::ListDetectedDevices;
using ghub::device_inventory::UsbId;

namespace {

constexpr UsbId g915Receiver{.vendorId = 0x046D, .productId = 0x407C};
constexpr UsbId g703Receiver{.vendorId = 0x046D, .productId = 0x4086};
constexpr UsbId g703Cable{.vendorId = 0x046D, .productId = 0xC090};
constexpr UsbId g733Dongle{.vendorId = 0x046D, .productId = 0x0AFE};
constexpr UsbId brio100{.vendorId = 0x046D, .productId = 0x094C};

QVariantMap item(const DeviceInventoryViewModel& viewModel, qsizetype index)
{
    return viewModel.devices().at(index).toMap();
}

class DeviceInventoryViewModelTest : public QObject {
    Q_OBJECT

private slots:
    void exposesDevices_AfterConstruction()
    {
        discovery.setResult(std::vector{g915Receiver, g703Receiver, g733Dongle, brio100});

        const DeviceInventoryViewModel viewModel{listDetectedDevices, 1h};

        QCOMPARE(viewModel.devices().size(), 4);
        QCOMPARE(item(viewModel, 0).value("name").toString(), QStringLiteral("G915"));
        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("detectado via receptor Lightspeed"));
        QCOMPARE(item(viewModel, 1).value("name").toString(), QStringLiteral("G703"));
        QCOMPARE(item(viewModel, 1).value("summary").toString(), QStringLiteral("detectado via receptor Lightspeed"));
        QCOMPARE(item(viewModel, 2).value("name").toString(), QStringLiteral("G733"));
        QCOMPARE(item(viewModel, 2).value("summary").toString(), QStringLiteral("detectado via dongle USB"));
        QCOMPARE(item(viewModel, 3).value("name").toString(), QStringLiteral("Brio 100"));
        QCOMPARE(item(viewModel, 3).value("summary").toString(), QStringLiteral("detectado via cabo USB"));
    }

    void reportsBrio100NotDetected_WhenAbsent()
    {
        discovery.setResult(std::vector{g915Receiver});

        const DeviceInventoryViewModel viewModel{listDetectedDevices, 1h};

        QCOMPARE(item(viewModel, 3).value("summary").toString(), QStringLiteral("não detectado"));
    }

    void joinsPaths_WhenDetectedTwice()
    {
        discovery.setResult(std::vector{g703Receiver, g703Cable});

        const DeviceInventoryViewModel viewModel{listDetectedDevices, 1h};

        QCOMPARE(item(viewModel, 1).value("summary").toString(),
                 QStringLiteral("detectado via receptor Lightspeed e cabo USB"));
        QCOMPARE(item(viewModel, 0).value("summary").toString(), QStringLiteral("não detectado"));
    }

    void reportsUnavailable_WhenDiscoveryFails()
    {
        discovery.setResult(ghub::shared_kernel::fail(ghub::shared_kernel::ErrorCode::BackendUnavailable, "sysfs"));

        const DeviceInventoryViewModel viewModel{listDetectedDevices, 1h};

        for (qsizetype i = 0; i < 4; ++i) {
            QCOMPARE(item(viewModel, i).value("summary").toString(), QStringLiteral("detecção indisponível"));
        }
    }

    void emitsChanged_OnlyWhenDevicesChange()
    {
        discovery.setResult(std::vector{g915Receiver});
        DeviceInventoryViewModel viewModel{listDetectedDevices, 1h};
        QSignalSpy changed(&viewModel, &DeviceInventoryViewModel::devicesChanged);

        viewModel.refresh();
        QCOMPARE(changed.count(), 0);

        discovery.setResult(std::vector{g915Receiver, g703Receiver});
        viewModel.refresh();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(item(viewModel, 1).value("summary").toString(), QStringLiteral("detectado via receptor Lightspeed"));
    }

    void refreshesPeriodically()
    {
        discovery.setResult(std::vector{g915Receiver});
        DeviceInventoryViewModel viewModel{listDetectedDevices, 10ms};
        QSignalSpy changed(&viewModel, &DeviceInventoryViewModel::devicesChanged);
        discovery.setResult(std::vector{g915Receiver, g703Receiver});

        QVERIFY(changed.wait(5000));
        QCOMPARE(item(viewModel, 1).value("summary").toString(), QStringLiteral("detectado via receptor Lightspeed"));
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    ghub::fakes::FakeDeviceDiscovery discovery;
    ListDetectedDevices listDetectedDevices{{&discovery}};
};

} // namespace

QTEST_GUILESS_MAIN(DeviceInventoryViewModelTest)
#include "DeviceInventoryViewModelTest.moc"
