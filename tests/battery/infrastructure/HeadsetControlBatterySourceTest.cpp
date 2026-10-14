#include "modules/battery/infrastructure/HeadsetControlBatterySource.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "modules/device_access/api/DeviceAccess.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>

using ghub::battery::BatteryLevel;
using ghub::battery::BatteryReading;
using ghub::battery::ChargeState;
using ghub::battery::HeadsetControlBatterySource;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;

namespace {

constexpr auto availableJson =
    R"({"name": "HeadsetControl", "api_version": "1.5", "device_count": 1, "devices": [{"id_vendor": "0x046d", )"
    R"("id_product": "0x0afe", "battery": {"status": "BATTERY_AVAILABLE", "level": 40, "voltage_mv": 3754}}]})";
constexpr auto noDeviceJson = R"({"name": "HeadsetControl", "api_version": "1.5", "device_count": 0, "devices": []})";

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class HeadsetControlBatterySourceTest : public QObject {
    Q_OBJECT

private:
    // Grava um script /bin/sh executável no diretório temporário e devolve o caminho absoluto.
    QString writeScript(const QString& name, const QString& body)
    {
        const auto path = dir_.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qFatal("não foi possível criar o script falso");
        }
        file.write(("#!/bin/sh\n" + body + "\n").toUtf8());
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        file.close();
        return path;
    }

    QString printing(const QString& name, const QString& json, int exitCode = 0)
    {
        return writeScript(name, QStringLiteral("cat <<'EOF'\n%1\nEOF\nexit %2").arg(json).arg(exitCode));
    }

    QTemporaryDir dir_;
    ghub::fakes::FakeExternalProcessProbe probe_;
    ghub::fakes::FakeClock clock_;
    ghub::device_access::AccessRegistry registry_{clock_};
    ghub::device_access::PhysicalInterfaceId interfaceId_ =
        ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:0afe").value();

    // Tenta adquirir o lease da interface como um terceiro holder.
    ghub::shared_kernel::Result<ghub::device_access::DeviceLease> acquireAsOther()
    {
        return registry_.acquire({
            .interfaceId = interfaceId_,
            .holder = ghub::device_access::LeaseHolder::create("teste.outro").value(),
            .purpose = "teste",
            .maxDuration = 10s,
        });
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` é `private:` para o clang.
private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void reportsReading_FromFakeExecutable()
    {
        HeadsetControlBatterySource source{printing("ok.sh", availableJson), probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().device.vendorId, std::uint16_t{0x046d});
        QCOMPARE(all->front().device.productId, std::uint16_t{0x0afe});
        QVERIFY(all->front().reading.has_value());
        QVERIFY(*all->front().reading ==
                (BatteryReading{ChargeState::Discharging, BatteryLevel::fromPercent(40).value()}));
        QCOMPARE(changed.count(), 1);
    }

    void passesExactArguments()
    {
        const auto record = dir_.filePath("args.txt");
        qputenv("GHUB_TEST_LEAK_CHECK", "vazou");
        const auto script =
            writeScript("args.sh", QStringLiteral("{ echo \"argc=$#\"; for a in \"$@\"; do echo \"arg=$a\"; done;"
                                                  " echo \"LC_ALL=$LC_ALL\"; echo \"HOME=${HOME-unset}\";"
                                                  " echo \"LEAK=${GHUB_TEST_LEAK_CHECK-unset}\"; } > '%1'\n"
                                                  "cat <<'EOF'\n%2\nEOF")
                                       .arg(record, availableJson));
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QFile file(record);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto text = QString::fromUtf8(file.readAll());
        // Regressão (teste físico 2026-10-13): sem "0x" o HeadsetControl 4.1.0 lê os IDs em decimal
        // (lib/utility.cpp:202-214 @4.1.0) e "046d:0afe" não seleciona o G733.
        QCOMPARE(text, QStringLiteral("argc=5\narg=-d\narg=0x046d:0x0afe\narg=-b\narg=-o\narg=json\n"
                                      "LC_ALL=C\nHOME=unset\nLEAK=unset\n"));
        qunsetenv("GHUB_TEST_LEAK_CHECK");
    }

    void reportsNotDetected_WhenNoDevice()
    {
        HeadsetControlBatterySource source{
            printing("none.sh", noDeviceJson, 1), probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QVERIFY(all->empty());
    }

    void reportsUnavailable_WhenExecutableMissing()
    {
        for (const auto& executable : {QString{}, dir_.filePath("nao-existe")}) {
            HeadsetControlBatterySource source{executable, probe_, registry_, interfaceId_, 1h, 10s};
            QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

            QVERIFY(changed.wait(10000));

            const auto all = source.readAll();
            QVERIFY(all.has_value());
            QCOMPARE(all->size(), std::size_t{1});
            QVERIFY(!all->front().reading.has_value());
            QCOMPARE(all->front().reading.error().code, ErrorCode::BackendUnavailable);
            QCOMPARE(changed.count(), 1);
        }
    }

    void reportsUnavailable_WhenExitCodeIsUnexpected()
    {
        HeadsetControlBatterySource source{
            printing("exit2.sh", availableJson, 2), probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QVERIFY(!all->front().reading.has_value());
        QCOMPARE(all->front().reading.error().code, ErrorCode::BackendUnavailable);
    }

    void reportsError_WhenExitIsOneButDevicesPresent()
    {
        HeadsetControlBatterySource source{
            printing("exit1.sh", availableJson, 1), probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QVERIFY(!all->front().reading.has_value());
    }

    void reportsMalformed_WhenOutputIsNotJson()
    {
        HeadsetControlBatterySource source{
            printing("garbage.sh", "isto nao e json"), probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().reading.error().code, ErrorCode::MalformedData);
    }

    void killsAndReportsUnavailable_OnTimeout()
    {
        const auto script = writeScript("sleepy.sh", "exec sleep 30");
        QElapsedTimer elapsed;
        elapsed.start();
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 200ms};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QVERIFY(elapsed.elapsed() < 10000);
        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().reading.error().code, ErrorCode::BackendUnavailable);
        QCOMPARE(changed.count(), 1);
    }

    void reportsMalformed_WhenOutputTooLarge()
    {
        const auto script = writeScript("flood.sh", "exec head -c 200000 /dev/zero");
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().reading.error().code, ErrorCode::MalformedData);
        QVERIFY(QString::fromStdString(all->front().reading.error().detail).contains("64 KiB"));
        QCOMPARE(changed.count(), 1);
    }

    void skipsQuery_WhenConflictingProcessRuns()
    {
        const auto marker = dir_.filePath("marker.txt");
        const auto script =
            writeScript("marker.sh", QStringLiteral("touch '%1'\ncat <<'EOF'\n%2\nEOF").arg(marker, availableJson));
        QFile::remove(marker);
        probe_.setResult(std::vector{ghub::device_access::ExternalOwner::Solaar});
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QVERIFY(!QFile::exists(marker));
        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().reading.error().code, ErrorCode::DeviceBusy);
        QVERIFY(QString::fromStdString(all->front().reading.error().detail).contains("solaar"));
        probe_.setResult(std::vector<ghub::device_access::ExternalOwner>{});
    }

    void skipsQuery_WhenProbeFails()
    {
        const auto marker = dir_.filePath("marker2.txt");
        const auto script =
            writeScript("marker2.sh", QStringLiteral("touch '%1'\ncat <<'EOF'\n%2\nEOF").arg(marker, availableJson));
        QFile::remove(marker);
        probe_.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "/proc ilegível"));
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QVERIFY(!QFile::exists(marker));
        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().reading.error().code, ErrorCode::BackendUnavailable);
        probe_.setResult(std::vector<ghub::device_access::ExternalOwner>{});
    }

    void readAll_IsEmptyBeforeFirstQuery()
    {
        const auto callsBefore = probe_.calls();
        HeadsetControlBatterySource source{printing("ok2.sh", availableJson), probe_, registry_, interfaceId_, 1h, 10s};

        const auto all = source.readAll();

        QVERIFY(all.has_value());
        QVERIFY(all->empty());
        QCOMPARE(probe_.calls(), callsBefore);
    }

    void pollsPeriodically()
    {
        HeadsetControlBatterySource source{
            printing("poll.sh", availableJson), probe_, registry_, interfaceId_, 50ms, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 3, 10000);
    }

    void ignoresRequest_WhileQueryRuns()
    {
        const auto counter = dir_.filePath("runs.txt");
        QFile::remove(counter);
        const auto script = writeScript(
            "slow.sh", QStringLiteral("echo run >> '%1'\nsleep 0.5\ncat <<'EOF'\n%2\nEOF").arg(counter, availableJson));
        const auto callsBefore = probe_.calls();
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QTRY_VERIFY_WITH_TIMEOUT(probe_.calls() > callsBefore, 10000);
        source.requestUpdate();
        source.requestUpdate();
        QVERIFY(changed.wait(10000));

        QCOMPARE(changed.count(), 1);
        QFile file(counter);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll().count('\n'), 1);
    }

    void destructor_KillsRunningProcess()
    {
        const auto pidFile = dir_.filePath("pid.txt");
        const auto script = writeScript("pid.sh", QStringLiteral("echo $$ > '%1'\nexec sleep 30").arg(pidFile));
        qint64 pid = 0;
        {
            HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 60s};
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(pidFile) && QFileInfo(pidFile).size() > 0, 10000);
            QFile file(pidFile);
            QVERIFY(file.open(QIODevice::ReadOnly));
            pid = file.readAll().trimmed().toLongLong();
            QVERIFY(pid > 0);
        }
        QVERIFY(!QFile::exists(QStringLiteral("/proc/%1").arg(pid)));
    }

    void reportsDeviceBusy_AndSkipsQuery_WhenLeaseHeldByOther()
    {
        const auto marker = dir_.filePath("marker3.txt");
        const auto script =
            writeScript("marker3.sh", QStringLiteral("touch '%1'\ncat <<'EOF'\n%2\nEOF").arg(marker, availableJson));
        QFile::remove(marker);
        auto other = acquireAsOther();
        QVERIFY(other.has_value());
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QVERIFY(!QFile::exists(marker));
        const auto all = source.readAll();
        QVERIFY(all.has_value());
        QCOMPARE(all->size(), std::size_t{1});
        QCOMPARE(all->front().reading.error().code, ErrorCode::DeviceBusy);
    }

    void releasesLease_AfterSuccessfulQuery()
    {
        HeadsetControlBatterySource source{printing("ok3.sh", availableJson), probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QVERIFY(acquireAsOther().has_value());
    }

    void releasesLease_AfterTimeout()
    {
        const auto script = writeScript("sleepy2.sh", "exec sleep 30");
        HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 200ms};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QVERIFY(acquireAsOther().has_value());
    }

    void releasesLease_AfterFailureToStart()
    {
        HeadsetControlBatterySource source{dir_.filePath("nao-existe2"), probe_, registry_, interfaceId_, 1h, 10s};
        QSignalSpy changed(&source, &HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(10000));

        QVERIFY(acquireAsOther().has_value());
    }

    void holdsLease_WhileQueryRuns_AndReleasesOnDestruction()
    {
        const auto pidFile = dir_.filePath("pid2.txt");
        const auto script = writeScript("pid2.sh", QStringLiteral("echo $$ > '%1'\nexec sleep 30").arg(pidFile));
        {
            HeadsetControlBatterySource source{script, probe_, registry_, interfaceId_, 1h, 60s};
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(pidFile) && QFileInfo(pidFile).size() > 0, 10000);
            const auto during = acquireAsOther();
            QVERIFY(!during.has_value());
            QCOMPARE(during.error().code, ErrorCode::DeviceBusy);
        }
        QVERIFY(acquireAsOther().has_value());
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(HeadsetControlBatterySourceTest)
#include "HeadsetControlBatterySourceTest.moc"
