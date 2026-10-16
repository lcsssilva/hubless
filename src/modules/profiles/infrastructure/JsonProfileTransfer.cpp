#include "modules/profiles/infrastructure/JsonProfileTransfer.hpp"

#include "modules/profiles/infrastructure/JsonProfileFormat.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>

namespace ghub::profiles {

namespace {

namespace fmt = json_format;
using shared_kernel::ErrorCode;

constexpr const char* kFormat = "ghub-linux-profile";
constexpr int kVersion = 1;                // arquivo sem seções de hardware
constexpr int kVersionWithHardware = 2;    // INC-017 parte 2: aceita as seções de hardware
constexpr const char* kContext = "arquivo de perfil inválido";

std::unexpected<shared_kernel::DomainError> malformed(const QString& reason)
{
    return fmt::malformed(kContext, reason);
}

} // namespace

shared_kernel::Result<void> JsonProfileTransfer::exportProfile(const Profile& profile, const std::string& path)
{
    const int version = profile.hardware().empty() ? kVersion : kVersionWithHardware;
    const QJsonObject root{{"format", kFormat}, {"version", version}, {"profile", fmt::toJson(profile, false)}};
    QSaveFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly)) {
        return shared_kernel::fail(ErrorCode::BackendUnavailable, "não foi possível abrir o arquivo para exportar");
    }
    using P = QFileDevice::Permission;
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.setPermissions(P::ReadOwner | P::WriteOwner) || !file.commit()) {
        file.cancelWriting();
        return shared_kernel::fail(ErrorCode::BackendUnavailable, "falha ao gravar o arquivo exportado");
    }
    return {};
}

shared_kernel::Result<ImportedProfile> JsonProfileTransfer::importProfile(const std::string& path)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        return shared_kernel::fail(ErrorCode::BackendUnavailable, "não foi possível ler o arquivo escolhido");
    }
    const QByteArray content = file.read(maximumFileBytes + 1);
    if (content.size() > maximumFileBytes) {
        return malformed("arquivo maior que 64 KiB");
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(content, &parseError);
    if (document.isNull()) {
        return malformed("JSON ilegível (" + parseError.errorString() + ")");
    }
    if (!document.isObject()) {
        return malformed("a raiz deve ser um objeto");
    }
    const QJsonObject root = document.object();
    if (root.value("format").toString() != kFormat) {
        return malformed("\"format\" diferente de ghub-linux-profile");
    }
    const auto version = fmt::exactInteger(root.value("version"), 0, 1'000'000);
    if (!version || (*version != kVersion && *version != kVersionWithHardware)) {
        return malformed("\"version\" não suportada (esperada 1 ou 2)");
    }
    if (!root.contains("profile")) {
        return malformed("falta a chave \"profile\"");
    }
    std::vector<std::string> ignored;
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.key() != "format" && it.key() != "version" && it.key() != "profile") {
            ignored.push_back(it.key().toStdString());
        }
    }
    auto profile = fmt::parseProfile(root.value("profile"), {.context = kContext, .allowHardware = *version >= kVersionWithHardware, .ignored = &ignored});
    if (!profile) {
        return std::unexpected(profile.error());
    }
    return ImportedProfile{.profile = std::move(*profile), .ignoredFields = std::move(ignored)};
}

} // namespace ghub::profiles
