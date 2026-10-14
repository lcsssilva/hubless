#pragma once

#include <QString>

namespace ghub::app {

// Entrada XDG de inicialização automática (INC-013 R6, ADR-0019 §4). Só dados: nenhum I/O aqui, o
// arquivo é escrito pelo AutostartManager.
struct AutostartEntry {
    QString name;
    QString comment;
    QString exec;
    QString icon;
};

// Entrada padrão do app: nome do produto, execução em segundo plano e ícone do próprio pacote.
[[nodiscard]] AutostartEntry defaultAutostartEntry();

// Gera o conteúdo completo do `.desktop`, na ordem da spec Desktop Entry, terminando em '\n'
// (ferramentas do desktop ignoram a última linha sem quebra).
[[nodiscard]] QString renderAutostartEntry(const AutostartEntry& entry);

} // namespace ghub::app
