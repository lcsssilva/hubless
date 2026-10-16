#pragma once

#include "modules/headset/application/ChangeHeadsetSetting.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace ghub::headset {

// Aplica sidetone e luzes do headset por ação do usuário, fora da thread de UI (ADR-0015, INC-009 R8).
// Nada é executado na construção. Uma operação por vez: um pedido feito durante uma operação substitui o
// pendente (só o último vale) e roda quando a atual termina. Sem retentativa. O destrutor espera a operação em
// andamento e descarta o pendente. O caso de uso deve sobreviver a este objeto. Use sempre da thread dona.
//
// `lastSidetone` e `lightsState` são só "o último valor aplicado nesta sessão" (o headset não informa o valor
// atual): -1 / "" quando nada foi aplicado; uma falha não os altera.
class HeadsetSettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY settingsChanged)
    Q_PROPERTY(QString status READ status NOTIFY settingsChanged)
    Q_PROPERTY(int lastSidetone READ lastSidetone NOTIFY settingsChanged)
    Q_PROPERTY(QString lightsState READ lightsState NOTIFY settingsChanged)

public:
    explicit HeadsetSettingsController(const ChangeHeadsetSetting& changeHeadsetSetting, QObject* parent = nullptr);
    ~HeadsetSettingsController() override;

    HeadsetSettingsController(const HeadsetSettingsController&) = delete;
    HeadsetSettingsController& operator=(const HeadsetSettingsController&) = delete;
    HeadsetSettingsController(HeadsetSettingsController&&) = delete;
    HeadsetSettingsController& operator=(HeadsetSettingsController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] int lastSidetone() const { return lastSidetone_; }
    [[nodiscard]] QString lightsState() const { return lightsState_; }

    // INC-017 parte 2: avisado na thread dona, só depois de um sidetone aplicado com sucesso (o composition root
    // liga isto ao "último valor aplicado" que o perfil captura).
    void setSidetoneObserver(std::function<void(int)> observer) { sidetoneObserver_ = std::move(observer); }
    // Valor fora de 0-100: status de falha, nada é executado.
    Q_INVOKABLE void setSidetone(int level);
    // true = ligar (efeito ciclo, período 5000 ms, brilho 100, nas duas zonas), false = desligar.
    Q_INVOKABLE void setLights(bool on);

signals:
    void settingsChanged();

private:
    struct Request {
        bool isLights;
        SidetoneLevel level;
        HeadsetLights lights;
    };
    using Outcome = shared_kernel::Result<void>;

    void submit(Request request);
    void start(Request request);
    void onFinished();

    const ChangeHeadsetSetting* changeHeadsetSetting_; // não nulo
    QFutureWatcher<Outcome> watcher_;
    std::optional<Request> current_; // a operação em andamento
    std::optional<Request> pending_;
    std::function<void(int)> sidetoneObserver_;
    QString status_;
    QString lightsState_;
    int lastSidetone_ = -1;
    bool running_ = false;
};

} // namespace ghub::headset
