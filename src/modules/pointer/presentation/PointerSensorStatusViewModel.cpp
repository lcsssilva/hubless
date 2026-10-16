#include "modules/pointer/presentation/PointerSensorStatusViewModel.hpp"

#include <QStringList>
#include <QtConcurrent>

namespace ghub::pointer {

namespace {

QString notInformed()
{
    return QStringLiteral("não informado");
}

QString hertzOf(unsigned periodMs)
{
    return QString::number(1000U / periodMs);
}

QString segmentText(const DpiSegment& segment)
{
    if (segment.step == 0) {
        return QString::number(segment.minimum);
    }
    return QStringLiteral("%1–%2, passo %3").arg(segment.minimum).arg(segment.maximum).arg(segment.step);
}

QString dpiRangeText(const std::optional<DpiList>& list)
{
    if (!list) {
        return notInformed();
    }
    if (!list->valid()) {
        QStringList raw;
        for (const auto byte : list->rawBytes()) {
            raw << QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0'));
        }
        return QStringLiteral("lista inválida (bytes brutos: %1)").arg(raw.join(QLatin1Char(' ')));
    }
    QStringList parts;
    for (const auto& segment : list->segments()) {
        parts << segmentText(segment);
    }
    return parts.join(QStringLiteral("; "));
}

QString dpiText(const std::optional<DpiSetting>& setting)
{
    if (!setting) {
        return notInformed();
    }
    return QStringLiteral("%1 (padrão %2)").arg(setting->current).arg(setting->defaultDpi);
}

QString reportRateText(const PointerSensorStatus& status)
{
    if (!status.currentPeriodMs) {
        return notInformed();
    }
    auto text = QStringLiteral("%1 Hz (%2 ms)").arg(hertzOf(*status.currentPeriodMs)).arg(*status.currentPeriodMs);
    if (status.reportRateSupport && !status.reportRateSupport->supports(*status.currentPeriodMs)) {
        text += QStringLiteral(" — fora da lista suportada");
    }
    return text;
}

QString supportedRatesText(const PointerSensorStatus& status)
{
    if (!status.reportRateSupport) {
        return notInformed();
    }
    QStringList rates;
    for (const auto period : status.reportRateSupport->periodsMs()) {
        rates << hertzOf(period);
    }
    return rates.join(QStringLiteral(", ")) + QStringLiteral(" Hz");
}

QVariantMap viewOf(const PointerSensorStatus& status)
{
    return {
        {QStringLiteral("dpiRange"), dpiRangeText(status.dpiList)},
        {QStringLiteral("dpi"), dpiText(status.dpiSetting)},
        {QStringLiteral("reportRate"), reportRateText(status)},
        {QStringLiteral("supportedReportRates"), supportedRatesText(status)},
    };
}

QString errorText(const shared_kernel::DomainError& error)
{
    const auto detail = QString::fromStdString(error.detail);
    return error.code == shared_kernel::ErrorCode::DeviceBusy ? QStringLiteral("ocupado: ") + detail : detail;
}

} // namespace

PointerSensorStatusViewModel::PointerSensorStatusViewModel(const ReadPointerSensorStatus& useCase, QObject* parent)
    : QObject(parent), useCase_(&useCase)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, [this] { onFinished(); });
}

PointerSensorStatusViewModel::~PointerSensorStatusViewModel()
{
    // A leitura não é cancelável e usa o caso de uso: espera terminar antes de destruir qualquer coisa.
    watcher_.disconnect(this);
    watcher_.waitForFinished();
}

void PointerSensorStatusViewModel::read()
{
    if (reading_) {
        return;
    }
    reading_ = true;
    const bool hadResult = !status_.isEmpty();
    const bool hadError = !error_.isEmpty();
    status_.clear();
    error_.clear();
    const ReadPointerSensorStatus& useCase = *useCase_;
    watcher_.setFuture(QtConcurrent::run([&useCase] { return Outcome{useCase.execute()}; }));
    emit readingChanged();
    if (hadResult) {
        emit statusChanged();
    }
    if (hadError) {
        emit errorChanged();
    }
}

void PointerSensorStatusViewModel::onFinished()
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

} // namespace ghub::pointer
