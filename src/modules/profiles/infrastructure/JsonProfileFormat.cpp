#include "modules/profiles/infrastructure/JsonProfileFormat.hpp"

#include <QJsonArray>

#include <cmath>
#include <limits>
#include <set>

namespace ghub::profiles::json_format {

namespace {

using shared_kernel::ErrorCode;

bool onlyKeys(const QJsonObject& object, const std::set<QString>& allowed)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!allowed.contains(it.key())) {
            return false;
        }
    }
    return true;
}

// Em modo de importação lista (e descarta) as chaves fora de `allowed`; no modo estrito, recusa.
// Devolve false quando deve recusar.
bool checkKeys(const QJsonObject& object, const std::set<QString>& allowed, const ParseOptions& options,
               const QString& where)
{
    if (options.ignored == nullptr) {
        return onlyKeys(object, allowed);
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!allowed.contains(it.key())) {
            options.ignored->push_back((where + it.key()).toStdString());
        }
    }
    return true;
}

shared_kernel::Result<std::vector<CameraControlSetting>> parseCamera(const QJsonValue& value,
                                                                     const ParseOptions& options)
{
    if (!value.isArray()) {
        return malformed(options.context, "\"camera\" deve ser uma lista");
    }
    std::vector<CameraControlSetting> camera;
    for (const QJsonValue& entry : value.toArray()) {
        if (!entry.isObject()) {
            return malformed(options.context, "item de \"camera\" deve ser um objeto");
        }
        const QJsonObject object = entry.toObject();
        if (!checkKeys(object, {"id", "name", "value"}, options, "camera[].")) {
            return malformed(options.context, "chave desconhecida em item de \"camera\"");
        }
        const auto id = exactInteger(object.value("id"), 0, std::numeric_limits<std::uint32_t>::max());
        const auto controlValue = exactInteger(object.value("value"), -kMaxExactInteger, kMaxExactInteger);
        if (!id || !controlValue || !object.value("name").isString()) {
            return malformed(options.context, "item de \"camera\" com id, name ou value inválido");
        }
        camera.push_back({.id = static_cast<std::uint32_t>(*id),
                          .name = object.value("name").toString().toStdString(),
                          .value = *controlValue});
    }
    return camera;
}

shared_kernel::Result<EqualizerGains> parseEqualizer(const QJsonValue& value, const ParseOptions& options)
{
    if (!value.isArray()) {
        return malformed(options.context, "\"equalizer\" deve ser uma lista");
    }
    std::vector<int> gains;
    for (const QJsonValue& entry : value.toArray()) {
        const auto gain = exactInteger(entry, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        if (!gain) {
            return malformed(options.context, "ganho do \"equalizer\" não é um inteiro válido");
        }
        gains.push_back(static_cast<int>(*gain));
    }
    auto created = EqualizerGains::create(gains);
    if (!created) {
        return malformed(options.context, QString::fromStdString(created.error().detail));
    }
    return *created;
}

shared_kernel::Result<std::vector<ApplicationId>> parseApplications(const QJsonValue& value,
                                                                    const ParseOptions& options)
{
    if (!value.isArray()) {
        return malformed(options.context, "\"applications\" deve ser uma lista");
    }
    std::vector<ApplicationId> applications;
    for (const QJsonValue& entry : value.toArray()) {
        if (!entry.isString()) {
            return malformed(options.context, "aplicativo deve ser texto");
        }
        auto application = ApplicationId::create(entry.toString().toStdString());
        if (!application) {
            return malformed(options.context, "aplicativo inválido");
        }
        applications.push_back(std::move(*application));
    }
    return applications;
}

} // namespace

std::unexpected<shared_kernel::DomainError> malformed(const QString& context, const QString& reason)
{
    return shared_kernel::fail(ErrorCode::MalformedData, (context + ": " + reason).toStdString());
}

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

shared_kernel::Result<Profile> parseProfile(const QJsonValue& value, const ParseOptions& options)
{
    if (!value.isObject()) {
        return malformed(options.context, "perfil deve ser um objeto");
    }
    const QJsonObject object = value.toObject();
    // Importação: "applications" é descartado e listado, mesmo que o formato o aceitasse.
    const bool readApplications = options.allowApplications && options.ignored == nullptr;
    const std::set<QString> allowedKeys = readApplications
                                              ? std::set<QString>{"name", "camera", "equalizer", "applications"}
                                              : std::set<QString>{"name", "camera", "equalizer"};
    if (!checkKeys(object, allowedKeys, options, "")) {
        return malformed(options.context, "chave desconhecida em perfil");
    }
    if (!object.value("name").isString()) {
        return malformed(options.context, "\"name\" do perfil deve ser texto");
    }
    auto name = ProfileName::create(object.value("name").toString().toStdString());
    if (!name) {
        return malformed(options.context, QString::fromStdString(name.error().detail));
    }
    std::vector<CameraControlSetting> camera;
    if (object.contains("camera")) {
        auto parsed = parseCamera(object.value("camera"), options);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        camera = std::move(*parsed);
    }
    std::optional<EqualizerGains> equalizer;
    if (object.contains("equalizer")) {
        auto parsed = parseEqualizer(object.value("equalizer"), options);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        equalizer = *parsed;
    }
    std::vector<ApplicationId> applications;
    if (readApplications && object.contains("applications")) {
        auto parsed = parseApplications(object.value("applications"), options);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        applications = std::move(*parsed);
    }
    auto profile = Profile::create(std::move(*name), std::move(camera), equalizer, std::move(applications));
    if (!profile) {
        return malformed(options.context, QString::fromStdString(profile.error().detail));
    }
    return profile;
}

QJsonObject toJson(const Profile& profile, bool withApplications)
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
    if (withApplications && !profile.applications().empty()) {
        QJsonArray applications;
        for (const ApplicationId& application : profile.applications()) {
            applications.append(QString::fromStdString(application.value()));
        }
        object.insert("applications", applications);
    }
    return object;
}

} // namespace ghub::profiles::json_format
