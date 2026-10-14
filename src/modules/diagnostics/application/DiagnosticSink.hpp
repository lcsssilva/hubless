#pragma once

#include "modules/diagnostics/domain/DiagnosticTypes.hpp"

namespace ghub::diagnostics {

// Port: destino das mensagens de diagnóstico (ADR-0019 §5). A implementação decide onde guardar; a
// aplicação não conhece disco nem Qt. `record` nunca deve lançar: quem chama pode ser um tratador de
// mensagem do Qt, em qualquer thread.
class DiagnosticSink {
public:
    virtual ~DiagnosticSink() = default;

    virtual void record(DiagnosticEntry entry) = 0;

protected:
    DiagnosticSink() = default;
    DiagnosticSink(const DiagnosticSink&) = default;
    DiagnosticSink& operator=(const DiagnosticSink&) = default;
    DiagnosticSink(DiagnosticSink&&) = default;
    DiagnosticSink& operator=(DiagnosticSink&&) = default;
};

} // namespace ghub::diagnostics
