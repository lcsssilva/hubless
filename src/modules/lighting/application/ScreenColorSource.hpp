#pragma once

#include "modules/lighting/domain/RgbColor.hpp"
#include "shared_kernel/Result.hpp"

#include <vector>

namespace ghub::lighting::application {

// INC-021 R4 (ADR-0030): port da fonte de cores médias da tela (10 regiões em uma linha, da esquerda para a direita).
// Privacidade: a captura só existe entre start() e stop(); só as médias saem da implementação, nenhum quadro é
// guardado nem registrado em log. start() pode demorar (o sistema pede a permissão do usuário a cada sessão) e
// stop() pode ser chamado de OUTRA thread para cancelar um start() pendente. regions() é seguro de qualquer thread.
class ScreenColorSource {
public:
    virtual ~ScreenColorSource() = default;

    // Pede permissão e abre a captura. Já ligada -> sucesso sem efeito. Negada, cancelada ou sem portal/PipeWire ->
    // BackendUnavailable (e nada fica aberto).
    [[nodiscard]] virtual shared_kernel::Result<void> start() = 0;
    // Fecha a captura (e a sessão de permissão) e esquece as médias. Idempotente; cancela um start() pendente.
    virtual void stop() = 0;
    [[nodiscard]] virtual bool running() const = 0;
    // As 10 médias do último quadro; vazio quando parada ou antes do primeiro quadro.
    [[nodiscard]] virtual std::vector<domain::RgbColor> regions() const = 0;

protected:
    ScreenColorSource() = default;
    ScreenColorSource(const ScreenColorSource&) = default;
    ScreenColorSource& operator=(const ScreenColorSource&) = default;
    ScreenColorSource(ScreenColorSource&&) = default;
    ScreenColorSource& operator=(ScreenColorSource&&) = default;
};

} // namespace ghub::lighting::application
