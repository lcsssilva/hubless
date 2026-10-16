#pragma once

#include "modules/headset/application/ChangeHeadsetLighting.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace ghub::headset {

// Aplica cor/efeito de uma zona do G733 por ação do usuário, fora da thread de UI (INC-018 R1). Nada é executado
// na construção. Uma operação por vez: um pedido feito durante uma operação substitui o pendente (só o último vale).
// Sem retentativa. O destrutor espera a operação em andamento. O caso de uso deve sobreviver a este objeto.
// A janela mostra que o efeito é volátil e que a cor é enviada sem correção.
class HeadsetLightingController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY lightingChanged)
    Q_PROPERTY(QString status READ status NOTIFY lightingChanged)

public:
    explicit HeadsetLightingController(const ChangeHeadsetLighting& changeHeadsetLighting, QObject* parent = nullptr);
    ~HeadsetLightingController() override;

    HeadsetLightingController(const HeadsetLightingController&) = delete;
    HeadsetLightingController& operator=(const HeadsetLightingController&) = delete;
    HeadsetLightingController(HeadsetLightingController&&) = delete;
    HeadsetLightingController& operator=(HeadsetLightingController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QString status() const { return status_; }

    // INC-017 parte 2: avisado na thread dona, só depois de uma zona aplicada com sucesso, com os valores pedidos
    // (zona, efeito, r, g, b, período, brilho).
    using AppliedObserver = std::function<void(int, int, int, int, int, int, int)>;
    void setAppliedObserver(AppliedObserver observer) { observer_ = std::move(observer); }
    // zone: 0 inferior, 1 superior; effect: 0 desligado, 1 fixo, 2 respiração, 3 ciclo; red/green/blue 0-255;
    // periodMs 1000-20000 em passos de 100 e brightness 1-100 (respiração e ciclo). Valor inválido: status de
    // falha, nada é executado.
    Q_INVOKABLE void apply(int zone, int effect, int red, int green, int blue, int periodMs, int brightness);

signals:
    void lightingChanged();

private:
    struct Values {
        int zone, effect, red, green, blue, periodMs, brightness;
    };
    using Outcome = shared_kernel::Result<HeadsetLighting>;

    void start(HeadsetLighting lighting, Values values);
    void onFinished();

    const ChangeHeadsetLighting* changeHeadsetLighting_; // não nulo
    QFutureWatcher<Outcome> watcher_;
    AppliedObserver observer_;
    std::optional<Values> runningValues_;
    std::optional<Values> pendingValues_;
    std::optional<HeadsetLighting> pending_;
    QString status_;
    bool running_ = false;
};

} // namespace ghub::headset
