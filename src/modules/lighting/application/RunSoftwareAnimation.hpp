#pragma once

#include "modules/lighting/application/LiveKeyboardSession.hpp"
#include "modules/lighting/domain/KeyColorMap.hpp"
#include "shared_kernel/Clock.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <functional>
#include <optional>

namespace ghub::lighting::application {

enum class TickOutcome : std::uint8_t {
    Skipped,   // o intervalo do limite de taxa ainda não passou: nada foi calculado nem enviado
    Unchanged, // quadro calculado igual ao anterior: nada enviado
    Sent,      // enviadas só as teclas que mudaram
};

// INC-021 R2 (ADR-0030): alimenta a sessão ao vivo do G915 com quadros de uma animação por software. Quem a cria
// (clique do usuário) já abriu a sessão; este caso de uso nunca abre nada. Dirigido por tick(): o agendador (um QTimer
// na apresentação) chama tick() e o relógio injetado decide se o intervalo passou, então os testes não dormem. Limite
// de taxa: padrão 10 quadros/s, mínimo 1 e máximo 20 (valores fora disso são limitados). O intervalo é 1000/fps ms;
// depois de um atraso longo envia UM quadro, nunca uma rajada. Só as teclas cujo valor mudou desde o último quadro
// enviado vão para a sessão. Falha de envio fecha a sessão (LiveKeyboardSession) e é devolvida; ticks seguintes
// devolvem InvalidArgument sem enviar nada. stop() devolve o controle ao firmware (uma vez). Não é segura para uso
// concorrente; `session` e `clock` precisam sobreviver a este objeto.
class RunSoftwareAnimation {
public:
    static constexpr unsigned kDefaultFramesPerSecond = 10;
    static constexpr unsigned kMaxFramesPerSecond = 20;

    // Função pura do tempo decorrido (ms desde o começo da animação) para o quadro completo.
    using FrameSource = std::function<domain::KeyColorMap(std::int64_t elapsedMs)>;

    RunSoftwareAnimation(LiveKeyboardSession& session, const shared_kernel::Clock& clock, FrameSource source,
                         unsigned framesPerSecond = kDefaultFramesPerSecond);

    [[nodiscard]] unsigned framesPerSecond() const noexcept { return framesPerSecond_; }

    [[nodiscard]] shared_kernel::Result<TickOutcome> tick();
    // Devolve o controle ao firmware. Idempotente.
    [[nodiscard]] shared_kernel::Result<void> stop();

private:
    LiveKeyboardSession* session_;      // não nulo
    const shared_kernel::Clock* clock_; // não nulo
    FrameSource source_;
    unsigned framesPerSecond_;
    shared_kernel::Instant start_;
    std::optional<shared_kernel::Instant> lastTick_;
    domain::KeyColorMap lastSent_;
};

} // namespace ghub::lighting::application
