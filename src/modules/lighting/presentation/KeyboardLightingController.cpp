#include "modules/lighting/presentation/KeyboardLightingController.hpp"

#include "modules/lighting/domain/EffectParameters.hpp"
#include "modules/lighting/domain/G915KeyCatalog.hpp"
#include "modules/lighting/domain/KeyColorMap.hpp"
#include "modules/lighting/domain/KeyboardZone.hpp"
#include "modules/lighting/domain/RgbColor.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QtConcurrent>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace ghub::lighting::presentation {

namespace {

QString noSessionText()
{
    return KeyboardLightingController::tr("ligue o modo ao vivo antes de escolher uma cor ou efeito");
}

QString failureText(const shared_kernel::DomainError& error)
{
    switch (error.code) {
    case shared_kernel::ErrorCode::DeviceBusy:
        // O detalhe traz o nome do processo que disputa o teclado (R6).
        return KeyboardLightingController::tr("teclado ocupado: ") + QString::fromStdString(error.detail);
    case shared_kernel::ErrorCode::BackendUnavailable:
        return KeyboardLightingController::tr("teclado indisponível (desligado, dormindo ou sem permissão)");
    case shared_kernel::ErrorCode::LeaseExpired:
    case shared_kernel::ErrorCode::LeaseRevoked:
        return KeyboardLightingController::tr("modo ao vivo encerrado: acesso ao teclado perdido");
    case shared_kernel::ErrorCode::InvalidArgument:
    case shared_kernel::ErrorCode::MalformedData:
        break;
    }
    return KeyboardLightingController::tr("falha ao falar com o teclado");
}

// Todas as teclas do catálogo na mesma cor.
domain::KeyColorMap wholeKeyboard(const QColor& color)
{
    const domain::RgbColor rgb{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                               static_cast<std::uint8_t>(color.blue())};
    domain::KeyColorMap map;
    for (const auto& key : domain::G915KeyCatalog::keys()) {
        map = map.with(key.id, rgb);
    }
    return map;
}

// Parâmetros da prévia (R14; brilho do R1 do INC-019, já limitado a 1..100); nulo para `kind` desconhecido.
std::optional<domain::KeyboardEffect> previewEffect(const QString& kind, const QColor& color, int brightnessPercent)
{
    // Constantes dentro das faixas válidas: create() não falha.
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    const auto period = *domain::EffectPeriod::create(5000);
    const auto brightness = *domain::EffectBrightness::create(static_cast<std::uint32_t>(brightnessPercent));
    // NOLINTEND(bugprone-unchecked-optional-access)
    const domain::RgbColor rgb{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                               static_cast<std::uint8_t>(color.blue())};
    if (kind == QLatin1String("wave")) {
        return domain::KeyboardEffect::wave(period, brightness);
    }
    if (kind == QLatin1String("cycle")) {
        return domain::KeyboardEffect::cycle(period, brightness);
    }
    if (kind == QLatin1String("breathing")) {
        return domain::KeyboardEffect::breathing(rgb, period, brightness);
    }
    if (kind == QLatin1String("ripple")) {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return domain::KeyboardEffect::ripple(rgb, *domain::RippleSpeed::create(20));
    }
    if (kind == QLatin1String("static")) {
        return domain::KeyboardEffect::staticColor(rgb);
    }
    return std::nullopt;
}

} // namespace

KeyboardLightingController::KeyboardLightingController(const application::StartLiveKeyboardSession& start,
                                                       std::chrono::milliseconds controlCheckInterval, QObject* parent)
    : QObject(parent), start_(&start)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &KeyboardLightingController::onFinished);
    checkTimer_.setInterval(controlCheckInterval);
    connect(&checkTimer_, &QTimer::timeout, this, &KeyboardLightingController::onCheckTimer);
}

KeyboardLightingController::~KeyboardLightingController()
{
    // As operações não são canceláveis: espera a atual terminar, pois ela usa a sessão e o caso de uso.
    checkTimer_.stop();
    pending_.reset();
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
    // Fechar a sessão aberta devolve o controle ao firmware (RAII).
    session_.reset();
}

void KeyboardLightingController::startLive()
{
    if (!running_ && live_) {
        return;
    }
    submit({.kind = Kind::Start, .color = {}, .effect = std::nullopt});
}

void KeyboardLightingController::stopLive()
{
    if (!running_ && !live_) {
        return;
    }
    submit({.kind = Kind::Stop, .color = {}, .effect = std::nullopt});
}

void KeyboardLightingController::paintAll(const QColor& color)
{
    if (!running_ && !live_) {
        status_ = noSessionText();
        emit controllerChanged();
        return;
    }
    submit({.kind = Kind::Paint, .color = color, .effect = std::nullopt});
}

void KeyboardLightingController::setBrightness(int percent)
{
    const int clamped = std::clamp(percent, static_cast<int>(domain::EffectBrightness::kMinPercent),
                                   static_cast<int>(domain::EffectBrightness::kMaxPercent));
    if (clamped == brightness_) {
        return;
    }
    brightness_ = clamped;
    emit brightnessChanged();
}

void KeyboardLightingController::applyEffect(const QString& kind, const QColor& color)
{
    const auto effect = previewEffect(kind, color, brightness_);
    if (!effect.has_value()) {
        status_ = tr("efeito desconhecido");
        emit controllerChanged();
        return;
    }
    if (!running_ && !live_) {
        status_ = noSessionText();
        emit controllerChanged();
        return;
    }
    submit({.kind = Kind::Effect,
            .color = {},
            .effect = effect}); // a cor já está dentro do efeito (R12: não é registrada)
}

void KeyboardLightingController::releaseForSleep()
{
    checkTimer_.stop();
    pending_.reset();
    if (running_) {
        // Desconecta para a conclusão não rodar onFinished (que mexeria no estado); descarta o evento já postado.
        watcher_.disconnect(this);
        watcher_.waitForFinished();
        QCoreApplication::sendPostedEvents(&watcher_);
        connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &KeyboardLightingController::onFinished);
        running_ = false;
    }
    if (!session_) {
        if (live_) {
            live_ = false;
            emit controllerChanged();
        }
        return;
    }
    // Devolve o controle ao firmware (uma tentativa); falha é só registrada.
    if (const auto ended = session_->end(); !ended.has_value()) {
        qWarning("lighting: falha ao devolver o controle antes da suspensão (código %d): %s",
                 static_cast<int>(ended.error().code), ended.error().detail.c_str());
    } else {
        qInfo("lighting: controle devolvido antes da suspensão do sistema");
    }
    session_.reset();
    live_ = false;
    status_ = tr("modo ao vivo desligado: o sistema vai suspender");
    emit controllerChanged();
}

void KeyboardLightingController::onCheckTimer()
{
    // Nunca substitui um pedido do usuário nem roda junto com outra operação.
    if (live_ && !running_ && !pending_.has_value()) {
        submit({.kind = Kind::Check, .color = {}, .effect = std::nullopt});
    }
}

void KeyboardLightingController::syncTimer()
{
    if (live_ && !checkTimer_.isActive()) {
        checkTimer_.start();
    } else if (!live_ && checkTimer_.isActive()) {
        checkTimer_.stop();
    }
}

void KeyboardLightingController::submit(Request request)
{
    if (running_) {
        pending_ = request; // só o último pedido vale
        return;
    }
    start(request);
}

void KeyboardLightingController::start(Request request)
{
    running_ = true;
    runningKind_ = request.kind;
    watcher_.setFuture(QtConcurrent::run([this, request] { return run(request); }));
    if (request.kind != Kind::Check) {
        emit controllerChanged();
    }
}

// Roda na thread de trabalho; é a única que toca `session_` enquanto `running_`.
KeyboardLightingController::Outcome KeyboardLightingController::run(const Request& request)
{
    switch (request.kind) {
    case Kind::Start: {
        if (session_) {
            return {.result = {}, .open = true};
        }
        auto started = start_->start();
        if (!started.has_value()) {
            return {.result = std::unexpected(started.error()), .open = false};
        }
        session_ = std::move(*started);
        return {.result = {}, .open = true};
    }
    case Kind::Stop: {
        if (!session_) {
            return {.result = {}, .open = false};
        }
        auto ended = session_->end();
        session_.reset();
        return {.result = std::move(ended), .open = false};
    }
    case Kind::Check: {
        if (!session_) {
            return {.result = {}, .open = false};
        }
        const auto lost = session_->checkControl();
        if (!lost.has_value()) {
            return {.result = std::unexpected(lost.error()), .open = session_->isOpen()};
        }
        if (*lost) {
            session_.reset(); // já encerrada por checkControl, sem enviar nada
        }
        return {.result = {}, .open = !*lost, .lost = *lost};
    }
    case Kind::Paint:
    case Kind::Effect:
        break;
    }
    if (!session_) {
        auto missing = shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "sem sessão aberta");
        return {.result = std::move(missing), .open = false};
    }
    shared_kernel::Result<void> painted;
    if (request.kind == Kind::Effect && request.effect.has_value()) {
        // R14: o logo acompanha as teclas; só é enviado se as teclas deram certo.
        painted = session_->applyEffect(domain::KeyboardZone::Keys, *request.effect);
        if (painted.has_value()) {
            painted = session_->applyEffect(domain::KeyboardZone::Logo, domain::logoCompanion(*request.effect));
        }
    } else {
        painted = session_->paint(wholeKeyboard(request.color));
    }
    // Uma falha fecha a sessão sozinha (devolvendo o controle); aqui só se descarta o objeto.
    const bool open = session_->isOpen();
    if (!open) {
        session_.reset();
    }
    return {.result = std::move(painted), .open = open};
}

void KeyboardLightingController::onFinished()
{
    const auto outcome = watcher_.result();
    live_ = outcome.open;
    if (runningKind_ == Kind::Check) {
        finishCheck(outcome);
        return;
    }
    if (outcome.result.has_value()) {
        switch (runningKind_) {
        case Kind::Start:
            status_ = tr("modo ao vivo ligado: o aplicativo controla as cores");
            qInfo("lighting: modo ao vivo ligado");
            break;
        case Kind::Paint:
            status_ = tr("cor aplicada");
            qInfo("lighting: cor aplicada");
            break;
        case Kind::Effect:
            status_ = tr("efeito aplicado (prévia: some ao desligar o modo ao vivo)");
            qInfo("lighting: efeito aplicado");
            break;
        case Kind::Stop:
            status_ = tr("modo ao vivo desligado: o efeito salvo no teclado voltou");
            qInfo("lighting: modo ao vivo desligado");
            break;
        case Kind::Check:
            break; // tratado em finishCheck
        }
    } else {
        const auto& error = outcome.result.error();
        // Nunca registra a cor (R12).
        qWarning("lighting: operação do modo ao vivo falhou (código %d): %s", static_cast<int>(error.code),
                 error.detail.c_str());
        const bool noSession = (runningKind_ == Kind::Paint || runningKind_ == Kind::Effect) &&
                               error.code == shared_kernel::ErrorCode::InvalidArgument;
        status_ = noSession ? noSessionText() : failureText(error);
    }
    running_ = false;
    syncTimer();
    if (const auto next = std::exchange(pending_, std::nullopt); next.has_value()) {
        start(*next);
    }
    emit controllerChanged();
}

void KeyboardLightingController::finishCheck(const Outcome& outcome)
{
    running_ = false;
    syncTimer();
    if (!outcome.result.has_value()) {
        const auto& error = outcome.result.error();
        qWarning("lighting: sondagem do controle falhou (código %d): %s", static_cast<int>(error.code),
                 error.detail.c_str());
    } else if (outcome.lost) {
        qInfo("lighting: o teclado retomou o controle (repouso)");
        status_ = tr("o teclado entrou em repouso e retomou o efeito salvo; ligue o modo ao vivo de novo");
    }
    if (const auto next = std::exchange(pending_, std::nullopt); next.has_value()) {
        start(*next);
        return;
    }
    if (outcome.lost || !outcome.result.has_value()) {
        emit controllerChanged();
    }
}

} // namespace ghub::lighting::presentation
