#pragma once

#include "modules/audio_processing/api/AudioLevelSource.hpp"
#include "modules/lighting/application/AudioVisualizer.hpp"
#include "modules/lighting/application/LiveKeyboardSession.hpp"
#include "modules/lighting/application/RunSoftwareAnimation.hpp"
#include "shared_kernel/Clock.hpp"

#include <QColor>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <memory>

namespace ghub::lighting::presentation {

// Animações por software do G915 (INC-021 R2/R3, ADR-0030). Nada é aberto nem escrito na construção: start() só roda
// por clique do usuário e abre a sua PRÓPRIA sessão ao vivo (o mesmo lease do modo ao vivo: com o modo ao vivo ligado,
// ou com outro programa no teclado, a abertura falha com "ocupado" e nada é enviado). stop(), a perda de acesso, uma
// falha de envio, releaseForSleep() e o destrutor devolvem o controle ao firmware. A captura de áudio do visualizador
// só existe entre o início e o fim da animação "audio". Cada operação (abrir, quadro, fechar) roda numa thread de
// trabalho, uma por vez; o QTimer só dispara o próximo quadro quando o anterior terminou (o limite de taxa vem do caso
// de uso RunSoftwareAnimation com o relógio injetado). A cor nunca é registrada em log. Use sempre a partir da thread
// que possui o objeto.
class SoftwareAnimationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY controllerChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY controllerChanged)
    Q_PROPERTY(QString status READ status NOTIFY controllerChanged)
    Q_PROPERTY(int framesSent READ framesSent NOTIFY controllerChanged)
    Q_PROPERTY(int framesPerSecond READ framesPerSecond NOTIFY controllerChanged)

public:
    // `starSeed` alimenta a animação "stars" (injetada: testes determinísticos). Todas as referências precisam
    // sobreviver ao controlador.
    SoftwareAnimationController(const application::StartLiveKeyboardSession& start, const shared_kernel::Clock& clock,
                                audio_processing::AudioLevelSource& audio, std::uint32_t starSeed,
                                QObject* parent = nullptr);
    ~SoftwareAnimationController() override;

    SoftwareAnimationController(const SoftwareAnimationController&) = delete;
    SoftwareAnimationController& operator=(const SoftwareAnimationController&) = delete;
    SoftwareAnimationController(SoftwareAnimationController&&) = delete;
    SoftwareAnimationController& operator=(SoftwareAnimationController&&) = delete;

    [[nodiscard]] bool running() const { return running_; }
    [[nodiscard]] bool busy() const { return jobRunning_ && jobKind_ != Job::Tick; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] int framesSent() const { return framesSent_; }
    [[nodiscard]] int framesPerSecond() const { return framesPerSecond_; }

    // `kind`: cycle, wave, stars, keyframes ou audio. `color`: matiz inicial dos keyframes, cor das estrelas e das
    // barras; `color2`: segunda cor dos keyframes. `periodSeconds` (1..60): volta do ciclo/onda ou ida e volta dos
    // keyframes; `reverse`: onda da direita para a esquerda; `density` (0..100): estrelas; `framesPerSecond` (1..20,
    // padrão do caso de uso 10). Já em andamento, ocupado ou `kind` desconhecido: não abre nada.
    Q_INVOKABLE void start(const QString& kind, const QColor& color, const QColor& color2, int periodSeconds,
                           bool reverse, int density, int framesPerSecond);
    Q_INVOKABLE void stop();
    // Chamado da thread de UI antes da suspensão do sistema: síncrono, devolve o controle ao firmware.
    Q_INVOKABLE void releaseForSleep();

signals:
    void controllerChanged();

private:
    enum class Job : std::uint8_t { Start, Tick, Stop };
    struct Plan {
        bool audio = false;
        domain::RgbColor audioColor{0, 0, 0};
        application::RunSoftwareAnimation::FrameSource source;
        unsigned framesPerSecond = 10;
    };
    // Copiável (exigência de QFutureWatcher::result).
    struct Outcome {
        shared_kernel::Result<void> result;
        bool sent = false;
        bool ended = false; // a sessão foi encerrada pelo trabalho (falha ou perda do controle)
        bool lost = false;
    };

    void submit(Job kind);
    void submit(Job kind, Plan plan);
    Outcome run(Job kind, const Plan& plan);
    Outcome runStart(const Plan& plan);
    Outcome runTick();
    Outcome runStop();
    void onFinished();
    void onTimer();
    void releaseResources();

    const application::StartLiveKeyboardSession* start_; // não nulo
    const shared_kernel::Clock* clock_;                  // não nulo
    audio_processing::AudioLevelSource* audio_;          // não nulo
    std::uint32_t starSeed_;
    // Ordem de destruição (inversa): animação, sessão (devolve o controle), visualizador (fecha a captura).
    std::unique_ptr<application::AudioVisualizer> visualizer_;
    std::unique_ptr<application::LiveKeyboardSession> session_;
    std::unique_ptr<application::RunSoftwareAnimation> animation_;
    QFutureWatcher<Outcome> watcher_;
    QTimer timer_;
    QString status_;
    Job jobKind_ = Job::Start;
    int framesSent_ = 0;
    int framesPerSecond_ = application::RunSoftwareAnimation::kDefaultFramesPerSecond;
    int ticksSinceControlCheck_ = 0;
    bool running_ = false;
    bool jobRunning_ = false;
    bool stopRequested_ = false;
};

} // namespace ghub::lighting::presentation
