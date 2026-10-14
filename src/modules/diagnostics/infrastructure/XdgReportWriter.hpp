#pragma once

#include "modules/diagnostics/application/ReportWriter.hpp"
#include "shared_kernel/Result.hpp"

#include <QString>

#include <string>
#include <string_view>

namespace ghub::diagnostics {

// Grava o relatório em $XDG_STATE_HOME/ghub-linux/diagnosticos (fallback ~/.local/state) com o nome
// diagnostico-<yyyyMMdd-hhmmss>.txt, de forma atômica, e devolve o caminho em UTF-8 (INC-013 R13,
// ADR-0019 §5). O diretório é criado na primeira gravação; nada é gravado fora de `write`.
class XdgReportWriter final : public ReportWriter {
public:
    // `directory` vazio: diretório de estado do usuário (QStandardPaths::GenericStateLocation).
    explicit XdgReportWriter(QString directory = {});

    [[nodiscard]] shared_kernel::Result<std::string> write(std::string_view content) const override;

private:
    QString directory_;
};

} // namespace ghub::diagnostics
