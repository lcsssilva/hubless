#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ghub::diagnostics {

// Severidade de uma entrada de diagnóstico (ADR-0019 §5, INC-013 R10).
enum class DiagnosticSeverity : std::uint8_t { Debug, Info, Warning, Critical };

// Nome estável da severidade, usado no relatório e na lista exibida ao usuário.
[[nodiscard]] std::string severityName(DiagnosticSeverity severity);

// Uma mensagem de diagnóstico já normalizada (instante em texto, severidade, categoria e mensagem).
struct DiagnosticEntry {
    std::string timestamp;
    DiagnosticSeverity severity;
    std::string category;
    std::string message;
};

// Saúde de um backend consultado pelo composition root (INC-013 R15).
struct BackendHealth {
    std::string name;
    bool available;
    std::string detail;
};

// Tudo o que o relatório de diagnóstico precisa; as entradas vêm das mais recentes para as mais antigas.
struct DiagnosticReportInput {
    std::string generatedAt;
    std::string appVersion;
    std::string platform;
    std::vector<BackendHealth> backends;
    std::vector<DiagnosticEntry> entries;
};

} // namespace ghub::diagnostics
