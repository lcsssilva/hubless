// LogindSleepMonitor contra um login1 falso num dbus-daemon privado (nunca o logind real).
#include "FakeLogindService.hpp"

#include "modules/device_access/infrastructure/LogindSleepMonitor.hpp"

#include <QDBusConnection>
#include <QSignalSpy>
#include <QTest>

#include <memory>

using ghub::fakes::FakeLogindService;
using ghub::fakes::PrivateBus;

class LogindSleepMonitorTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        bus_ = std::make_unique<PrivateBus>();
        QVERIFY(!bus_->address().isEmpty());
    }

    void cleanupTestCase() { bus_.reset(); }

    void cleanup() { QDBusConnection::disconnectFromBus(connectionName_); }

    void constructionCallsNothingAndStartAsksOneSleepDelayInhibitor()
    {
        FakeLogindService fake(bus_->address());
        QVERIFY(fake.started());
        ghub::device_access::LogindSleepMonitor monitor(connect());
        QVERIFY(fake.received().isEmpty());
        QVERIFY(!monitor.holdsInhibitor());

        QVERIFY(monitor.start());
        QCOMPARE(fake.inhibitArguments().size(), 1);
        const QStringList args = fake.inhibitArguments().at(0);
        QCOMPARE(args.at(0), QStringLiteral("sleep"));
        QCOMPARE(args.at(1), QStringLiteral("ghub-linux"));
        QCOMPARE(args.at(3), QStringLiteral("delay"));
        QVERIFY(monitor.holdsInhibitor());
    }

    void prepareForSleepEmitsBeforeReleasingTheInhibitor()
    {
        FakeLogindService fake(bus_->address());
        ghub::device_access::LogindSleepMonitor monitor(connect());
        QVERIFY(monitor.start());
        bool heldInsideSlot = false;
        bool pipeOpenInsideSlot = false;
        connect_(monitor, [&] {
            heldInsideSlot = monitor.holdsInhibitor();
            pipeOpenInsideSlot = !fake.pipeWriterClosed(0);
        });
        QSignalSpy spy(&monitor, &ghub::device_access::LogindSleepMonitor::aboutToSleep);

        fake.emitPrepareForSleep(true);
        QVERIFY(spy.wait(5000));

        QCOMPARE(spy.count(), 1);
        QVERIFY(heldInsideSlot);
        QVERIFY(pipeOpenInsideSlot);
        QVERIFY(!monitor.holdsInhibitor());
        QTRY_VERIFY(fake.pipeWriterClosed(0));
    }

    void resumeAsksTheInhibitorAgainAndEmitsResumed()
    {
        FakeLogindService fake(bus_->address());
        ghub::device_access::LogindSleepMonitor monitor(connect());
        QVERIFY(monitor.start());
        QSignalSpy sleepSpy(&monitor, &ghub::device_access::LogindSleepMonitor::aboutToSleep);
        QSignalSpy resumedSpy(&monitor, &ghub::device_access::LogindSleepMonitor::resumed);
        fake.emitPrepareForSleep(true);
        QVERIFY(sleepSpy.wait(5000));

        fake.emitPrepareForSleep(false);
        QVERIFY(resumedSpy.wait(5000));

        QCOMPARE(resumedSpy.count(), 1);
        QCOMPARE(fake.inhibitArguments().size(), 2);
        QVERIFY(monitor.holdsInhibitor());
    }

    void inhibitErrorMakesStartFailButSleepStillEmitted()
    {
        FakeLogindService fake(bus_->address(), true);
        ghub::device_access::LogindSleepMonitor monitor(connect());

        QVERIFY(!monitor.start());
        QVERIFY(!monitor.lastError().isEmpty());
        QVERIFY(!monitor.holdsInhibitor());

        QSignalSpy spy(&monitor, &ghub::device_access::LogindSleepMonitor::aboutToSleep);
        fake.emitPrepareForSleep(true);
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.count(), 1);
    }

    void missingLogindServiceMakesStartFail()
    {
        ghub::device_access::LogindSleepMonitor monitor(connect());
        QVERIFY(!monitor.start());
        QVERIFY(!monitor.lastError().isEmpty());
    }

private: // NOLINT(readability-redundant-access-specifiers): `private slots` não aceita helpers no moc
    [[nodiscard]] QDBusConnection connect() const
    {
        return QDBusConnection::connectToBus(bus_->address(), connectionName_);
    }

    template <typename F> static void connect_(ghub::device_access::LogindSleepMonitor& monitor, F slot)
    {
        QObject::connect(&monitor, &ghub::device_access::LogindSleepMonitor::aboutToSleep, &monitor, slot,
                         Qt::DirectConnection);
    }

    std::unique_ptr<PrivateBus> bus_;
    QString connectionName_ = QStringLiteral("logind-monitor-client");
};

QTEST_GUILESS_MAIN(LogindSleepMonitorTest)
#include "LogindSleepMonitorTest.moc"
