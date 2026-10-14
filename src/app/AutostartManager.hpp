#pragma once

#include "shared_kernel/Result.hpp"

#include <QString>

namespace ghub::app {

// Lê, grava e remove o `.desktop` de inicialização automática do usuário em
// `$XDG_CONFIG_HOME/autostart` (fallback `~/.config/autostart`), conforme a spec XDG Autostart
// (INC-013 R7, ADR-0019 §4). O diretório é injetável para o teste usar diretório temporário.
//
// Nada é escrito na construção: só `setEnabled` toca o disco, e só por pedido explícito do usuário.
class AutostartManager {
public:
    explicit AutostartManager(QString directory = {});

    // `directory_/ghub-linux.desktop`.
    [[nodiscard]] QString filePath() const;

    // Arquivo presente e sem `Hidden=true` (o app nunca escreve essa chave, mas ela pode vir de fora).
    // Arquivo ausente ou ilegível conta como desligado: sem ler o conteúdo não há como afirmar que a
    // entrada está ativa.
    [[nodiscard]] bool isEnabled() const;

    // `true` cria o diretório e grava de forma atômica; `false` remove o arquivo. Idempotente. Não lança:
    // falha de sistema de arquivos vira Result de erro em pt-BR.
    [[nodiscard]] shared_kernel::Result<void> setEnabled(bool enabled) const;

private:
    QString directory_;
};

} // namespace ghub::app
