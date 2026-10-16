#include "modules/lighting/presentation/SoftwareAnimationController.hpp"

#include "modules/lighting/domain/AnimationFrames.hpp"

#include <QDebug>
#include <QtConcurrent>

#include <algorithm>
#include <utility>

namespace ghub::lighting::presentation {

namespace {

// A cada tantos quadros enviados a sessão consulta se o firmware retomou o controle (repouso do teclado, R15).
constexpr int kControlCheckEveryTicks = 20;

QString failureText(const shared_kernel::DomainError& error)
{
    switch (error.code) {
    case shared_kernel::ErrorCode::DeviceBusy:
        return SoftwareAnimationController::tr("teclado ocupado: ") + QString::fromStdString(error.detail);
    case shared_kernel::ErrorCode::BackendUnavailable:
        return SoftwareAnimationController::tr("indisponível: teclado ou áudio (desligado, dormindo ou sem permissão)");
    case shared_kernel::ErrorCode::LeaseExpired:
    case shared_kernel::ErrorCode::LeaseRevoked:
        return SoftwareAnimationController::tr("animação encerrada: acesso ao teclado perdido");
    case shared_kernel::ErrorCode::InvalidArgument:
    case shared_kernel::ErrorCode::MalformedData:
        break;
    }
    return SoftwareAnimationController::tr("falha ao falar com o teclado");
}

domain::RgbColor toRgb(const QColor& color)
{
    return {static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
            static_cast<std::uint8_t>(color.blue())};
}

} // namespace

SoftwareAnimationController::SoftwareAnimationController(const application::StartLiveKeyboardSession& start,
                                                         const shared_kernel::Clock& clock,
                                                         audio_processing::AudioLevelSource& audio,
                                                         std::uint32_t starSeed, QObject* parent)
    : QObject(parent), start_(&start), clock_(&clock), audio_(&audio), starSeed_(starSeed)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &SoftwareAnimationController::onFinished);
    connect(&timer_, &QTimer::timeout, this, &SoftwareAnimationController::onTimer);
}

SoftwareAnimationController::~SoftwareAnimationController()
{
    timer_.stop();
    watcher_.disconnect(this);
    if (jobRunning_) {
        watcher_.waitForFinished();
    }
    releaseResources(); // devolve o controle ao firmware e fecha a captura de áudio (RAII)
}

void SoftwareAnimationController::start(const QString& kind, const QColor& color, const QColor& color2,
                                        int periodSeconds, bool reverse, int density, int framesPerSecond)
{
    if (running_ || jobRunning_) {
        return;
    }
    const auto period = static_cast<std::uint32_t>(std::clamp(periodSeconds, 1, 60)) * 1000U;
    const auto rgb = toRgb(color);
    Plan plan;
    plan.framesPerSecond = static_cast<unsigned>(
        std::clamp(framesPerSecond, 1, static_cast<int>(application::RunSoftwareAnimation::kMaxFramesPerSecond)));
    if (kind == QLatin1String("cycle")) {
        const domain::AnimationSpec spec{domain::ColorCycleParams{.periodMs = period}};
        plan.source = [spec](std::int64_t t) { return domain::animationFrame(t, spec); };
    } else if (kind == QLatin1String("wave")) {
        const domain::AnimationSpec spec{domain::WaveParams{.periodMs = period,
                                                            .direction = reverse ? domain::WaveDirection::RightToLeft
                                                                                 : domain::WaveDirection::LeftToRight}};
        plan.source = [spec](std::int64_t t) { return domain::animationFrame(t, spec); };
    } else if (kind == QLatin1String("stars")) {
        const domain::AnimationSpec spec{domain::StarParams{
            .densityPercent = static_cast<std::uint8_t>(std::clamp(density, 0, 100)), .color = rgb, .seed = starSeed_}};
        plan.source = [spec](std::int64_t t) { return domain::animationFrame(t, spec); };
    } else if (kind == QLatin1String("keyframes")) {
        auto track = domain::KeyframeTrack::create(
            {{0, rgb}, {period / 2, toRgb(color2)}, {period, rgb}}); // ida e volta; period >= 1000
        if (!track.has_value()) {
            status_ = failureText(track.error());
            emit controllerChanged();
            return;
        }
        const domain::AnimationSpec spec{*std::move(track)};
        plan.source = [spec](std::int64_t t) { return domain::animationFrame(t, spec); };
    } else if (kind == QLatin1String("audio")) {
        plan.audio = true;
        plan.audioColor = rgb;
    } else {
        status_ = tr("animação desconhecida");
        emit controllerChanged();
        return;
    }
    framesPerSecond_ = static_cast<int>(plan.framesPerSecond);
    framesSent_ = 0;
    stopRequested_ = false;
    submit(Job::Start, std::move(plan));
}

void SoftwareAnimationController::stop()
{
    if (jobRunning_) {
        stopRequested_ = true; // roda quando a operação atual terminar
        return;
    }
    if (running_) {
        submit(Job::Stop);
    }
}

void SoftwareAnimationController::releaseForSleep()
{
    timer_.stop();
    stopRequested_ = false;
    if (jobRunning_) {
        watcher_.disconnect(this);
        watcher_.waitForFinished();
        QCoreApplication::sendPostedEvents(&watcher_);
        connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &SoftwareAnimationController::onFinished);
        jobRunning_ = false;
    }
    const bool had = running_ || session_ != nullptr;
    releaseResources();
    running_ = false;
    if (had) {
        qInfo("lighting: animação encerrada antes da suspensão do sistema");
        status_ = tr("animação encerrada: o sistema vai suspender");
        emit controllerChanged();
    }
}

void SoftwareAnimationController::releaseResources()
{
    animation_.reset();
    session_.reset(); // devolve o controle ao firmware
    visualizer_.reset();
}

void SoftwareAnimationController::submit(Job kind)
{
    submit(kind, Plan{});
}

void SoftwareAnimationController::submit(Job kind, Plan plan)
{
    jobRunning_ = true;
    jobKind_ = kind;
    watcher_.setFuture(QtConcurrent::run([this, kind, plan = std::move(plan)] { return run(kind, plan); }));
    if (kind != Job::Tick) {
        emit controllerChanged();
    }
}

SoftwareAnimationController::Outcome SoftwareAnimationController::run(Job kind, const Plan& plan)
{
    switch (kind) {
    case Job::Start:
        return runStart(plan);
    case Job::Tick:
        return runTick();
    case Job::Stop:
        break;
    }
    return runStop();
}

// Roda na thread de trabalho; é a única que toca os recursos enquanto `jobRunning_`.
SoftwareAnimationController::Outcome SoftwareAnimationController::runStart(const Plan& plan)
{
    if (plan.audio) {
        visualizer_ = std::make_unique<application::AudioVisualizer>(*audio_, plan.audioColor);
        if (auto started = visualizer_->start(); !started.has_value()) {
            visualizer_.reset();
            return {.result = std::unexpected(started.error())};
        }
    }
    auto started = start_->start();
    if (!started.has_value()) {
        visualizer_.reset(); // fecha a captura de áudio
        return {.result = std::unexpected(started.error())};
    }
    session_ = std::move(*started);
    animation_ = std::make_unique<application::RunSoftwareAnimation>(
        *session_, *clock_, plan.audio ? visualizer_->frameSource() : plan.source, plan.framesPerSecond);
    ticksSinceControlCheck_ = 0;
    return {.result = {}};
}

SoftwareAnimationController::Outcome SoftwareAnimationController::runTick()
{
    if (!animation_) {
        return {.result = {}, .ended = true};
    }
    if (++ticksSinceControlCheck_ >= kControlCheckEveryTicks) {
        ticksSinceControlCheck_ = 0;
        const auto lost = session_->checkControl();
        if (lost.has_value() && *lost) {
            releaseResources(); // sessão já encerrada por checkControl, sem enviar nada
            return {.result = {}, .ended = true, .lost = true};
        }
    }
    const auto ticked = animation_->tick();
    if (!ticked.has_value()) {
        releaseResources(); // uma falha já fechou a sessão sozinha
        return {.result = std::unexpected(ticked.error()), .ended = true};
    }
    return {.result = {}, .sent = *ticked == application::TickOutcome::Sent};
}

SoftwareAnimationController::Outcome SoftwareAnimationController::runStop()
{
    shared_kernel::Result<void> ended;
    if (animation_) {
        ended = animation_->stop();
    }
    releaseResources();
    return {.result = std::move(ended), .ended = true};
}

void SoftwareAnimationController::onFinished()
{
    const auto outcome = watcher_.result();
    jobRunning_ = false;
    switch (jobKind_) {
    case Job::Start:
        if (outcome.result.has_value()) {
            running_ = true;
            status_ = tr("animação em andamento: o aplicativo controla as cores");
            qInfo("lighting: animação por software iniciada");
            timer_.start(1000 / framesPerSecond_);
        } else {
            const auto& error = outcome.result.error();
            qWarning("lighting: não foi possível iniciar a animação (código %d): %s", static_cast<int>(error.code),
                     error.detail.c_str());
            status_ = failureText(error);
            stopRequested_ = false;
        }
        break;
    case Job::Tick:
        if (outcome.sent) {
            ++framesSent_;
        }
        if (outcome.ended) {
            running_ = false;
            timer_.stop();
            if (outcome.lost) {
                status_ = tr("o teclado entrou em repouso e retomou o efeito salvo; inicie a animação de novo");
            } else if (!outcome.result.has_value()) {
                const auto& error = outcome.result.error();
                qWarning("lighting: animação falhou (código %d): %s", static_cast<int>(error.code),
                         error.detail.c_str());
                status_ = failureText(error);
            }
            stopRequested_ = false;
        }
        break;
    case Job::Stop:
        running_ = false;
        timer_.stop();
        stopRequested_ = false;
        if (outcome.result.has_value()) {
            status_ = tr("animação parada: o efeito salvo no teclado voltou");
            qInfo("lighting: animação por software parada");
        } else {
            qWarning("lighting: falha ao devolver o controle (código %d): %s",
                     static_cast<int>(outcome.result.error().code), outcome.result.error().detail.c_str());
            status_ = failureText(outcome.result.error());
        }
        break;
    }
    if (stopRequested_ && running_) {
        stopRequested_ = false;
        submit(Job::Stop);
        return;
    }
    emit controllerChanged();
}

void SoftwareAnimationController::onTimer()
{
    if (running_ && !jobRunning_ && !stopRequested_) {
        submit(Job::Tick);
    }
}

} // namespace ghub::lighting::presentation
