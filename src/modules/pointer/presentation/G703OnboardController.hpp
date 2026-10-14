#pragma once

#include "modules/pointer/application/G703OnboardSettings.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>
#include <optional>
#include <string>

namespace ghub::pointer {

// Lê, grava e restaura o perfil onboard do G703 (DPI e taxa) por ação do usuário, fora da thread de UI
// (ADR-0017 §3, INC-011 R10). Nada é executado na construção. Uma operação por vez: um pedido feito durante uma
// operação é recusado ("aguarde a operação em andamento"), nunca enfileirado, pois a gravação é persistente.
// A gravação é validada pelo domínio (G703ProfileSettings::make) antes de iniciar o trabalho. O destrutor espera
// a operação em andamento. O caso de uso deve sobreviver a este objeto. Use sempre da thread dona.
class G703OnboardController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY stateChanged)
    Q_PROPERTY(int reportRateHz READ reportRateHz NOTIFY stateChanged)
    Q_PROPERTY(QVariantList dpiStages READ dpiStages NOTIFY stateChanged)
    Q_PROPERTY(int defaultStage READ defaultStage NOTIFY stateChanged)
    Q_PROPERTY(bool backupExists READ backupExists NOTIFY stateChanged)
    Q_PROPERTY(int activeSector READ activeSector NOTIFY stateChanged)

public:
    explicit G703OnboardController(const G703OnboardSettings& settings, QObject* parent = nullptr);
    ~G703OnboardController() override;

    G703OnboardController(const G703OnboardController&) = delete;
    G703OnboardController& operator=(const G703OnboardController&) = delete;
    G703OnboardController(G703OnboardController&&) = delete;
    G703OnboardController& operator=(G703OnboardController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool loaded() const { return loaded_; }
    [[nodiscard]] int reportRateHz() const { return reportRateHz_; }
    [[nodiscard]] QVariantList dpiStages() const { return dpiStages_; }
    [[nodiscard]] int defaultStage() const { return defaultStage_; }
    [[nodiscard]] bool backupExists() const { return backupExists_; }
    [[nodiscard]] int activeSector() const { return activeSector_; }

    Q_INVOKABLE void readFromMouse();
    // Valores inválidos: status com o motivo, nada é executado.
    Q_INVOKABLE void writeToMouse(int reportRateHz, const QVariantList& dpiStages, int defaultStage);
    Q_INVOKABLE void restoreBackup();

signals:
    void stateChanged();

private:
    enum class Kind : std::uint8_t { Read, Write, Restore };

    struct Outcome {
        Kind kind;
        std::optional<G703ProfileSettings> settings = std::nullopt; // vazio = falha
        std::optional<std::uint16_t> sector = std::nullopt;         // da leitura (também relida após gravar)
        bool backupExists = false;                                  // idem
        // NOLINTNEXTLINE(readability-redundant-member-init): o GCC exige (-Wmissing-field-initializers).
        std::string error = {};
    };

    // Recusa se ocupado; devolve true se o pedido pode prosseguir.
    [[nodiscard]] bool acceptRequest();
    void fail(const QString& message);
    void start(Kind kind, std::optional<G703ProfileSettings> request);
    void onFinished();

    const G703OnboardSettings* settings_; // não nulo
    QFutureWatcher<Outcome> watcher_;
    QString status_;
    QVariantList dpiStages_;
    int reportRateHz_ = 0;
    int defaultStage_ = 0;
    int activeSector_ = 0;
    bool loaded_ = false;
    bool backupExists_ = false;
    bool running_ = false;
};

} // namespace ghub::pointer
