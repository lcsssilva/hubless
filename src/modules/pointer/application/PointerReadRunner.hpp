#pragma once

#include "modules/pointer/application/PointerStatus.hpp"

#include <optional>

namespace ghub::pointer {

// Port: executa a leitura fora da thread de UI. O resultado chega de forma assíncrona por quem
// implementa o port; `lastStatus` devolve a última leitura concluída.
class PointerReadRunner {
public:
    virtual ~PointerReadRunner() = default;

    // Inicia uma leitura; ignorado se já houver uma em andamento.
    virtual void start() = 0;
    [[nodiscard]] virtual bool isReading() const = 0;
    // Vazio até a primeira leitura terminar.
    [[nodiscard]] virtual std::optional<PointerStatus> lastStatus() const = 0;

protected:
    PointerReadRunner() = default;
    PointerReadRunner(const PointerReadRunner&) = default;
    PointerReadRunner& operator=(const PointerReadRunner&) = default;
    PointerReadRunner(PointerReadRunner&&) = default;
    PointerReadRunner& operator=(PointerReadRunner&&) = default;
};

} // namespace ghub::pointer
