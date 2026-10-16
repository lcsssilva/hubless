#include "modules/profiles/infrastructure/JsonProfileRepository.hpp"

#include "modules/profiles/infrastructure/JsonProfileFormat.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>

namespace ghub::profiles {

namespace {

using shared_kernel::ErrorCode;
namespace fmt = json_format;

constexpr const char* kFormat = "ghub-linux-profiles";
constexpr int kVersion = 4; // versão gravada; de 1 a 3 ainda são lidas (ADR-0022 R3, ADR-0028)
constexpr const char* kContext = "profiles.json inválido";

std::unexpected<shared_kernel::DomainError> malformed(const QString& reason)
{
    return fmt::malformed(kContext, reason);
}

bool onlyKeys(const QJsonObject& object, const std::set<QString>& allowed)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!allowed.contains(it.key())) {
            return false;
        }
    }
    return true;
}

} // namespace

JsonProfileRepository::JsonProfileRepository(QString filePath) : filePath_(std::move(filePath)) {}

QString JsonProfileRepository::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/ghub-linux/profiles.json";
}

shared_kernel::Result<ProfileLibrary> JsonProfileRepository::parseDocument(const QByteArray& content)
{
    if (content.size() > maximumFileBytes) {
        return malformed("arquivo maior que 1 MiB");
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
    const auto version = fmt::exactInteger(root.value("version"), 0, 1'000'000);
    if (!version || *version < 1 || *version > kVersion) {
        return malformed("\"version\" não suportada (esperada de 1 a 4)");
    }
    const bool version2 = *version >= 2;
    const bool version3 = *version >= 3;
    const bool version4 = *version >= 4;
    std::set<QString> rootKeys{"format", "version", "profiles"};
    if (version2) {
        rootKeys.insert("automaticSwitching");
    }
    if (version3) {
        rootKeys.insert("defaultProfile");
        rootKeys.insert("persistentSections");
    }
    if (!onlyKeys(root, rootKeys)) {
        return malformed("chave desconhecida na raiz");
    }
    if (root.value("format").toString() != kFormat) {
        return malformed("\"format\" diferente de ghub-linux-profiles");
    }
    bool automaticSwitching = false;
    if (version2) {
        if (!root.value("automaticSwitching").isBool()) {
            return malformed("\"automaticSwitching\" deve ser verdadeiro ou falso");
        }
        automaticSwitching = root.value("automaticSwitching").toBool();
    }
    if (!root.value("profiles").isArray()) {
        return malformed("\"profiles\" deve ser uma lista");
    }
    std::vector<Profile> profiles;
    for (const QJsonValue& entry : root.value("profiles").toArray()) {
        auto profile = fmt::parseProfile(entry, {.context = kContext, .allowApplications = version2,
                                                      .allowHardware = version4});
        if (!profile) {
            return std::unexpected(profile.error());
        }
        profiles.push_back(std::move(*profile));
    }
    auto library = ProfileLibrary::create(std::move(profiles));
    if (!library) {
        return library;
    }
    library->setAutomaticSwitching(automaticSwitching);
    if (root.contains("defaultProfile")) {
        if (!root.value("defaultProfile").isString()) {
            return malformed("\"defaultProfile\" deve ser texto");
        }
        auto name = ProfileName::create(root.value("defaultProfile").toString().toStdString());
        if (!name) {
            return malformed(QString::fromStdString(name.error().detail));
        }
        if (auto set = library->setDefaultProfile(*name); !set) {
            return malformed("\"defaultProfile\" não é um perfil existente");
        }
    }
    if (root.contains("persistentSections")) {
        if (!root.value("persistentSections").isArray()) {
            return malformed("\"persistentSections\" deve ser uma lista");
        }
        std::set<QString> seen;
        for (const QJsonValue& entry : root.value("persistentSections").toArray()) {
            const QString key = entry.toString();
            const auto section = fmt::sectionFromKey(key, version4);
            if (!entry.isString() || !section.has_value() || !seen.insert(key).second) {
                return malformed("\"persistentSections\" com seção desconhecida ou repetida");
            }
            library->setPersistent(*section, true);
        }
    }
    return library;
}

shared_kernel::Result<ProfileLibrary> JsonProfileRepository::load()
{
    auto result = [this]() -> shared_kernel::Result<ProfileLibrary> {
        QFile file(filePath_);
        if (!file.exists()) {
            return ProfileLibrary{};
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return shared_kernel::fail(ErrorCode::BackendUnavailable, "não foi possível ler profiles.json");
        }
        return parseDocument(file.read(maximumFileBytes + 1));
    }();
    lastLoadFailed_ = !result.has_value();
    return result;
}

shared_kernel::Result<void> JsonProfileRepository::save(const ProfileLibrary& library)
{
    if (lastLoadFailed_) {
        return shared_kernel::fail(ErrorCode::MalformedData,
                                   "gravação recusada: a leitura anterior de profiles.json falhou");
    }
    QJsonArray profiles;
    for (const Profile& profile : library.profiles()) {
        for (const CameraControlSetting& setting : profile.camera()) {
            if (setting.value > fmt::kMaxExactInteger || setting.value < -fmt::kMaxExactInteger) {
                return shared_kernel::fail(ErrorCode::InvalidArgument,
                                           "valor de controle fora da faixa exata do JSON (2^53-1)");
            }
        }
        profiles.append(fmt::toJson(profile, true));
    }
    QJsonObject root{{"format", kFormat},
                     {"version", kVersion},
                     {"automaticSwitching", library.automaticSwitching()},
                     {"profiles", profiles}};
    if (library.defaultProfile().has_value()) {
        root.insert("defaultProfile", QString::fromStdString(library.defaultProfile()->value()));
    }
    QJsonArray persistent;
    for (std::size_t i = 0; i < profileSectionCount; ++i) {
        const auto section = static_cast<ProfileSection>(i);
        if (library.isPersistent(section)) {
            persistent.append(fmt::sectionKey(section));
        }
    }
    if (!persistent.isEmpty()) {
        root.insert("persistentSections", persistent);
    }

    using P = QFileDevice::Permission;
    const QString directory = QFileInfo(filePath_).absolutePath();
    if (!QDir().mkpath(directory, P::ReadOwner | P::WriteOwner | P::ExeOwner)) {
        return shared_kernel::fail(ErrorCode::BackendUnavailable, "não foi possível criar o diretório de perfis");
    }
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        return shared_kernel::fail(ErrorCode::BackendUnavailable, "não foi possível abrir profiles.json para gravar");
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.setPermissions(P::ReadOwner | P::WriteOwner) || !file.commit()) {
        file.cancelWriting();
        return shared_kernel::fail(ErrorCode::BackendUnavailable, "falha ao gravar profiles.json");
    }
    return {};
}

} // namespace ghub::profiles
