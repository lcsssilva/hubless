#include "modules/lighting/presentation/KeyboardStatusViewModel.hpp"

#include <QStringList>
#include <QtConcurrent>

namespace ghub::lighting::presentation {

namespace {

QString notInformed()
{
    return QStringLiteral("não informado");
}

QString hertzOf(unsigned periodMs)
{
    return QString::number(1000U / periodMs);
}

QString brightnessText(const KeyboardStatus& status)
{
    if (status.currentBrightness && status.brightnessLimits) {
        return QStringLiteral("%1 de %2").arg(*status.currentBrightness).arg(status.brightnessLimits->maximum);
    }
    if (status.currentBrightness) {
        return QString::number(*status.currentBrightness);
    }
    if (status.brightnessLimits) {
        return QStringLiteral("máximo %1").arg(status.brightnessLimits->maximum);
    }
    return notInformed();
}

QString unexplainedText(const KeyboardStatus& status)
{
    if (!status.brightnessLimits) {
        return {};
    }
    const auto& raw = status.brightnessLimits->unexplained;
    return QStringLiteral("%1 %2 (não interpretado)")
        .arg(raw[0], 2, 16, QLatin1Char('0'))
        .arg(raw[1], 2, 16, QLatin1Char('0'));
}

QString reportRateText(const KeyboardStatus& status)
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

QString supportedRatesText(const KeyboardStatus& status)
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

QVariantMap viewOf(const KeyboardStatus& status)
{
    return {
        {QStringLiteral("brightness"), brightnessText(status)},
        {QStringLiteral("brightnessUnexplained"), unexplainedText(status)},
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

KeyboardStatusViewModel::KeyboardStatusViewModel(const application::ReadKeyboardStatus& useCase, QObject* parent)
    : QObject(parent), useCase_(&useCase)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, [this] { onFinished(); });
}

KeyboardStatusViewModel::~KeyboardStatusViewModel()
{
    // A leitura não é cancelável e usa o caso de uso: espera terminar antes de destruir qualquer coisa.
    watcher_.disconnect(this);
    watcher_.waitForFinished();
}

void KeyboardStatusViewModel::read()
{
    if (reading_) {
        return;
    }
    reading_ = true;
    const bool hadResult = !status_.isEmpty();
    const bool hadError = !error_.isEmpty();
    status_.clear();
    error_.clear();
    const application::ReadKeyboardStatus& useCase = *useCase_;
    watcher_.setFuture(QtConcurrent::run([&useCase] { return Outcome{useCase.execute()}; }));
    emit readingChanged();
    if (hadResult) {
        emit statusChanged();
    }
    if (hadError) {
        emit errorChanged();
    }
}

void KeyboardStatusViewModel::onFinished()
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

} // namespace ghub::lighting::presentation
