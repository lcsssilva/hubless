#pragma once

#include "modules/lighting/application/ControlKeyboard.hpp"
#include "modules/lighting/domain/GameModeKeys.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstdint>
#include <optional>
#include <vector>

namespace ghub::lighting::presentation {

// Modo de jogo e troca de slot onboard do G915 para a janela (INC-019 R2/R3/R6, ADR-0029). Nada é aberto nem enviado
// na construção nem ao reconectar: activateGameMode(), deactivateGameMode(), refreshSlots() e selectSlot() só rodam
// por clique. Escolher teclas só altera a seleção local (nada vai ao teclado). Uma operação por vez, numa thread de
// trabalho; pedido feito durante outra operação é ignorado (sem fila, sem retentativa). Falhas são registradas só
// com código e detalhe. O caso de uso deve sobreviver a este objeto; o destrutor espera a operação em andamento.
class KeyboardControlsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY controllerChanged)
    Q_PROPERTY(QString status READ status NOTIFY controllerChanged)
    // Teclas escolhidas para o modo de jogo (nomes do catálogo sem o prefixo "Key: ") e as que ainda podem ser
    // adicionadas.
    Q_PROPERTY(QStringList gameModeKeys READ gameModeKeys NOTIFY controllerChanged)
    Q_PROPERTY(QStringList gameModeChoices READ gameModeChoices NOTIFY controllerChanged)
    // Slots M1..M3 habilitados (1..3) e o ativo (0 = desconhecido); só preenchidos após refreshSlots()/selectSlot().
    Q_PROPERTY(QVariantList enabledSlots READ enabledSlots NOTIFY controllerChanged)
    Q_PROPERTY(int activeSlot READ activeSlot NOTIFY controllerChanged)

public:
    explicit KeyboardControlsController(const application::ControlKeyboard& control, QObject* parent = nullptr);
    ~KeyboardControlsController() override;

    KeyboardControlsController(const KeyboardControlsController&) = delete;
    KeyboardControlsController& operator=(const KeyboardControlsController&) = delete;
    KeyboardControlsController(KeyboardControlsController&&) = delete;
    KeyboardControlsController& operator=(KeyboardControlsController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QStringList gameModeKeys() const;
    [[nodiscard]] QStringList gameModeChoices() const;
    [[nodiscard]] QVariantList enabledSlots() const;
    [[nodiscard]] int activeSlot() const { return activeSlot_; }

    // Seleção local; não escreve no teclado. Nome desconhecido, repetido ou além de 16 teclas: ignorado (o excesso
    // informa o limite no status).
    Q_INVOKABLE void addGameKey(const QString& name);
    Q_INVOKABLE void removeGameKey(const QString& name);
    Q_INVOKABLE void resetGameKeys();

    Q_INVOKABLE void activateGameMode();
    Q_INVOKABLE void deactivateGameMode();
    Q_INVOKABLE void refreshSlots();
    Q_INVOKABLE void selectSlot(int number);

signals:
    void controllerChanged();

private:
    enum class Kind : std::uint8_t { Activate, Deactivate, Refresh, Select };
    // Copiável (exigência de QFutureWatcher::result).
    struct Outcome {
        shared_kernel::Result<void> result;
        std::optional<domain::OnboardSlots> listed; // só Refresh
    };

    void start(Kind kind, std::optional<domain::OnboardSlot> slot = std::nullopt);
    void onFinished();

    const application::ControlKeyboard* control_; // não nulo
    QFutureWatcher<Outcome> watcher_;
    std::vector<domain::KeyId> selected_;
    std::optional<domain::OnboardSlot> pendingSlot_; // slot da troca em andamento
    domain::OnboardSlots slots_;
    QString status_;
    Kind runningKind_ = Kind::Refresh;
    int activeSlot_ = 0;
    bool running_ = false;
};

} // namespace ghub::lighting::presentation
