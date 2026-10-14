#pragma once

#include "modules/diagnostics/application/DiagnosticLog.hpp"
#include "modules/diagnostics/application/ExportDiagnosticReport.hpp"
#include "modules/diagnostics/domain/DiagnosticTypes.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>
#include <string>
#include <vector>

namespace ghub::diagnostics {

// Expõe ao QML a saúde dos backends e as últimas entradas do log, e exporta o relatório só por clique
// (INC-013 R14, ADR-0019 §5/§7). Nada é lido nem gravado na construção; o relatório sai do que está
// exibido (não relê os backends no momento da exportação).
//
// `log` e `exportReport` precisam sobreviver ao ViewModel. `healthProvider` é montado pelo composition
// root (R15): fornece a lista de backends, é chamado em `refresh()` e não pode lançar (uma falha do
// backend vira um item com `available` falso).
class DiagnosticsViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList backends READ backends NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString lastReportPath READ lastReportPath NOTIFY changed)

public:
    using BackendHealthProvider = std::function<std::vector<BackendHealth>()>;

    DiagnosticsViewModel(DiagnosticLog& log, const ExportDiagnosticReport& exportReport, std::string appVersion,
                         std::string platform, BackendHealthProvider healthProvider, QObject* parent = nullptr);

    [[nodiscard]] QVariantList backends() const { return backends_; }
    [[nodiscard]] QVariantList entries() const { return entries_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QString lastReportPath() const { return lastReportPath_; }

    // Relê a saúde dos backends e as últimas entradas do log.
    Q_INVOKABLE void refresh();
    // Monta o relatório com o que está exibido, grava pelo caso de uso e publica o caminho; falha vira
    // mensagem no status.
    Q_INVOKABLE void exportReport();

signals:
    void changed();

private:
    DiagnosticLog* log_;                         // não nulo
    const ExportDiagnosticReport* exportReport_; // não nulo
    std::string appVersion_;
    std::string platform_;
    BackendHealthProvider healthProvider_;
    std::vector<BackendHealth> backendData_;
    std::vector<DiagnosticEntry> entryData_;
    QVariantList backends_;
    QVariantList entries_;
    QString status_;
    QString lastReportPath_;
};

} // namespace ghub::diagnostics
