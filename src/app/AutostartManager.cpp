#include "AutostartManager.hpp"

#include "AutostartEntry.hpp"

#include <QByteArray>
#include <QChar>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <utility>

namespace ghub::app {

namespace {

// $XDG_CONFIG_HOME com fallback ~/.config (spec XDG Base Directory; INC-013 R7).
QString autostartDirectory()
{
    auto config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (config.isEmpty()) {
        config = QDir::homePath() + QStringLiteral("/.config");
    }
    return config + QStringLiteral("/autostart");
}

// `Hidden=true` desliga a entrada mesmo com o arquivo presente (spec XDG Autostart). A chave pode ter
// vindo de outro gerenciador, então é lida de forma tolerante: espaços em volta do '=' e do valor (a
// spec Desktop Entry manda ignorá-los) e caixa indiferente.
bool isMarkedHidden(const QString& content)
{
    for (const QString& rawLine : content.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue; // linha vazia, comentário ou sem chave/valor
        }
        if (line.left(separator).trimmed().compare(QStringLiteral("Hidden"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        return line.sliced(separator + 1).trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    }
    return false;
}

} // namespace

AutostartManager::AutostartManager(QString directory)
    : directory_(directory.isEmpty() ? autostartDirectory() : std::move(directory))
{
}

QString AutostartManager::filePath() const
{
    return QDir{directory_}.filePath(QStringLiteral("ghub-linux.desktop"));
}

bool AutostartManager::isEnabled() const
{
    QFile file{filePath()};
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        // Ausente ou ilegível conta como desligado: sem ler o conteúdo não há como afirmar que a entrada
        // está ativa — e o gerenciador de sessão também não conseguiria lê-la.
        return false;
    }
    return !isMarkedHidden(QString::fromUtf8(file.readAll()));
}

shared_kernel::Result<void> AutostartManager::setEnabled(bool enabled) const
{
    const QString path = filePath();
    if (!enabled) {
        QFile file{path};
        if (!file.exists()) {
            return {}; // já desligado: desligar de novo é sucesso
        }
        if (!file.remove()) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível remover " + path.toStdString());
        }
        return {};
    }

    QDir directory{directory_};
    if (!directory.mkpath(QStringLiteral("."))) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "não foi possível criar o diretório " + directory_.toStdString());
    }
    // QSaveFile: troca atômica, para o gerenciador de sessão nunca ler um .desktop pela metade no login.
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "não foi possível abrir " + path.toStdString());
    }
    const QByteArray content = renderAutostartEntry(defaultAutostartEntry()).toUtf8();
    if (file.write(content) != content.size()) {
        file.cancelWriting();
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "não foi possível gravar " + path.toStdString());
    }
    if (!file.commit()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "não foi possível concluir a gravação de " + path.toStdString());
    }
    return {};
}

} // namespace ghub::app
