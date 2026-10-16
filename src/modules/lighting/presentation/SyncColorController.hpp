#pragma once

#include "modules/lighting/application/SyncColor.hpp"

#include <QColor>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

namespace ghub::lighting::presentation {

// "Sincronizar cor" para a janela (INC-021 R5, ADR-0030). Nada é lido nem escrito na construção: apply() só roda por
// clique do usuário. O teclado (G915, modo ao vivo, não bloqueante) é tratado na thread de UI, que é a dona da
// sessão ao vivo; o headset (G733, volátil) e o mouse (G703, persistente: só com `confirmMouse`) rodam numa thread de
// trabalho. Uma operação por vez: um pedido durante outra é ignorado. A cor nunca é registrada em log. O caso de uso
// deve sobreviver a este objeto; o destrutor espera a operação em andamento. Use sempre a partir da thread que o
// possui.
class SyncColorController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // Uma linha por dispositivo selecionado: "G915: aplicado", "G733: falhou (...)", "G703: pulado (...)".
    Q_PROPERTY(QStringList results READ results NOTIFY changed)

public:
    explicit SyncColorController(const application::SyncColor& sync, QObject* parent = nullptr);
    ~SyncColorController() override;

    SyncColorController(const SyncColorController&) = delete;
    SyncColorController& operator=(const SyncColorController&) = delete;
    SyncColorController(SyncColorController&&) = delete;
    SyncColorController& operator=(SyncColorController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QStringList results() const { return results_; }

    Q_INVOKABLE void apply(const QColor& color, bool keyboard, bool headset, bool mouse, bool confirmMouse);

signals:
    void changed();
    // Ao fim de cada operação (também quando tudo foi pulado).
    void finished();

private:
    void onFinished();
    void publish(const std::vector<application::SyncOutcome>& outcomes);

    const application::SyncColor* sync_; // não nulo
    QFutureWatcher<std::vector<application::SyncOutcome>> watcher_;
    std::vector<application::SyncOutcome> keyboardOutcomes_;
    QString status_;
    QStringList results_;
    bool running_ = false;
};

} // namespace ghub::lighting::presentation
