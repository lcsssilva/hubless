#include "modules/battery/infrastructure/HidppBatterySource.hpp"
#include "fakes/FakeClock.hpp"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <chrono>

using namespace std::chrono_literals;
using ghub::battery::BatteryLevel;
using ghub::battery::BatteryReading;
using ghub::battery::ChargeState;
using ghub::battery::HidppBatterySource;
using ghub::device_access::AccessRegistry;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::shared_kernel::ErrorCode;

namespace {

class HidppBatterySourceTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        clock = std::make_unique<ghub::fakes::FakeClock>();
        registry = std::make_unique<AccessRegistry>(*clock);
        reads = 0;
        outcome = BatteryReading{ChargeState::Discharging, BatteryLevel::fromPercent(80).value()};
    }

    void cleanup()
    {
        registry.reset();
        clock.reset();
    }

    void construction_ReadsNothingBeforeTheEventLoopRuns()
    {
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h};

        QCOMPARE(reads.load(), 0);
        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QVERIFY(all->empty());
    }

    void firstUpdate_PublishesTheG733ReadingAndNotifies()
    {
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h};
        QSignalSpy changed(&source, &HidppBatterySource::readingsChanged);

        QTRY_COMPARE(changed.count(), 1);

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().device.vendorId, std::uint16_t{0x046d});
        QCOMPARE(all->front().device.productId, std::uint16_t{0x0afe});
        QVERIFY(all->front().reading.has_value());
        QCOMPARE(all->front().reading->currentLevel()->percent(), 80);
        QCOMPARE(reads.load(), 1);
    }

    void failure_IsPublishedAsTheEntryError()
    {
        outcome = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "headset desligado ou fora de alcance");
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h};
        QSignalSpy changed(&source, &HidppBatterySource::readingsChanged);

        QTRY_COMPARE(changed.count(), 1);

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QVERIFY(!all->front().reading.has_value());
        QCOMPARE(QString::fromStdString(all->front().reading.error().detail),
                 QStringLiteral("headset desligado ou fora de alcance"));
    }

    void busyLease_IsDeviceBusyWithoutReading()
    {
        const auto held = registry->acquire(LeaseRequest{.interfaceId = interfaceId(),
                                                         .holder = LeaseHolder::create("headset.lighting").value(),
                                                         .purpose = "teste",
                                                         .maxDuration = 60s});
        QVERIFY(held.has_value());
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h};
        QSignalSpy changed(&source, &HidppBatterySource::readingsChanged);

        QTRY_COMPARE(changed.count(), 1);

        QCOMPARE(reads.load(), 0);
        const auto all = source.readAll();
        QVERIFY(!all->front().reading.has_value());
        QCOMPARE(all->front().reading.error().code, ErrorCode::DeviceBusy);
    }

    void leaseIsReleasedAfterEachRead()
    {
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h};
        QSignalSpy changed(&source, &HidppBatterySource::readingsChanged);

        QTRY_COMPARE(changed.count(), 1);

        const auto again = registry->acquire(LeaseRequest{.interfaceId = interfaceId(),
                                                          .holder = LeaseHolder::create("outro").value(),
                                                          .purpose = "teste",
                                                          .maxDuration = 1s});
        QVERIFY(again.has_value());
    }

    void requestUpdate_ReadsAgain()
    {
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h};
        QSignalSpy changed(&source, &HidppBatterySource::readingsChanged);
        QTRY_COMPARE(changed.count(), 1);

        source.requestUpdate();

        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(reads.load(), 2);
    }

    void headsetEvent_ReadsNowAndOnceMoreAfterTheFollowUpDelay()
    {
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h, 150ms};
        QSignalSpy changed(&source, &HidppBatterySource::readingsChanged);
        QTRY_COMPARE(changed.count(), 1);

        source.onHeadsetEvent();

        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(reads.load(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 3, 5000);
        QCOMPARE(reads.load(), 3);
    }

    void headsetEventBurst_YieldsOneImmediateAndOneFollowUpRead()
    {
        HidppBatterySource source{reader(), *registry, interfaceId(), 1h, 150ms};
        QSignalSpy changed(&source, &HidppBatterySource::readingsChanged);
        QTRY_COMPARE(changed.count(), 1);

        source.onHeadsetEvent();
        source.onHeadsetEvent();
        source.onHeadsetEvent();

        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 3, 5000);
        QTest::qWait(400);
        QCOMPARE(reads.load(), 3);
    }

private:
    [[nodiscard]] std::function<ghub::shared_kernel::Result<BatteryReading>()> reader()
    {
        return [this] {
            ++reads;
            return outcome;
        };
    }

    static PhysicalInterfaceId interfaceId() { return PhysicalInterfaceId::create("hidpp:046d:0afe").value(); }

    std::unique_ptr<ghub::fakes::FakeClock> clock;
    std::unique_ptr<AccessRegistry> registry;
    std::atomic<int> reads{0};
    ghub::shared_kernel::Result<BatteryReading> outcome = BatteryReading{ChargeState::Unknown, std::nullopt};
};

} // namespace

QTEST_GUILESS_MAIN(HidppBatterySourceTest)
#include "HidppBatterySourceTest.moc"
