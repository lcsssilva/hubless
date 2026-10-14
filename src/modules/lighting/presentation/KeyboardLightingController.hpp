#pragma once

#include "modules/lighting/application/LiveKeyboardSession.hpp"
#include "modules/lighting/domain/KeyboardEffect.hpp"

#include <QColor>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

namespace ghub::lighting::presentation {

// Modo ao vivo do G915 para a janela (INC-008 parte C, ADR-0013). Nada é aberto nem escrito na construção:
// startLive(), paintAll() e stopLive() só rodam por ação do usuário (R5/R9). O efeito é volátil (R11): nada é
// gravado no teclado; desligar o modo ou destruir o controlador devolve o controle ao firmware.
// Executa o caso de uso numa thread de trabalho, uma operação por vez: um pedido feito durante uma operação
// substitui o pendente (só o último vale) e roda quando a atual termina. Sem retentativa. A sessão pertence a
// este objeto; só a operação em andamento a toca, e a thread de UI lê apenas `live`, atualizado ao fim de cada
// operação. Falhas são registradas só com código e detalhe, nunca com a cor (R12); o detalhe de DeviceBusy traz
// o nome do processo que disputa o teclado (R6).
// R15: enquanto o modo está ligado e nada está em andamento, uma sondagem (só leitura, na thread de trabalho) a cada
// `controlCheckInterval` verifica se o firmware retomou o controle (repouso do teclado); se sim, o modo é desligado
// sem enviar nada. Um pedido do usuário nunca é descartado pela sondagem. releaseForSleep() (R16) devolve o controle
// de forma síncrona antes da suspensão do sistema.
// O caso de uso deve sobreviver a este objeto. O destrutor descarta o pendente, espera a operação em andamento e
// fecha a sessão aberta (devolve o controle ao firmware): isso bloqueia o encerramento por alguns centésimos
// de segundo. Use sempre a partir da thread que o possui.
class KeyboardLightingController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live NOTIFY controllerChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY controllerChanged)
    Q_PROPERTY(QString status READ status NOTIFY controllerChanged)

public:
    explicit KeyboardLightingController(const application::StartLiveKeyboardSession& start,
                                        std::chrono::milliseconds controlCheckInterval = std::chrono::seconds{2},
                                        QObject* parent = nullptr);
    ~KeyboardLightingController() override;

    KeyboardLightingController(const KeyboardLightingController&) = delete;
    KeyboardLightingController& operator=(const KeyboardLightingController&) = delete;
    KeyboardLightingController(KeyboardLightingController&&) = delete;
    KeyboardLightingController& operator=(KeyboardLightingController&&) = delete;

    [[nodiscard]] bool live() const { return live_; }
    // A sondagem periódica é interna e não conta como ocupado.
    [[nodiscard]] bool busy() const { return running_ && runningKind_ != Kind::Check; }
    [[nodiscard]] QString status() const { return status_; }

    Q_INVOKABLE void startLive();
    Q_INVOKABLE void stopLive();
    Q_INVOKABLE void paintAll(const QColor& color);
    // R14: prévia de efeito de hardware nas teclas e, em seguida, o efeito correspondente no logo (logoCompanion)
    // enquanto o modo ao vivo está ligado. `kind`: wave, cycle, breathing, ripple ou static; parâmetros fixos (5 s,
    // brilho 100%, ripple 20); `color` vale para breathing, ripple e static. O efeito some ao desligar o modo ao vivo.
    // `kind` desconhecido não envia nada.
    Q_INVOKABLE void applyEffect(const QString& kind, const QColor& color);
    // R16: chamado da thread de UI antes da suspensão do sistema. Síncrono: descarta o pedido pendente, espera a
    // operação em andamento e devolve o controle ao firmware (sem sessão, não faz nada além de desligar o modo).
    Q_INVOKABLE void releaseForSleep();

signals:
    void controllerChanged();

private:
    enum class Kind : std::uint8_t { Start, Stop, Paint, Effect, Check };
    struct Request {
        Kind kind;
        QColor color;
        std::optional<domain::KeyboardEffect> effect;
    };
    // Copiável (exigência de QFutureWatcher::result): resultado e se a sessão continua aberta.
    struct Outcome {
        shared_kernel::Result<void> result;
        bool open;
        bool lost = false; // só Check: o firmware retomou o controle
    };

    void submit(Request request);
    void start(Request request);
    void onFinished();
    Outcome run(const Request& request);
    void finishCheck(const Outcome& outcome);
    void onCheckTimer();
    void syncTimer();

    const application::StartLiveKeyboardSession* start_;        // não nulo
    std::unique_ptr<application::LiveKeyboardSession> session_; // tocada só pela operação em andamento
    QFutureWatcher<Outcome> watcher_;
    QTimer checkTimer_;
    std::optional<Request> pending_;
    Kind runningKind_ = Kind::Start;
    QString status_;
    bool live_ = false;
    bool running_ = false;
};

} // namespace ghub::lighting::presentation
