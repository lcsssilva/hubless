#include "modules/diagnostics/application/ExportDiagnosticReport.hpp"

#include "modules/diagnostics/application/RenderDiagnosticReport.hpp"

namespace ghub::diagnostics {

shared_kernel::Result<std::string> ExportDiagnosticReport::run(const DiagnosticReportInput& input) const
{
    return writer_->write(renderDiagnosticReport(input));
}

} // namespace ghub::diagnostics
