#pragma once

#include "shared_kernel/Result.hpp"

#include <string>
#include <string_view>

namespace ghub::diagnostics {

// Port: grava o texto de um relatório e devolve o caminho gravado (INC-013 R13). O caso de uso não
// conhece o sistema de arquivos; a infraestrutura implementa.
class ReportWriter {
public:
    virtual ~ReportWriter() = default;

    [[nodiscard]] virtual shared_kernel::Result<std::string> write(std::string_view content) const = 0;

protected:
    ReportWriter() = default;
    ReportWriter(const ReportWriter&) = default;
    ReportWriter& operator=(const ReportWriter&) = default;
    ReportWriter(ReportWriter&&) = default;
    ReportWriter& operator=(ReportWriter&&) = default;
};

} // namespace ghub::diagnostics
