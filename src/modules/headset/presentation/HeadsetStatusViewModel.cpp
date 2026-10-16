#include "modules/headset/presentation/HeadsetStatusViewModel.hpp"

#include <QStringList>
#include <QtConcurrent>

#include <cstdint>
#include <span>

namespace ghub::headset {

namespace {

QString notInformed()
{
    return QStringLiteral("não informado");
}

QString hex(std::span<const std::uint8_t> bytes)
{
    QStringList parts;
    for (const auto byte : bytes) {
        parts << QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0'));
    }
    return parts.join(QLatin1Char(' ')) + QStringLiteral(" (não interpretado)");
}

QVariantMap viewOf(const HeadsetStatus& status)
{
    return {
        {QStringLiteral("equalizer"),
         status.equalizer ? QStringLiteral("%1 bandas").arg(status.equalizer->bandCount) : notInformed()},
        {QStringLiteral("equalizerUnexplained"), status.equalizer ? hex(status.equalizer->unexplained) : QString{}},
        {QStringLiteral("uninterpreted"), status.uninterpreted ? hex(status.uninterpreted->bytes) : notInformed()},
    };
}

QString errorText(const shared_kernel::DomainError& error)
{
    const auto detail = QString::fromStdString(error.detail);
    return error.code == shared_kernel::ErrorCode::DeviceBusy ? QStringLiteral("ocupado: ") + detail : detail;
}

} // namespace

HeadsetStatusViewModel::HeadsetStatusViewModel(const ReadHeadsetStatus& useCase, QObject* parent)
    : QObject(parent), useCase_(&useCase)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, [this] { onFinished(); });
}

HeadsetStatusViewModel::~HeadsetStatusViewModel()
{
    // A leitura não é cancelável e usa o caso de uso: espera terminar antes de destruir qualquer coisa.
    watcher_.disconnect(this);
    watcher_.waitForFinished();
}

void HeadsetStatusViewModel::read()
{
    if (reading_) {
        return;
    }
    reading_ = true;
    const bool hadResult = !status_.isEmpty();
    const bool hadError = !error_.isEmpty();
    status_.clear();
    error_.clear();
    const ReadHeadsetStatus& useCase = *useCase_;
    watcher_.setFuture(QtConcurrent::run([&useCase] { return Outcome{useCase.execute()}; }));
    emit readingChanged();
    if (hadResult) {
        emit statusChanged();
    }
    if (hadError) {
        emit errorChanged();
    }
}

void HeadsetStatusViewModel::onFinished()
{
    const Outcome outcome = watcher_.result();
    reading_ = false;
    if (outcome.result.has_value()) {
        status_ = viewOf(*outcome.result);
        emit statusChanged();
    } else {
        error_ = errorText(outcome.result.error());
        emit errorChanged();
    }
    emit readingChanged();
    emit readFinished();
}

} // namespace ghub::headset
