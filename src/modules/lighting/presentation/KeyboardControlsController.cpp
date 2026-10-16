#include "modules/lighting/presentation/KeyboardControlsController.hpp"

#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <QDebug>
#include <QtConcurrent>

#include <algorithm>
#include <string_view>
#include <utility>

namespace ghub::lighting::presentation {

namespace {

constexpr std::string_view keyPrefix = "Key: ";

// Nome mostrado ao usuário: o do catálogo sem o prefixo "Key: ".
QString displayName(const domain::KeyDefinition& key)
{
    std::string_view name = key.name;
    if (name.starts_with(keyPrefix)) {
        name.remove_prefix(keyPrefix.size());
    }
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

std::optional<domain::KeyId> findEligible(const QString& name)
{
    for (const auto id : domain::GameModeKeys::eligibleKeys()) {
        if (displayName(domain::G915KeyCatalog::definition(id)) == name) {
            return id;
        }
    }
    return std::nullopt;
}

std::vector<domain::KeyId> suggestedKeys()
{
    std::vector<domain::KeyId> keys;
    for (const auto code : domain::GameModeKeys::suggested().hidCodes()) {
        for (const auto id : domain::GameModeKeys::eligibleKeys()) {
            if (domain::G915KeyCatalog::definition(id).index == code) {
                keys.push_back(id);
                break;
            }
        }
    }
    return keys;
}

QString failureText(const shared_kernel::DomainError& error)
{
    switch (error.code) {
    case shared_kernel::ErrorCode::DeviceBusy:
        // O detalhe traz o nome do processo ou do módulo que disputa o teclado.
        return KeyboardControlsController::tr("teclado ocupado: ") + QString::fromStdString(error.detail);
    case shared_kernel::ErrorCode::BackendUnavailable:
        return KeyboardControlsController::tr("teclado indisponível (desligado, dormindo ou sem permissão)");
    case shared_kernel::ErrorCode::LeaseExpired:
    case shared_kernel::ErrorCode::LeaseRevoked:
        return KeyboardControlsController::tr("acesso ao teclado perdido");
    case shared_kernel::ErrorCode::InvalidArgument:
        return KeyboardControlsController::tr("pedido recusado: ") + QString::fromStdString(error.detail);
    case shared_kernel::ErrorCode::MalformedData:
        break;
    }
    return KeyboardControlsController::tr("falha ao falar com o teclado");
}

} // namespace

KeyboardControlsController::KeyboardControlsController(const application::ControlKeyboard& control, QObject* parent)
    : QObject(parent), control_(&control), selected_(suggestedKeys())
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &KeyboardControlsController::onFinished);
}

KeyboardControlsController::~KeyboardControlsController()
{
    // A operação não é cancelável: espera a atual terminar, pois ela usa o caso de uso.
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

QStringList KeyboardControlsController::gameModeKeys() const
{
    QStringList names;
    for (const auto id : selected_) {
        names.push_back(displayName(domain::G915KeyCatalog::definition(id)));
    }
    return names;
}

QStringList KeyboardControlsController::gameModeChoices() const
{
    QStringList names;
    for (const auto id : domain::GameModeKeys::eligibleKeys()) {
        if (std::ranges::find(selected_, id) == selected_.end()) {
            names.push_back(displayName(domain::G915KeyCatalog::definition(id)));
        }
    }
    return names;
}

QVariantList KeyboardControlsController::enabledSlots() const
{
    QVariantList list;
    for (const auto slot : slots_.enabled) {
        list.push_back(static_cast<int>(slot.number()));
    }
    return list;
}

void KeyboardControlsController::addGameKey(const QString& name)
{
    const auto id = findEligible(name);
    if (!id.has_value() || std::ranges::find(selected_, *id) != selected_.end()) {
        return;
    }
    if (selected_.size() >= domain::GameModeKeys::kMaxKeys) {
        status_ = tr("no máximo %1 teclas no modo de jogo").arg(domain::GameModeKeys::kMaxKeys);
        emit controllerChanged();
        return;
    }
    selected_.push_back(*id);
    emit controllerChanged();
}

void KeyboardControlsController::removeGameKey(const QString& name)
{
    const auto id = findEligible(name);
    if (!id.has_value()) {
        return;
    }
    if (const auto found = std::ranges::find(selected_, *id); found != selected_.end()) {
        selected_.erase(found);
        emit controllerChanged();
    }
}

void KeyboardControlsController::resetGameKeys()
{
    selected_ = suggestedKeys();
    emit controllerChanged();
}

void KeyboardControlsController::activateGameMode()
{
    start(Kind::Activate);
}

void KeyboardControlsController::deactivateGameMode()
{
    start(Kind::Deactivate);
}

void KeyboardControlsController::refreshSlots()
{
    start(Kind::Refresh);
}

void KeyboardControlsController::selectSlot(int number)
{
    const auto slot = number >= 0 && number <= 255
                          ? domain::OnboardSlot::create(static_cast<std::uint8_t>(number))
                          : shared_kernel::Result<domain::OnboardSlot>{std::unexpected(shared_kernel::DomainError{
                                .code = shared_kernel::ErrorCode::InvalidArgument, .detail = "slot fora da faixa"})};
    if (!slot.has_value()) {
        status_ = tr("slot inválido (use M1, M2 ou M3)");
        emit controllerChanged();
        return;
    }
    start(Kind::Select, *slot);
}

void KeyboardControlsController::start(Kind kind, std::optional<domain::OnboardSlot> slot)
{
    if (running_) {
        return; // sem fila: o clique durante outra operação é descartado
    }
    running_ = true;
    runningKind_ = kind;
    pendingSlot_ = slot;
    const auto* control = control_;
    // A lista de teclas é copiada na thread de UI antes de a operação sair dela.
    const auto keys = domain::GameModeKeys::create(selected_);
    watcher_.setFuture(QtConcurrent::run([control, kind, slot, keys]() -> Outcome {
        switch (kind) {
        case Kind::Activate:
            if (!keys.has_value()) {
                return {.result = std::unexpected(keys.error()), .listed = std::nullopt};
            }
            return {.result = control->setGameMode(*keys), .listed = std::nullopt};
        case Kind::Deactivate: {
            const auto none = domain::GameModeKeys::create({});
            return {.result = control->setGameMode(*none),
                    .listed = std::nullopt}; // NOLINT(bugprone-unchecked-optional-access)
        }
        case Kind::Refresh: {
            auto listed = control->listSlots();
            if (!listed.has_value()) {
                return {.result = std::unexpected(listed.error()), .listed = std::nullopt};
            }
            return {.result = {}, .listed = std::move(*listed)};
        }
        case Kind::Select:
            return {.result = control->selectSlot(*slot),
                    .listed = std::nullopt}; // NOLINT(bugprone-unchecked-optional-access)
        }
        return {.result = {}, .listed = std::nullopt};
    }));
    emit controllerChanged();
}

void KeyboardControlsController::onFinished()
{
    const auto outcome = watcher_.result();
    running_ = false;
    if (outcome.result.has_value()) {
        switch (runningKind_) {
        case Kind::Activate:
            status_ = tr("modo de jogo ativado (volátil até o teste físico mostrar o contrário)");
            qInfo("lighting: modo de jogo ativado");
            break;
        case Kind::Deactivate:
            status_ = tr("modo de jogo desativado: todas as teclas reativadas");
            qInfo("lighting: modo de jogo desativado");
            break;
        case Kind::Refresh:
            if (outcome.listed.has_value()) {
                slots_ = *outcome.listed;
                activeSlot_ = slots_.active.has_value() ? static_cast<int>(slots_.active->number()) : 0;
            }
            status_ = tr("slots lidos do teclado");
            qInfo("lighting: slots onboard lidos");
            break;
        case Kind::Select:
            if (pendingSlot_.has_value()) {
                activeSlot_ = static_cast<int>(pendingSlot_->number());
                status_ = tr("slot M%1 selecionado").arg(activeSlot_);
            }
            qInfo("lighting: slot onboard trocado");
            break;
        }
    } else {
        const auto& error = outcome.result.error();
        qWarning("lighting: controle do teclado falhou (código %d): %s", static_cast<int>(error.code),
                 error.detail.c_str());
        status_ = failureText(error);
    }
    pendingSlot_.reset();
    emit controllerChanged();
}

} // namespace ghub::lighting::presentation
