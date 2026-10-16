#include "AppSettings.hpp"

#include "modules/battery/domain/LowBatteryAlert.hpp"
#include "modules/device_inventory/api/DeviceInventory.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <cmath>

namespace ghub::app {

namespace {

constexpr const char* kFormat = "ghub-linux-settings";
constexpr int kVersion = 1;

bool knownDevice(const QString& catalogName)
{
    for (const auto model : device_inventory::kDeviceModels) {
        if (QString::fromUtf8(device_inventory::modelName(model)) == catalogName) {
            return true;
        }
    }
    return false;
}

} // namespace

AppSettings::AppSettings(QString filePath, QObject* parent) : QObject(parent), filePath_(std::move(filePath)) {}

QString AppSettings::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/ghub-linux/settings.json");
}

QVariantMap AppSettings::nicknames() const
{
    return nicknames_;
}

QString AppSettings::displayName(const QString& catalogName) const
{
    const QString nickname = nicknames_.value(catalogName).toString();
    return nickname.isEmpty() ? catalogName : nickname;
}

// Devolve vazio se válido; senão o motivo. Preenche `out` só quando válido.
QString AppSettings::parse(const QByteArray& document, State& out)
{
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(document, &parseError);
    if (parsed.isNull() || !parsed.isObject()) {
        return QStringLiteral("JSON ilegível");
    }
    const QJsonObject root = parsed.object();
    static const QSet<QString> rootKeys{QStringLiteral("format"), QStringLiteral("version"),
                                        QStringLiteral("lowBatteryAlert"), QStringLiteral("nicknames")};
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!rootKeys.contains(it.key())) {
            return QStringLiteral("chave desconhecida: %1").arg(it.key());
        }
    }
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)) {
        return QStringLiteral("formato diferente de %1").arg(QLatin1String(kFormat));
    }
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != kVersion) {
        return QStringLiteral("versão não suportada");
    }
    State state;
    if (root.contains(QStringLiteral("lowBatteryAlert"))) {
        const QJsonValue alert = root.value(QStringLiteral("lowBatteryAlert"));
        if (!alert.isObject()) {
            return QStringLiteral("\"lowBatteryAlert\" deve ser um objeto");
        }
        const QJsonObject object = alert.toObject();
        if (!object.value(QStringLiteral("enabled")).isBool()) {
            return QStringLiteral("\"enabled\" deve ser verdadeiro ou falso");
        }
        const QJsonValue threshold = object.value(QStringLiteral("threshold"));
        if (!threshold.isDouble() || std::trunc(threshold.toDouble()) != threshold.toDouble() ||
            !battery::LowBatteryThreshold::create(static_cast<int>(threshold.toDouble())).has_value()) {
            return QStringLiteral("\"threshold\" deve ser um inteiro de 5 a 50");
        }
        state.alertEnabled = object.value(QStringLiteral("enabled")).toBool();
        state.alertThreshold = static_cast<int>(threshold.toDouble());
    }
    if (root.contains(QStringLiteral("nicknames"))) {
        const QJsonValue nicknames = root.value(QStringLiteral("nicknames"));
        if (!nicknames.isObject()) {
            return QStringLiteral("\"nicknames\" deve ser um objeto");
        }
        const QJsonObject object = nicknames.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            const auto nickname = device_inventory::DeviceNickname::create(it.value().toString().toStdString());
            if (!knownDevice(it.key()) || !it.value().isString() || !nickname) {
                return QStringLiteral("apelido inválido para %1").arg(it.key());
            }
            state.nicknames.insert(it.key(), QString::fromStdString(nickname->value()));
        }
    }
    out = state;
    return {};
}

QString AppSettings::validateDocument(const QByteArray& document)
{
    State ignored;
    return parse(document, ignored);
}

QByteArray AppSettings::toDocument() const
{
    QJsonObject nicknames;
    for (auto it = nicknames_.begin(); it != nicknames_.end(); ++it) {
        nicknames.insert(it.key(), it.value().toString());
    }
    QJsonObject root{{QStringLiteral("format"), QLatin1String(kFormat)},
                     {QStringLiteral("version"), kVersion},
                     {QStringLiteral("lowBatteryAlert"), QJsonObject{{QStringLiteral("enabled"), alertEnabled_},
                                                                     {QStringLiteral("threshold"), alertThreshold_}}}};
    if (!nicknames.isEmpty()) {
        root.insert(QStringLiteral("nicknames"), nicknames);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

void AppSettings::load()
{
    loadFailed_ = false;
    QFile file(filePath_);
    if (!file.exists()) {
        emit changed();
        return;
    }
    State state;
    QString reason;
    if (!file.open(QIODevice::ReadOnly)) {
        reason = QStringLiteral("não foi possível ler settings.json");
    } else {
        const QByteArray content = file.read(maximumFileBytes + 1);
        reason = content.size() > maximumFileBytes ? QStringLiteral("arquivo maior que 64 KiB") : parse(content, state);
    }
    if (!reason.isEmpty()) {
        loadFailed_ = true;
        alertEnabled_ = false;
        alertThreshold_ = 15;
        nicknames_.clear();
        status_ = QStringLiteral("Configurações ilegíveis (%1): valem os padrões e nada será gravado.").arg(reason);
        qWarning("app: settings.json ilegível (%s)", qPrintable(reason));
    } else {
        alertEnabled_ = state.alertEnabled;
        alertThreshold_ = state.alertThreshold;
        nicknames_ = state.nicknames;
    }
    emit changed();
}

bool AppSettings::fail(const QString& reason)
{
    status_ = QStringLiteral("Falha: %1").arg(reason);
    emit changed();
    return false;
}

bool AppSettings::save()
{
    if (loadFailed_) {
        return fail(QStringLiteral("o arquivo de configurações não foi entendido; corrija-o antes de gravar"));
    }
    using P = QFileDevice::Permission;
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath(), P::ReadOwner | P::WriteOwner | P::ExeOwner)) {
        return fail(QStringLiteral("não foi possível criar o diretório de configurações"));
    }
    QSaveFile file(filePath_);
    const QByteArray bytes = toDocument();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
        !file.setPermissions(P::ReadOwner | P::WriteOwner) || !file.commit()) {
        file.cancelWriting();
        return fail(QStringLiteral("não foi possível gravar settings.json"));
    }
    return true;
}

bool AppSettings::setLowBatteryAlertEnabled(bool enabled)
{
    const bool previous = alertEnabled_;
    alertEnabled_ = enabled;
    if (!save()) {
        alertEnabled_ = previous;
        return false;
    }
    status_ =
        enabled ? QStringLiteral("Aviso de bateria fraca ligado") : QStringLiteral("Aviso de bateria fraca desligado");
    emit changed();
    return true;
}

bool AppSettings::setLowBatteryThreshold(int percent)
{
    if (!battery::LowBatteryThreshold::create(percent).has_value()) {
        return fail(QStringLiteral("o limiar deve estar entre 5 e 50 %"));
    }
    const int previous = alertThreshold_;
    alertThreshold_ = percent;
    if (!save()) {
        alertThreshold_ = previous;
        return false;
    }
    status_ = QStringLiteral("Limiar do aviso: %1 %").arg(percent);
    emit changed();
    return true;
}

bool AppSettings::setNickname(const QString& catalogName, const QString& nickname)
{
    if (!knownDevice(catalogName)) {
        return fail(QStringLiteral("dispositivo desconhecido: %1").arg(catalogName));
    }
    const auto valid = device_inventory::DeviceNickname::create(nickname.toStdString());
    if (!valid) {
        return fail(QString::fromStdString(valid.error().detail));
    }
    const QVariantMap previous = nicknames_;
    nicknames_.insert(catalogName, QString::fromStdString(valid->value()));
    if (!save()) {
        nicknames_ = previous;
        return false;
    }
    status_ = QStringLiteral("Nome de %1: %2").arg(catalogName, QString::fromStdString(valid->value()));
    emit changed();
    return true;
}

bool AppSettings::clearNickname(const QString& catalogName)
{
    if (!nicknames_.contains(catalogName)) {
        return true;
    }
    const QVariantMap previous = nicknames_;
    nicknames_.remove(catalogName);
    if (!save()) {
        nicknames_ = previous;
        return false;
    }
    status_ = QStringLiteral("Nome de %1 restaurado").arg(catalogName);
    emit changed();
    return true;
}

bool AppSettings::restoreFromDocument(const QByteArray& document)
{
    State state;
    if (const QString reason = parse(document, state); !reason.isEmpty()) {
        return fail(reason);
    }
    const State previous{alertEnabled_, alertThreshold_, nicknames_};
    const bool previousFailed = loadFailed_;
    // Restaurar é a escolha do usuário de substituir o arquivo, mesmo que o atual não fosse legível.
    loadFailed_ = false;
    alertEnabled_ = state.alertEnabled;
    alertThreshold_ = state.alertThreshold;
    nicknames_ = state.nicknames;
    if (!save()) {
        alertEnabled_ = previous.alertEnabled;
        alertThreshold_ = previous.alertThreshold;
        nicknames_ = previous.nicknames;
        loadFailed_ = previousFailed;
        return false;
    }
    status_ = QStringLiteral("Configurações restauradas");
    emit changed();
    return true;
}

} // namespace ghub::app
