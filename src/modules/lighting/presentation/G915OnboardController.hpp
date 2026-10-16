#pragma once

#include "modules/lighting/application/G915OnboardSettings.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace ghub::lighting::presentation {

// Lê, grava e restaura o perfil onboard do G915 (efeito gravado do logo e das teclas e teclas G1-G5, INC-019 R4 e
// INC-022 R1/R4, ADR-0029) por ação do usuário, fora da thread de UI. Nada é executado na construção nem ao
// reconectar. Uma operação por vez: um pedido feito durante uma operação é recusado ("aguarde a operação em
// andamento"), nunca enfileirado, pois a gravação é persistente. A gravação é validada pelo domínio antes de iniciar o
// trabalho. O destrutor espera a operação em andamento. O caso de uso deve sobreviver a este objeto. Use sempre da
// thread dona. Com o modo ao vivo ou uma animação ativos, o lease do teclado recusa a operação (DeviceBusy).
class G915OnboardController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY stateChanged)
    // Mapas {kind: "static"|"breathing"|"cycle"|"wave"|"ripple"|"raw", color: "#rrggbb", periodMs, brightness, speed}.
    Q_PROPERTY(QVariantMap logoEffect READ logoEffect NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap keysEffect READ keysEffect NOTIFY stateChanged)
    // Cinco mapas {kind: "key"|"raw", modifiers: 0..15, key: "C", raw: "xx xx xx xx"}; o QML também pode enviar
    // {kind: "default"} para voltar ao valor da cópia original.
    Q_PROPERTY(QVariantList gKeys READ gKeys NOTIFY stateChanged)
    Q_PROPERTY(QStringList keyChoices READ keyChoices CONSTANT)
    Q_PROPERTY(bool backupExists READ backupExists NOTIFY stateChanged)
    Q_PROPERTY(int activeSector READ activeSector NOTIFY stateChanged)

public:
    explicit G915OnboardController(const application::G915OnboardSettings& settings, QObject* parent = nullptr);
    ~G915OnboardController() override;

    G915OnboardController(const G915OnboardController&) = delete;
    G915OnboardController& operator=(const G915OnboardController&) = delete;
    G915OnboardController(G915OnboardController&&) = delete;
    G915OnboardController& operator=(G915OnboardController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool loaded() const { return loaded_; }
    [[nodiscard]] QVariantMap logoEffect() const { return logoEffect_; }
    [[nodiscard]] QVariantMap keysEffect() const { return keysEffect_; }
    [[nodiscard]] QVariantList gKeys() const { return gKeys_; }
    // Teclas de teclado do catálogo (sem o prefixo "Key: ") que podem ser atribuídas a uma tecla G.
    [[nodiscard]] QStringList keyChoices() const;
    [[nodiscard]] bool backupExists() const { return backupExists_; }
    [[nodiscard]] int activeSector() const { return activeSector_; }

    Q_INVOKABLE void readFromKeyboard();
    // Valores inválidos: status com o motivo, nada é executado. Um efeito de tipo "raw" (formato não interpretado,
    // devolvido como foi lido) não é alterado.
    Q_INVOKABLE void writeToKeyboard(const QVariantMap& logoEffect, const QVariantMap& keysEffect,
                                     const QVariantList& gKeys);
    Q_INVOKABLE void restoreBackup();

signals:
    void stateChanged();

private:
    enum class Kind : std::uint8_t { Read, Write, Restore };

    struct Outcome {
        Kind kind;
        std::optional<domain::G915ProfileSettings> settings = std::nullopt; // vazio = falha
        std::optional<std::uint16_t> sector = std::nullopt;                 // da leitura (também relida após gravar)
        bool backupExists = false;                                          // idem
        // NOLINTNEXTLINE(readability-redundant-member-init): o GCC exige (-Wmissing-field-initializers).
        std::string error = {};
    };

    // Recusa se ocupado; devolve true se o pedido pode prosseguir.
    [[nodiscard]] bool acceptRequest();
    void fail(const QString& message);
    void start(Kind kind, std::optional<domain::G915ProfileSettings> request);
    void onFinished();

    const application::G915OnboardSettings* settings_; // não nulo
    QFutureWatcher<Outcome> watcher_;
    QString status_;
    QVariantMap logoEffect_;
    QVariantMap keysEffect_;
    QVariantList gKeys_;
    int activeSector_ = 0;
    bool loaded_ = false;
    bool backupExists_ = false;
    bool running_ = false;
};

} // namespace ghub::lighting::presentation
