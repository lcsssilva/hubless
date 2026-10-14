#include "modules/battery/infrastructure/HeadsetControlBatterySource.hpp"

#include "modules/battery/infrastructure/HeadsetControlJson.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStringList>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <utility>

namespace ghub::battery {

namespace {

using device_access::ExternalOwner;
using shared_kernel::DomainError;
using shared_kernel::ErrorCode;
using shared_kernel::fail;

constexpr device_inventory::UsbId g733{.vendorId = 0x046d, .productId = 0x0afe};
constexpr auto leaseMargin = std::chrono::seconds{5};
constexpr qsizetype maxOutputBytes = qsizetype{64} * 1024;

// Softwares que também abrem o hidraw do G733 (ADR-0007 §3, DEVICE_ACCESS_POLICY §3).
constexpr std::array conflictingOwners{
    ExternalOwner::OpenRgb,
    ExternalOwner::Solaar,
    ExternalOwner::Ratbagd,
    ExternalOwner::HeadsetControl,
};

[[nodiscard]] bool conflicts(ExternalOwner owner)
{
    return std::ranges::any_of(conflictingOwners, [owner](ExternalOwner candidate) { return candidate == owner; });
}

// `headsetcontrol` sai com 1 quando não há dispositivo; só então a saída 1 é aceita.
[[nodiscard]] bool reportsZeroDevices(const QByteArray& output)
{
    const auto root = QJsonDocument::fromJson(output).object();
    const auto count = root.value("device_count");
    return count.isDouble() && count.toInt(-1) == 0;
}

} // namespace

HeadsetControlBatterySource::HeadsetControlBatterySource(QString executable, device_access::ExternalProcessProbe& probe,
                                                         device_access::AccessRegistry& registry,
                                                         device_access::PhysicalInterfaceId interfaceId,
                                                         std::chrono::milliseconds pollInterval,
                                                         std::chrono::milliseconds timeout, QObject* parent)
    : QObject(parent), executable_(std::move(executable)), probe_(probe), registry_(registry),
      interfaceId_(std::move(interfaceId)), timeout_(timeout), process_(this), pollTimer_(this), timeoutTimer_(this)
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process_.setProcessEnvironment(environment);
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    process_.setStandardErrorFile(QProcess::nullDevice());
    connect(&process_, &QProcess::readyReadStandardOutput, this, &HeadsetControlBatterySource::onReadyRead);
    connect(&process_, &QProcess::finished, this, &HeadsetControlBatterySource::onFinished);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && running_) {
            finish(fail(ErrorCode::BackendUnavailable, "headsetcontrol não pôde ser iniciado"));
        }
    });

    timeoutTimer_.setSingleShot(true);
    timeoutTimer_.setInterval(timeout_);
    connect(&timeoutTimer_, &QTimer::timeout, this,
            [this] { abort({.code = ErrorCode::BackendUnavailable, .detail = "headsetcontrol excedeu o tempo"}); });

    pollTimer_.setInterval(pollInterval);
    connect(&pollTimer_, &QTimer::timeout, this, &HeadsetControlBatterySource::requestUpdate);
    pollTimer_.start();
    QTimer::singleShot(0, this, &HeadsetControlBatterySource::requestUpdate);
}

HeadsetControlBatterySource::~HeadsetControlBatterySource()
{
    // RAII: nenhum processo órfão. O QProcess é membro; encerra e recolhe o filho aqui.
    if (process_.state() != QProcess::NotRunning) {
        process_.disconnect(this);
        process_.kill();
        process_.waitForFinished(2000);
    }
}

shared_kernel::Result<std::vector<SourcedBatteryReading>> HeadsetControlBatterySource::readAll()
{
    return cache_;
}

void HeadsetControlBatterySource::requestUpdate()
{
    if (running_) {
        return;
    }
    running_ = true;
    forcedError_.reset();
    output_.clear();

    const auto owners = probe_.runningOwners();
    if (!owners) {
        finish(fail(ErrorCode::BackendUnavailable, "não foi possível verificar processos externos"));
        return;
    }
    for (const auto owner : *owners) {
        if (conflicts(owner)) {
            finish(fail(ErrorCode::DeviceBusy, "em uso por " + std::string(device_access::processName(owner))));
            return;
        }
    }
    if (executable_.isEmpty()) {
        finish(fail(ErrorCode::BackendUnavailable, "headsetcontrol não encontrado"));
        return;
    }
    auto holder = device_access::LeaseHolder::create("battery.headsetcontrol");
    if (!holder) {
        finish(std::unexpected(holder.error()));
        return;
    }
    auto lease = registry_.acquire({
        .interfaceId = interfaceId_,
        .holder = std::move(*holder),
        .purpose = "consulta de bateria do G733",
        .maxDuration = timeout_ + leaseMargin,
    });
    if (!lease) {
        finish(std::unexpected(lease.error()));
        return;
    }
    lease_.emplace(std::move(*lease));
    startQuery();
}

void HeadsetControlBatterySource::startQuery()
{
    process_.start(executable_, {
                                    QStringLiteral("-d"),
                                    QStringLiteral("0x046d:0x0afe"),
                                    QStringLiteral("-b"),
                                    QStringLiteral("-o"),
                                    QStringLiteral("json"),
                                });
    process_.closeWriteChannel();
    if (running_) {
        timeoutTimer_.start();
    }
}

void HeadsetControlBatterySource::finish(shared_kernel::Result<std::optional<BatteryReading>> outcome)
{
    timeoutTimer_.stop();
    running_ = false;
    lease_.reset();
    cache_.clear();
    if (!outcome) {
        cache_.push_back(SourcedBatteryReading{.device = g733, .reading = std::unexpected(outcome.error())});
    } else if (outcome->has_value()) {
        cache_.push_back(SourcedBatteryReading{.device = g733, .reading = **outcome});
    }
    emit readingsChanged();
}

void HeadsetControlBatterySource::abort(shared_kernel::DomainError error)
{
    if (!running_ || forcedError_) {
        return;
    }
    forcedError_ = std::move(error);
    timeoutTimer_.stop();
    process_.kill(); // o resultado é publicado quando o processo terminar (onFinished)
}

void HeadsetControlBatterySource::onReadyRead()
{
    if (!running_ || forcedError_) {
        process_.readAllStandardOutput(); // descarta
        return;
    }
    output_ += process_.readAllStandardOutput();
    if (output_.size() > maxOutputBytes) {
        output_.clear();
        abort({.code = ErrorCode::MalformedData, .detail = "saída excede 64 KiB"});
    }
}

void HeadsetControlBatterySource::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!running_) {
        return;
    }
    if (forcedError_) {
        auto error = std::move(*forcedError_);
        forcedError_.reset();
        finish(std::unexpected(std::move(error)));
        return;
    }
    output_ += process_.readAllStandardOutput();
    if (status != QProcess::NormalExit) {
        finish(fail(ErrorCode::BackendUnavailable, "headsetcontrol terminou de forma anormal"));
        return;
    }
    if (output_.size() > maxOutputBytes) {
        finish(fail(ErrorCode::MalformedData, "saída excede 64 KiB"));
        return;
    }
    if (exitCode == 0) {
        finish(parseHeadsetControlBattery(output_));
        return;
    }
    if (exitCode == 1) {
        const auto parsed = parseHeadsetControlBattery(output_);
        if (parsed && !parsed->has_value() && reportsZeroDevices(output_)) {
            finish(std::optional<BatteryReading>{});
            return;
        }
    }
    finish(fail(ErrorCode::BackendUnavailable, "headsetcontrol saiu com " + std::to_string(exitCode)));
}

} // namespace ghub::battery
