#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace ghub::app {

// Configurações do próprio aplicativo (INC-017 R1/R9, ADR-0028): aviso de bateria fraca (desligado por padrão,
// limiar 5 a 50 %) e apelidos de dispositivo (só no Hubless; nada é gravado no dispositivo). Persistidas em
// `<GenericConfigLocation>/ghub-linux/settings.json` (0600) a cada alteração pedida pelo usuário; nada é gravado
// na construção nem em `load()`. Se a leitura falhar, os padrões valem e nenhuma gravação sobrescreve o arquivo
// que o app não entendeu. Use da thread dona (QML).
class AppSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool lowBatteryAlertEnabled READ lowBatteryAlertEnabled NOTIFY changed)
    Q_PROPERTY(int lowBatteryThreshold READ lowBatteryThreshold NOTIFY changed)
    // Nome do catálogo -> apelido (só os que existem).
    Q_PROPERTY(QVariantMap nicknames READ nicknames NOTIFY changed)
    Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    static constexpr qint64 maximumFileBytes = 64 * 1024;

    explicit AppSettings(QString filePath, QObject* parent = nullptr);

    // <GenericConfigLocation>/ghub-linux/settings.json (respeita XDG_CONFIG_HOME).
    [[nodiscard]] static QString defaultFilePath();

    [[nodiscard]] bool lowBatteryAlertEnabled() const { return alertEnabled_; }
    [[nodiscard]] int lowBatteryThreshold() const { return alertThreshold_; }
    [[nodiscard]] QVariantMap nicknames() const;
    [[nodiscard]] bool loadFailed() const { return loadFailed_; }
    [[nodiscard]] QString status() const { return status_; }

    // Apelido do dispositivo (nome do catálogo) ou o próprio nome do catálogo.
    Q_INVOKABLE [[nodiscard]] QString displayName(const QString& catalogName) const;

    // Lê o arquivo (ausente = padrões). Falha de leitura -> padrões, `loadFailed`.
    Q_INVOKABLE void load();

    // Cada setter valida, grava e emite `changed`; devolve false (com `status` "Falha: ...") sem alterar nada.
    Q_INVOKABLE bool setLowBatteryAlertEnabled(bool enabled);
    Q_INVOKABLE bool setLowBatteryThreshold(int percent);
    Q_INVOKABLE bool setNickname(const QString& catalogName, const QString& nickname);
    Q_INVOKABLE bool clearNickname(const QString& catalogName);

    // Cópia local (INC-017 R8): o documento completo e a restauração a partir de um documento já validado.
    [[nodiscard]] QByteArray toDocument() const;
    // Vazio = documento válido; senão o motivo.
    [[nodiscard]] static QString validateDocument(const QByteArray& document);
    [[nodiscard]] bool restoreFromDocument(const QByteArray& document);

signals:
    void changed();

private:
    struct State {
        bool alertEnabled = false;
        int alertThreshold = 15;
        QVariantMap nicknames;
    };

    [[nodiscard]] static QString parse(const QByteArray& document, State& out);
    [[nodiscard]] bool save();
    [[nodiscard]] bool fail(const QString& reason);

    QString filePath_;
    bool alertEnabled_ = false;
    int alertThreshold_ = 15;
    QVariantMap nicknames_;
    bool loadFailed_ = false;
    QString status_;
};

} // namespace ghub::app
