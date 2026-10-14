#include "modules/diagnostics/application/DiagnosticLog.hpp"

#include <utility>

namespace ghub::diagnostics {

DiagnosticLog::DiagnosticLog(std::size_t capacity) : capacity_(capacity) {}

void DiagnosticLog::record(DiagnosticEntry entry)
{
    const std::lock_guard lock{mutex_};
    entries_.push_back(std::move(entry));
    while (entries_.size() > capacity_) {
        entries_.pop_front();
    }
}

std::vector<DiagnosticEntry> DiagnosticLog::snapshot() const
{
    const std::lock_guard lock{mutex_};
    return std::vector<DiagnosticEntry>(entries_.rbegin(), entries_.rend());
}

std::size_t DiagnosticLog::size() const
{
    const std::lock_guard lock{mutex_};
    return entries_.size();
}

void DiagnosticLog::clear()
{
    const std::lock_guard lock{mutex_};
    entries_.clear();
}

} // namespace ghub::diagnostics
