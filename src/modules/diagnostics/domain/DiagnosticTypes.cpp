#include "modules/diagnostics/domain/DiagnosticTypes.hpp"

namespace ghub::diagnostics {

std::string severityName(DiagnosticSeverity severity)
{
    switch (severity) {
    case DiagnosticSeverity::Debug:
        return "Debug";
    case DiagnosticSeverity::Info:
        return "Info";
    case DiagnosticSeverity::Warning:
        return "Warning";
    case DiagnosticSeverity::Critical:
        return "Critical";
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

} // namespace ghub::diagnostics
