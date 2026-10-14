#include "modules/diagnostics/infrastructure/QtMessageLogSink.hpp"

#include <QDateTime>
#include <QtCore/qlogging.h>

#include <atomic>
#include <mutex>

namespace ghub::diagnostics {

namespace {

DiagnosticSeverity severityOf(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return DiagnosticSeverity::Debug;
    case QtInfoMsg:
        return DiagnosticSeverity::Info;
    case QtWarningMsg:
        return DiagnosticSeverity::Warning;
    case QtCriticalMsg:
    case QtFatalMsg:
        return DiagnosticSeverity::Critical;
    }
    return DiagnosticSeverity::Critical; // inalcançável: o switch cobre todos os enumeradores
}

// Estado do tratador, que é global do processo: quem registra e o tratador anterior (encadeamento).
struct HandlerState {
    std::mutex mutex; // serializa instalação e restauração
    std::atomic<DiagnosticSink*> sink{nullptr};
    std::atomic<QtMessageHandler> previous{nullptr};
};

HandlerState& state()
{
    static HandlerState globalState;
    return globalState;
}

} // namespace

QtMessageLogSink::QtMessageLogSink(DiagnosticSink& sink) : sink_(&sink)
{
    const std::lock_guard lock{state().mutex};
    state().sink.store(sink_, std::memory_order_release);
    const auto previous = qInstallMessageHandler(&QtMessageLogSink::handler);
    // Duas instâncias no mesmo processo encadeariam uma na outra: a nossa anterior é o default.
    state().previous.store(previous == &QtMessageLogSink::handler ? nullptr : previous, std::memory_order_release);
}

QtMessageLogSink::~QtMessageLogSink()
{
    const std::lock_guard lock{state().mutex};
    state().sink.store(nullptr, std::memory_order_release);
    const auto previous = state().previous.exchange(nullptr, std::memory_order_acq_rel);
    qInstallMessageHandler(previous);
}

void QtMessageLogSink::handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // Sem logar aqui dentro: o tratador não pode reentrar (ADR-0019, consequências).
    auto* const sink = state().sink.load(std::memory_order_acquire);
    if (sink != nullptr) {
        sink->record(DiagnosticEntry{
            .timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toStdString(),
            .severity = severityOf(type),
            .category = context.category != nullptr ? context.category : "default",
            .message = message.toStdString(),
        });
    }
    const auto previous = state().previous.load(std::memory_order_acquire);
    if (previous != nullptr) {
        previous(type, context, message);
    }
}

} // namespace ghub::diagnostics
