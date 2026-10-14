#include "modules/headset/infrastructure/HeadsetControlSettingsWriter.hpp"

#include "modules/headset/infrastructure/HeadsetControlActionJson.hpp"

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

#include <algorithm>
#include <string>
#include <utility>

namespace ghub::headset {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;

constexpr qsizetype maxOutputBytes = qsizetype{64} * 1024;
constexpr int pollSliceMs = 50;
constexpr int reapTimeoutMs = 2000;

// `headsetcontrol` sai com 1 quando não há dispositivo; só então a saída 1 é aceita como "sem headset".
[[nodiscard]] bool reportsZeroDevices(const QByteArray& output)
{
    const auto count = QJsonDocument::fromJson(output).object().value("device_count");
    return count.isDouble() && count.toInt(-1) == 0;
}

void stop(QProcess& process)
{
    process.kill();
    process.waitForFinished(reapTimeoutMs);
}

} // namespace

HeadsetControlSettingsWriter::HeadsetControlSettingsWriter(QString executable, std::chrono::milliseconds timeout)
    : executable_(std::move(executable)), timeout_(timeout)
{
}

shared_kernel::Result<void> HeadsetControlSettingsWriter::applySidetone(SidetoneLevel level)
{
    return run({QStringLiteral("-s"), QString::number(level.value())}, "CAP_SIDETONE");
}

shared_kernel::Result<void> HeadsetControlSettingsWriter::applyLights(HeadsetLights lights)
{
    return run({QStringLiteral("-l"), lights == HeadsetLights::On ? QStringLiteral("1") : QStringLiteral("0")},
               "CAP_LIGHTS");
}

shared_kernel::Result<void> HeadsetControlSettingsWriter::run(const QStringList& option, const char* capability) const
{
    if (executable_.isEmpty()) {
        return fail(ErrorCode::BackendUnavailable, "headsetcontrol não encontrado");
    }

    QProcessEnvironment environment;
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setStandardErrorFile(QProcess::nullDevice());

    QStringList arguments{QStringLiteral("-d"), QStringLiteral("0x046d:0x0afe")};
    arguments += option;
    arguments += {QStringLiteral("-o"), QStringLiteral("json")};

    QElapsedTimer elapsed;
    elapsed.start();
    process.start(executable_, arguments);
    process.closeWriteChannel();
    if (!process.waitForStarted(static_cast<int>(timeout_.count()))) {
        return fail(ErrorCode::BackendUnavailable, "headsetcontrol não pôde ser iniciado");
    }

    QByteArray output;
    while (process.state() != QProcess::NotRunning) {
        const auto remaining = timeout_.count() - elapsed.elapsed();
        if (remaining <= 0) {
            stop(process);
            return fail(ErrorCode::BackendUnavailable, "headsetcontrol excedeu o tempo");
        }
        process.waitForFinished(static_cast<int>(std::min<qint64>(remaining, pollSliceMs)));
        output += process.readAllStandardOutput();
        if (output.size() > maxOutputBytes) {
            stop(process);
            return fail(ErrorCode::MalformedData, "saída excede 64 KiB");
        }
    }
    output += process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit) {
        return fail(ErrorCode::BackendUnavailable, "headsetcontrol terminou de forma anormal");
    }
    if (output.size() > maxOutputBytes) {
        return fail(ErrorCode::MalformedData, "saída excede 64 KiB");
    }
    const int exitCode = process.exitCode();
    if (exitCode == 0) {
        return parseHeadsetControlAction(output, capability);
    }
    if (exitCode == 1 && reportsZeroDevices(output)) {
        return fail(ErrorCode::BackendUnavailable, "headset não encontrado");
    }
    return fail(ErrorCode::BackendUnavailable, "headsetcontrol saiu com " + std::to_string(exitCode));
}

} // namespace ghub::headset
