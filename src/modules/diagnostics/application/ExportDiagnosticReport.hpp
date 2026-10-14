#pragma once

#include "modules/diagnostics/application/ReportWriter.hpp"
#include "modules/diagnostics/domain/DiagnosticTypes.hpp"
#include "shared_kernel/Result.hpp"

#include <string>

namespace ghub::diagnostics {

// Caso de uso: renderiza o relatório e grava pelo port `ReportWriter` (INC-013 R13). Nada é gravado na
// construção nem por conta própria: só quando `run` é chamado. `writer` precisa sobreviver ao caso de uso.
class ExportDiagnosticReport {
public:
    explicit ExportDiagnosticReport(const ReportWriter& writer) : writer_(&writer) {}

    // Caminho gravado em caso de sucesso; erro do writer é propagado sem virar sucesso.
    [[nodiscard]] shared_kernel::Result<std::string> run(const DiagnosticReportInput& input) const;

private:
    const ReportWriter* writer_; // não nulo
};

} // namespace ghub::diagnostics
