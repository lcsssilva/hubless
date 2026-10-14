#pragma once

#include "modules/diagnostics/domain/DiagnosticTypes.hpp"

#include <string>

namespace ghub::diagnostics {

// Função pura: texto legível (pt-BR) do relatório, sem I/O e sem Qt (INC-013 R13). A ordem das entradas
// é preservada (mais recentes primeiro) e o texto termina com uma quebra de linha.
[[nodiscard]] std::string renderDiagnosticReport(const DiagnosticReportInput& input);

} // namespace ghub::diagnostics
