#include "modules/diagnostics/application/RenderDiagnosticReport.hpp"

#include <string>

namespace ghub::diagnostics {

namespace {

std::string backendLine(const BackendHealth& backend)
{
    std::string line = "- " + backend.name + " — " + (backend.available ? "ok" : "indisponível");
    if (!backend.detail.empty()) {
        line += " — " + backend.detail;
    }
    return line + "\n";
}

std::string entryLine(const DiagnosticEntry& entry)
{
    return "- " + entry.timestamp + " [" + severityName(entry.severity) + "] " + entry.category + ": " + entry.message +
           "\n";
}

} // namespace

std::string renderDiagnosticReport(const DiagnosticReportInput& input)
{
    std::string text = "Relatório de diagnóstico do Hubless\n";
    text += "Gerado em: " + input.generatedAt + "\n";
    text += "Versão do app: " + input.appVersion + "\n";
    text += "Plataforma: " + input.platform + "\n";

    text += "\nBackends\n";
    for (const auto& backend : input.backends) {
        text += backendLine(backend);
    }

    text += "\nÚltimas mensagens\n";
    for (const auto& entry : input.entries) {
        text += entryLine(entry);
    }
    return text;
}

} // namespace ghub::diagnostics
