#include "ConfigBackup.hpp"

#include "AppSettings.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace ghub::app {

namespace {

constexpr const char* kFormat = "ghub-linux-backup";
constexpr int kVersion = 1;
constexpr qint64 kProfilesLimit = 1024 * 1024;

const QRegularExpression& namePattern()
{
    static const QRegularExpression pattern{QStringLiteral(R"(^config-\d{8}-\d{6}(-\d+)?\.json$)")};
    return pattern;
}

bool writePrivate(const QString& path, const QByteArray& bytes)
{
    using P = QFileDevice::Permission;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
        !file.setPermissions(P::ReadOwner | P::WriteOwner) || !file.commit()) {
        file.cancelWriting();
        return false;
    }
    return true;
}

} // namespace

ConfigBackup::ConfigBackup(QString backupDirectory, QString profilesPath, AppSettings& settings,
                           ProfilesValidator validate, QObject* parent)
    : QObject(parent), directory_(std::move(backupDirectory)), profilesPath_(std::move(profilesPath)),
      settings_(&settings), validate_(std::move(validate))
{
    refresh();
}

QString ConfigBackup::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/ghub-linux/backup");
}

void ConfigBackup::refresh()
{
    QStringList names;
    const QDir dir{directory_};
    for (const QString& name : dir.entryList(QDir::Files, QDir::Name)) {
        if (namePattern().match(name).hasMatch()) {
            names.push_back(name);
        }
    }
    std::ranges::sort(names, std::greater<>{}); // o nome carrega a data: mais recente primeiro
    if (names != backups_) {
        backups_ = names;
        emit backupsChanged();
    }
}

bool ConfigBackup::fail(const QString& reason)
{
    status_ = QStringLiteral("Falha: %1").arg(reason);
    emit backupsChanged();
    return false;
}

bool ConfigBackup::makeBackup()
{
    QJsonValue profiles = QJsonValue::Null;
    if (QFile::exists(profilesPath_)) {
        QFile file(profilesPath_);
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(QStringLiteral("não foi possível ler os perfis para a cópia"));
        }
        const QByteArray content = file.read(kProfilesLimit + 1);
        const QJsonDocument document = QJsonDocument::fromJson(content);
        if (content.size() > kProfilesLimit || !document.isObject()) {
            return fail(QStringLiteral("o arquivo de perfis está ilegível; a cópia não foi feita"));
        }
        profiles = document.object();
    }
    const QJsonDocument settings = QJsonDocument::fromJson(settings_->toDocument());
    const QJsonObject root{{QStringLiteral("format"), QLatin1String(kFormat)},
                           {QStringLiteral("version"), kVersion},
                           {QStringLiteral("createdAt"), QDateTime::currentDateTime().toString(Qt::ISODate)},
                           {QStringLiteral("profiles"), profiles},
                           {QStringLiteral("settings"), settings.object()}};

    using P = QFileDevice::Permission;
    if (!QDir().mkpath(directory_, P::ReadOwner | P::WriteOwner | P::ExeOwner)) {
        return fail(QStringLiteral("não foi possível criar o diretório de cópias"));
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString name = QStringLiteral("config-%1.json").arg(stamp);
    for (int suffix = 1; QFile::exists(directory_ + QLatin1Char('/') + name); ++suffix) {
        name = QStringLiteral("config-%1-%2.json").arg(stamp).arg(suffix);
    }
    if (!writePrivate(directory_ + QLatin1Char('/') + name, QJsonDocument(root).toJson(QJsonDocument::Indented))) {
        return fail(QStringLiteral("não foi possível gravar a cópia"));
    }
    status_ = QStringLiteral("Cópia feita: %1").arg(name);
    backups_.clear(); // força a emissão pelo refresh
    refresh();
    emit backupsChanged();
    return true;
}

bool ConfigBackup::restore(const QString& name)
{
    if (!namePattern().match(name).hasMatch()) {
        return fail(QStringLiteral("nome de cópia inválido"));
    }
    QFile file(directory_ + QLatin1Char('/') + name);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("não foi possível ler a cópia %1").arg(name));
    }
    const QByteArray content = file.read(maximumFileBytes + 1);
    if (content.size() > maximumFileBytes) {
        return fail(QStringLiteral("a cópia é grande demais"));
    }
    const QJsonDocument document = QJsonDocument::fromJson(content);
    if (!document.isObject()) {
        return fail(QStringLiteral("a cópia não é um JSON válido"));
    }
    const QJsonObject root = document.object();
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat) || !version.isDouble() ||
        version.toDouble() != kVersion) {
        return fail(QStringLiteral("a cópia tem formato ou versão desconhecidos"));
    }
    // Tudo é validado antes de qualquer regravação.
    const QJsonValue profiles = root.value(QStringLiteral("profiles"));
    QByteArray profilesBytes;
    if (profiles.isObject()) {
        profilesBytes = QJsonDocument(profiles.toObject()).toJson(QJsonDocument::Indented);
        if (const QString reason = validate_(profilesBytes); !reason.isEmpty()) {
            return fail(QStringLiteral("perfis da cópia inválidos (%1)").arg(reason));
        }
    } else if (!profiles.isNull()) {
        return fail(QStringLiteral("perfis da cópia inválidos"));
    }
    const QByteArray settingsBytes =
        QJsonDocument(root.value(QStringLiteral("settings")).toObject()).toJson(QJsonDocument::Indented);
    if (const QString reason = AppSettings::validateDocument(settingsBytes); !reason.isEmpty()) {
        return fail(QStringLiteral("configurações da cópia inválidas (%1)").arg(reason));
    }

    if (!profilesBytes.isEmpty()) {
        using P = QFileDevice::Permission;
        if (!QDir().mkpath(QFileInfo(profilesPath_).absolutePath(), P::ReadOwner | P::WriteOwner | P::ExeOwner) ||
            !writePrivate(profilesPath_, profilesBytes)) {
            return fail(QStringLiteral("não foi possível regravar os perfis"));
        }
    }
    if (!settings_->restoreFromDocument(settingsBytes)) {
        return fail(QStringLiteral("não foi possível regravar as configurações"));
    }
    status_ = QStringLiteral("Cópia restaurada: %1").arg(name);
    emit backupsChanged();
    emit restored();
    return true;
}

} // namespace ghub::app
