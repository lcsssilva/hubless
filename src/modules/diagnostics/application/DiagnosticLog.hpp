#pragma once

#include "modules/diagnostics/application/DiagnosticSink.hpp"
#include "modules/diagnostics/domain/DiagnosticTypes.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

namespace ghub::diagnostics {

// Buffer circular limitado das últimas entradas (INC-013 R11, ADR-0019 §5). Thread-safe: o tratador do
// Qt pode chamar `record` de qualquer thread. Não grava em disco e `record` nunca lança. As entradas
// são expostas das mais recentes para as mais antigas.
class DiagnosticLog final : public DiagnosticSink {
public:
    explicit DiagnosticLog(std::size_t capacity = 500);

    void record(DiagnosticEntry entry) override;

    [[nodiscard]] std::vector<DiagnosticEntry> snapshot() const;
    [[nodiscard]] std::size_t size() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::deque<DiagnosticEntry> entries_;
    std::size_t capacity_;
};

} // namespace ghub::diagnostics
