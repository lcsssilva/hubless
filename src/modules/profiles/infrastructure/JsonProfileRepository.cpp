#include "modules/profiles/infrastructure/JsonProfileRepository.hpp"

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

constexpr const char* kFormat = "ghub-linux-profiles";
constexpr int kVersion = 2; // versão gravada; a 1 ainda é lida (ADR-0022, R3)
// O JSON guarda números como double: só inteiros até 2^53-1 são exatos, então só eles são aceitos e gravados.
constexpr std::int64_t kMaxExactInteger = 9007199254740991LL;

std::unexpected<shared_kernel::DomainError> malformed(const QString& reason)
{
    return shared_kernel::fail(ErrorCode::MalformedData, "profiles.json inválido: " + reason.toStdString());
}

// Inteiro exato dentro de [minimum, maximum]; qualquer outra coisa (texto, fração, fora da faixa) -> nullopt.
std::optional<std::int64_t> exactInteger(const QJsonValue& value, std::int64_t minimum, std::int64_t maximum)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number ||
        std::fabs(number) > static_cast<double>(kMaxExactInteger)) {
        return std::nullopt;
    }
    const auto integer = static_cast<std::int64_t>(number);
    if (integer < minimum || integer > maximum) {
        return std::nullopt;
    }
    return integer;
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

shared_kernel::Result<std::vector<CameraControlSetting>> parseCamera(const QJsonValue& value)
{
    if (!value.isArray()) {
        return malformed("\"camera\" deve ser uma lista");
    }
    std::vector<CameraControlSetting> camera;
    for (const QJsonValue& entry : value.toArray()) {
        if (!entry.isObject()) {
            return malformed("item de \"camera\" deve ser um objeto");
        }
        const QJsonObject object = entry.toObject();
        if (!onlyKeys(object, {"id", "name", "value"})) {
            return malformed("chave desconhecida em item de \"camera\"");
        }
        const auto id = exactInteger(object.value("id"), 0, std::numeric_limits<std::uint32_t>::max());
        const auto controlValue = exactInteger(object.value("value"), -kMaxExactInteger, kMaxExactInteger);
        if (!id || !controlValue || !object.value("name").isString()) {
            return malformed("item de \"camera\" com id, name ou value inválido");
        }
        camera.push_back({.id = static_cast<std::uint32_t>(*id),
                          .name = object.value("name").toString().toStdString(),
                          .value = *controlValue});
    }
    return camera;
}

shared_kernel::Result<EqualizerGains> parseEqualizer(const QJsonValue& value)
{
    if (!value.isArray()) {
        return malformed("\"equalizer\" deve ser uma lista");
    }
    std::vector<int> gains;
    for (const QJsonValue& entry : value.toArray()) {
        const auto gain = exactInteger(entry, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        if (!gain) {
            return malformed("ganho do \"equalizer\" não é um inteiro válido");
        }
        gains.push_back(static_cast<int>(*gain));
    }
    auto created = EqualizerGains::create(gains);
    if (!created) {
        return malformed(QString::fromStdString(created.error().detail));
    }
    return *created;
}

shared_kernel::Result<std::vector<ApplicationId>> parseApplications(const QJsonValue& value)
{
    if (!value.isArray()) {
        return malformed("\"applications\" deve ser uma lista");
    }
    std::vector<ApplicationId> applications;
    for (const QJsonValue& entry : value.toArray()) {
        if (!entry.isString()) {
            return malformed("aplicativo deve ser texto");
        }
        auto application = ApplicationId::create(entry.toString().toStdString());
        if (!application) {
            return malformed("aplicativo inválido");
        }
        applications.push_back(std::move(*application));
    }
    return applications;
}

shared_kernel::Result<Profile> parseProfile(const QJsonValue& value, bool version2)
{
    if (!value.isObject()) {
        return malformed("perfil deve ser um objeto");
    }
    const QJsonObject object = value.toObject();
    const std::set<QString> allowedKeys = version2 ? std::set<QString>{"name", "camera", "equalizer", "applications"}
                                                   : std::set<QString>{"name", "camera", "equalizer"};
    if (!onlyKeys(object, allowedKeys)) {
        return malformed("chave desconhecida em perfil");
    }
    if (!object.value("name").isString()) {
        return malformed("\"name\" do perfil deve ser texto");
    }
    auto name = ProfileName::create(object.value("name").toString().toStdString());
    if (!name) {
        return malformed(QString::fromStdString(name.error().detail));
    }
    std::vector<CameraControlSetting> camera;
    if (object.contains("camera")) {
        auto parsed = parseCamera(object.value("camera"));
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        camera = std::move(*parsed);
    }
    std::optional<EqualizerGains> equalizer;
    if (object.contains("equalizer")) {
        auto parsed = parseEqualizer(object.value("equalizer"));
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        equalizer = *parsed;
    }
    std::vector<ApplicationId> applications;
    if (object.contains("applications")) {
        auto parsed = parseApplications(object.value("applications"));
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        applications = std::move(*parsed);
    }
    auto profile = Profile::create(std::move(*name), std::move(camera), equalizer, std::move(applications));
    if (!profile) {
        return malformed(QString::fromStdString(profile.error().detail));
    }
    return profile;
}

QJsonObject toJson(const Profile& profile)
{
    QJsonObject object;
    object.insert("name", QString::fromStdString(profile.name().value()));
    if (!profile.camera().empty()) {
        QJsonArray camera;
        for (const CameraControlSetting& setting : profile.camera()) {
            camera.append(QJsonObject{{"id", static_cast<double>(setting.id)},
                                      {"name", QString::fromStdString(setting.name)},
                                      {"value", static_cast<double>(setting.value)}});
        }
        object.insert("camera", camera);
    }
    if (profile.equalizer()) {
        QJsonArray gains;
        for (const int gain : profile.equalizer()->values()) {
            gains.append(gain);
        }
        object.insert("equalizer", gains);
    }
    if (!profile.applications().empty()) {
        QJsonArray applications;
        for (const ApplicationId& application : profile.applications()) {
            applications.append(QString::fromStdString(application.value()));
        }
        object.insert("applications", applications);
    }
    return object;
}

} // namespace

JsonProfileRepository::JsonProfileRepository(QString filePath) : filePath_(std::move(filePath)) {}

QString JsonProfileRepository::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/ghub-linux/profiles.json";
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
        const QByteArray content = file.read(maximumFileBytes + 1);
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
        const auto version = exactInteger(root.value("version"), 0, 1'000'000);
        if (!version || (*version != 1 && *version != kVersion)) {
            return malformed("\"version\" não suportada (esperada 1 ou 2)");
        }
        const bool version2 = *version == kVersion;
        const std::set<QString> rootKeys =
            version2 ? std::set<QString>{"format", "version", "profiles", "automaticSwitching"}
                     : std::set<QString>{"format", "version", "profiles"};
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
            auto profile = parseProfile(entry, version2);
            if (!profile) {
                return std::unexpected(profile.error());
            }
            profiles.push_back(std::move(*profile));
        }
        auto library = ProfileLibrary::create(std::move(profiles));
        if (library) {
            library->setAutomaticSwitching(automaticSwitching);
        }
        return library;
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
            if (setting.value > kMaxExactInteger || setting.value < -kMaxExactInteger) {
                return shared_kernel::fail(ErrorCode::InvalidArgument,
                                           "valor de controle fora da faixa exata do JSON (2^53-1)");
            }
        }
        profiles.append(toJson(profile));
    }
    const QJsonObject root{{"format", kFormat},
                           {"version", kVersion},
                           {"automaticSwitching", library.automaticSwitching()},
                           {"profiles", profiles}};

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
