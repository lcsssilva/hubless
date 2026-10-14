#include "modules/diagnostics/infrastructure/XdgReportWriter.hpp"

#include <QDateTime>
#include <QDir>
#include <QIODevice>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>

#include <utility>

namespace ghub::diagnostics {

namespace {

// $XDG_STATE_HOME com fallback ~/.local/state (especificação XDG; INC-013 R13).
QString stateDirectory()
{
    auto state = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    if (state.isEmpty()) {
        state = QDir::homePath() + QStringLiteral("/.local/state");
    }
    return state + QStringLiteral("/ghub-linux/diagnosticos");
}

QString reportFileName()
{
    return QStringLiteral("diagnostico-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")) +
           QStringLiteral(".txt");
}

} // namespace

XdgReportWriter::XdgReportWriter(QString directory)
    : directory_(directory.isEmpty() ? stateDirectory() : std::move(directory))
{
}

shared_kernel::Result<std::string> XdgReportWriter::write(std::string_view content) const
{
    QDir directory{directory_};
    if (!directory.mkpath(QStringLiteral("."))) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "não foi possível criar " + directory_.toStdString());
    }
    const auto path = directory.filePath(reportFileName());
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "não foi possível abrir " + path.toStdString());
    }
    const auto size = static_cast<qint64>(content.size());
    if (file.write(content.data(), size) != size) {
        file.cancelWriting();
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "não foi possível gravar " + path.toStdString());
    }
    if (!file.commit()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "não foi possível concluir a gravação de " + path.toStdString());
    }
    return path.toStdString();
}

} // namespace ghub::diagnostics
