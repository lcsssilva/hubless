#include "modules/diagnostics/presentation/DiagnosticsViewModel.hpp"

#include <QDateTime>
#include <QVariantMap>

#include <utility>

namespace ghub::diagnostics {

DiagnosticsViewModel::DiagnosticsViewModel(DiagnosticLog& log, const ExportDiagnosticReport& exportReport,
                                           std::string appVersion, std::string platform,
                                           BackendHealthProvider healthProvider, QObject* parent)
    : QObject(parent), log_(&log), exportReport_(&exportReport), appVersion_(std::move(appVersion)),
      platform_(std::move(platform)), healthProvider_(std::move(healthProvider))
{
}

void DiagnosticsViewModel::refresh()
{
    backendData_.clear();
    if (healthProvider_) {
        backendData_ = healthProvider_();
    }
    QVariantList backends;
    for (const auto& backend : backendData_) {
        backends.append(QVariantMap{
            {QStringLiteral("name"), QString::fromStdString(backend.name)},
            {QStringLiteral("available"), backend.available},
            {QStringLiteral("detail"), QString::fromStdString(backend.detail)},
        });
    }

    entryData_ = log_->snapshot();
    QVariantList entries;
    for (const auto& entry : entryData_) {
        entries.append(QVariantMap{
            {QStringLiteral("timestamp"), QString::fromStdString(entry.timestamp)},
            {QStringLiteral("severity"), QString::fromStdString(severityName(entry.severity))},
            {QStringLiteral("category"), QString::fromStdString(entry.category)},
            {QStringLiteral("message"), QString::fromStdString(entry.message)},
        });
    }

    backends_ = std::move(backends);
    entries_ = std::move(entries);
    emit changed();
}

void DiagnosticsViewModel::exportReport()
{
    const DiagnosticReportInput input{
        .generatedAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toStdString(),
        .appVersion = appVersion_,
        .platform = platform_,
        .backends = backendData_,
        .entries = entryData_,
    };

    const auto result = exportReport_->run(input);
    if (result.has_value()) {
        lastReportPath_ = QString::fromStdString(*result);
        status_ = tr("relatório gravado em %1").arg(lastReportPath_);
    } else {
        status_ = tr("falha ao exportar o relatório: %1").arg(QString::fromStdString(result.error().detail));
    }
    emit changed();
}

} // namespace ghub::diagnostics
