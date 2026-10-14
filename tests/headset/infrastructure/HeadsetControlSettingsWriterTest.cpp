#include "modules/headset/infrastructure/HeadsetControlSettingsWriter.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QFileDevice>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>

using ghub::headset::HeadsetControlSettingsWriter;
using ghub::headset::HeadsetLights;
using ghub::headset::SidetoneLevel;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;

namespace {

// JSON no formato do HeadsetControl 4.1.0 (cli/output/output_data.hpp:89-102 e :289-310).
constexpr auto sidetoneSuccessJson =
    R"({"name": "HeadsetControl", "version": "4.1.0", "api_version": "1.5", "hidapi_version": "0.15.0", )"
    R"("actions": [{"capability": "CAP_SIDETONE", "device": "Logitech G733", "status": "success", "value": 40}], )"
    R"("device_count": 1, "devices": [{"status": "success", "device": "Logitech G733"}]})";
constexpr auto lightsSuccessJson =
    R"({"name": "HeadsetControl", "version": "4.1.0", "api_version": "1.5", "hidapi_version": "0.15.0", )"
    R"("actions": [{"capability": "CAP_LIGHTS", "device": "Logitech G733", "status": "success"}], )"
    R"("device_count": 1, "devices": [{"status": "success", "device": "Logitech G733"}]})";
constexpr auto failureJson =
    R"({"name": "HeadsetControl", "api_version": "1.5", "actions": [{"capability": "CAP_SIDETONE", )"
    R"("device": "Logitech G733", "status": "failure", "error_message": "hid_write falhou"}], )"
    R"("device_count": 1, "devices": []})";
constexpr auto noDeviceJson = R"({"name": "HeadsetControl", "api_version": "1.5", "device_count": 0, "devices": []})";
constexpr auto apiTwoJson = R"({"api_version": "2.0", "actions": [{"capability": "CAP_SIDETONE", )"
                            R"("status": "success"}], "device_count": 1})";
constexpr auto noActionJson = R"({"api_version": "1.5", "device_count": 1, "devices": []})";

SidetoneLevel level(int value)
{
    return SidetoneLevel::create(value).value();
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class HeadsetControlSettingsWriterTest : public QObject {
    Q_OBJECT

private:
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

    QString readFile(const QString& name)
    {
        QFile file(dir_.filePath(name));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString{};
    }

    QTemporaryDir dir_;

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` é `private:` para o clang.
private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void sidetone_PassesExactArguments()
    {
        const auto args = dir_.filePath("sidetone.args");
        const auto script = writeScript(
            "sidetone.sh",
            QStringLiteral("printf '%s\\n' \"$@\" > '%1'\ncat <<'EOF'\n%2\nEOF").arg(args, sidetoneSuccessJson));
        HeadsetControlSettingsWriter writer{script, 10s};

        const auto result = writer.applySidetone(level(40));

        QVERIFY(result.has_value());
        QCOMPARE(readFile("sidetone.args"), QStringLiteral("-d\n0x046d:0x0afe\n-s\n40\n-o\njson\n"));
    }

    void lightsOn_PassesExactArguments()
    {
        const auto args = dir_.filePath("on.args");
        const auto script = writeScript(
            "on.sh", QStringLiteral("printf '%s\\n' \"$@\" > '%1'\ncat <<'EOF'\n%2\nEOF").arg(args, lightsSuccessJson));
        HeadsetControlSettingsWriter writer{script, 10s};

        QVERIFY(writer.applyLights(HeadsetLights::On).has_value());
        QCOMPARE(readFile("on.args"), QStringLiteral("-d\n0x046d:0x0afe\n-l\n1\n-o\njson\n"));
    }

    void lightsOff_PassesExactArgumentsAndAcceptsOutputWithoutValue()
    {
        const auto args = dir_.filePath("off.args");
        const auto script = writeScript(
            "off.sh",
            QStringLiteral("printf '%s\\n' \"$@\" > '%1'\ncat <<'EOF'\n%2\nEOF").arg(args, lightsSuccessJson));
        HeadsetControlSettingsWriter writer{script, 10s};

        QVERIFY(writer.applyLights(HeadsetLights::Off).has_value());
        QCOMPARE(readFile("off.args"), QStringLiteral("-d\n0x046d:0x0afe\n-l\n0\n-o\njson\n"));
    }

    void sidetoneZero_PassesZeroArgument()
    {
        const auto args = dir_.filePath("zero.args");
        const auto script = writeScript(
            "zero.sh",
            QStringLiteral("printf '%s\\n' \"$@\" > '%1'\ncat <<'EOF'\n%2\nEOF").arg(args, sidetoneSuccessJson));
        HeadsetControlSettingsWriter writer{script, 10s};

        QVERIFY(writer.applySidetone(level(0)).has_value());
        QCOMPARE(readFile("zero.args"), QStringLiteral("-d\n0x046d:0x0afe\n-s\n0\n-o\njson\n"));
    }

    void environment_HasOnlyLcAllC()
    {
        const auto envFile = dir_.filePath("env.out");
        const auto script =
            writeScript("env.sh", QStringLiteral("env > '%1'\ncat <<'EOF'\n%2\nEOF").arg(envFile, sidetoneSuccessJson));
        qputenv("GHUB_TEST_LEAK", "1");
        HeadsetControlSettingsWriter writer{script, 10s};

        QVERIFY(writer.applySidetone(level(10)).has_value());

        const auto env = readFile("env.out");
        QVERIFY(env.contains(QStringLiteral("LC_ALL=C\n")));
        QVERIFY(!env.contains(QStringLiteral("GHUB_TEST_LEAK")));
        QVERIFY(!env.contains(QStringLiteral("HOME=")));
        qunsetenv("GHUB_TEST_LEAK");
    }

    void runsTheExecutableExactlyOnce_AlsoOnFailure()
    {
        const auto counter = dir_.filePath("count.out");
        const auto script =
            writeScript("count.sh", QStringLiteral("echo x >> '%1'\ncat <<'EOF'\n%2\nEOF").arg(counter, failureJson));
        HeadsetControlSettingsWriter writer{script, 10s};

        QVERIFY(!writer.applySidetone(level(10)).has_value());
        QCOMPARE(readFile("count.out"), QStringLiteral("x\n"));
    }

    void failureWithExitZero_IsBackendUnavailableWithMessage()
    {
        HeadsetControlSettingsWriter writer{printing("failure.sh", failureJson), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QVERIFY(QString::fromStdString(result.error().detail).contains(QStringLiteral("hid_write falhou")));
    }

    void zeroDevicesWithExitOne_IsHeadsetNotFound()
    {
        HeadsetControlSettingsWriter writer{printing("nodev.sh", noDeviceJson, 1), 10s};

        const auto result = writer.applyLights(HeadsetLights::On);

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QCOMPARE(QString::fromStdString(result.error().detail), QStringLiteral("headset não encontrado"));
    }

    void invalidJson_IsMalformed()
    {
        HeadsetControlSettingsWriter writer{printing("bad.sh", QStringLiteral("não é json")), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::MalformedData);
    }

    void missingAction_IsMalformed()
    {
        HeadsetControlSettingsWriter writer{printing("noaction.sh", noActionJson), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::MalformedData);
    }

    void actionOfAnotherCapability_IsMalformed()
    {
        HeadsetControlSettingsWriter writer{printing("other.sh", lightsSuccessJson), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::MalformedData);
    }

    void apiVersionTwo_IsMalformed()
    {
        HeadsetControlSettingsWriter writer{printing("api2.sh", apiTwoJson), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::MalformedData);
    }

    void timeout_KillsTheProcessAndReports()
    {
        const auto script = writeScript("slow.sh", QStringLiteral("exec sleep 30"));
        HeadsetControlSettingsWriter writer{script, 300ms};
        QElapsedTimer elapsed;
        elapsed.start();

        const auto result = writer.applySidetone(level(10));

        QVERIFY(elapsed.elapsed() < 10000);
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QCOMPARE(QString::fromStdString(result.error().detail), QStringLiteral("headsetcontrol excedeu o tempo"));
    }

    void outputOver64KiB_IsMalformed()
    {
        const auto script = writeScript("big.sh", QStringLiteral("head -c 70000 /dev/zero | tr '\\0' 'a'"));
        HeadsetControlSettingsWriter writer{script, 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::MalformedData);
    }

    void exitCodeThree_IsBackendUnavailable()
    {
        HeadsetControlSettingsWriter writer{printing("three.sh", sidetoneSuccessJson, 3), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QCOMPARE(QString::fromStdString(result.error().detail), QStringLiteral("headsetcontrol saiu com 3"));
    }

    void exitOneWithOtherJson_IsExitCodeError()
    {
        HeadsetControlSettingsWriter writer{printing("one.sh", failureJson, 1), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(QString::fromStdString(result.error().detail), QStringLiteral("headsetcontrol saiu com 1"));
    }

    void killedBySignal_IsBackendUnavailable()
    {
        HeadsetControlSettingsWriter writer{writeScript("crash.sh", QStringLiteral("kill -SEGV $$")), 10s};

        const auto result = writer.applySidetone(level(10));

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
    }

    void nonexistentExecutable_IsBackendUnavailable()
    {
        HeadsetControlSettingsWriter writer{dir_.filePath("nao-existe"), 10s};

        const auto result = writer.applyLights(HeadsetLights::On);

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
    }

    void emptyExecutable_IsNotFound()
    {
        HeadsetControlSettingsWriter writer{QString{}, 10s};

        const auto result = writer.applyLights(HeadsetLights::On);

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QCOMPARE(QString::fromStdString(result.error().detail), QStringLiteral("headsetcontrol não encontrado"));
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(HeadsetControlSettingsWriterTest)
#include "HeadsetControlSettingsWriterTest.moc"
