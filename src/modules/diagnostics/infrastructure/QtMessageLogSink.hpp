#pragma once

#include "modules/diagnostics/application/DiagnosticSink.hpp"

#include <QtCore/qlogging.h>

namespace ghub::diagnostics {

// Instala qInstallMessageHandler e cria uma DiagnosticEntry por mensagem do Qt (INC-013 R12, ADR-0019 §5):
// severidade mapeada (Debug/Info/Warning/Critical-Fatal), instante em ISO com milissegundos, categoria do
// QMessageLogContext (ou "default") e a mensagem. Encadeia para o tratador que estava instalado antes; o Qt
// mantém sempre um tratador padrão não nulo (que escreve no stderr ou no journal, conforme o ambiente), então
// os avisos do produto continuam no destino de sempre. O tratador não loga dentro de si, portanto não há
// recursão.
//
// `sink` precisa sobreviver a este objeto. O tratador é global: uma instância por processo; não copiável.
// O destrutor restaura o tratador anterior.
class QtMessageLogSink {
public:
    explicit QtMessageLogSink(DiagnosticSink& sink);
    ~QtMessageLogSink();

    QtMessageLogSink(const QtMessageLogSink&) = delete;
    QtMessageLogSink& operator=(const QtMessageLogSink&) = delete;
    QtMessageLogSink(QtMessageLogSink&&) = delete;
    QtMessageLogSink& operator=(QtMessageLogSink&&) = delete;

private:
    static void handler(QtMsgType type, const QMessageLogContext& context, const QString& message);

    DiagnosticSink* sink_; // não nulo
};

} // namespace ghub::diagnostics
