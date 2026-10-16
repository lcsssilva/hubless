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

constexpr std::int64_t kIntMin = std::numeric_limits<int>::min();
constexpr std::int64_t kIntMax = std::numeric_limits<int>::max();

std::optional<int> intField(const QJsonObject& object, const char* key)
{
    const auto value = exactInteger(object.value(key), kIntMin, kIntMax);
    return value ? std::optional<int>{static_cast<int>(*value)} : std::nullopt;
}

shared_kernel::Result<std::vector<HeadsetLightSetting>> parseHeadsetLights(const QJsonValue& value,
                                                                           const ParseOptions& options)
{
    if (!value.isArray()) {
        return malformed(options.context, "\"headsetLights\" deve ser uma lista");
    }
    std::vector<HeadsetLightSetting> lights;
    for (const QJsonValue& entry : value.toArray()) {
        if (!entry.isObject()) {
            return malformed(options.context, "item de \"headsetLights\" deve ser um objeto");
        }
        const QJsonObject object = entry.toObject();
        if (!checkKeys(object, {"zone", "effect", "red", "green", "blue", "periodMs", "brightness"}, options,
                       "headsetLights[].")) {
            return malformed(options.context, "chave desconhecida em item de \"headsetLights\"");
        }
        const auto zone = intField(object, "zone");
        const auto effect = intField(object, "effect");
        const auto red = intField(object, "red");
        const auto green = intField(object, "green");
        const auto blue = intField(object, "blue");
        const auto period = intField(object, "periodMs");
        const auto brightness = intField(object, "brightness");
        if (!zone || !effect || !red || !green || !blue || !period || !brightness) {
            return malformed(options.context, "item de \"headsetLights\" com campo ausente ou inválido");
        }
        lights.push_back({.zone = *zone, .effect = *effect, .red = *red, .green = *green, .blue = *blue,
                          .periodMs = *period, .brightness = *brightness});
    }
    return lights;
}

shared_kernel::Result<KeyboardEffectSetting> parseKeyboardEffect(const QJsonValue& value, const ParseOptions& options)
{
    if (!value.isObject()) {
        return malformed(options.context, "\"keyboardEffect\" deve ser um objeto");
    }
    const QJsonObject object = value.toObject();
    if (!checkKeys(object, {"kind", "red", "green", "blue", "brightness"}, options, "keyboardEffect.")) {
        return malformed(options.context, "chave desconhecida em \"keyboardEffect\"");
    }
    const auto red = intField(object, "red");
    const auto green = intField(object, "green");
    const auto blue = intField(object, "blue");
    const auto brightness = intField(object, "brightness");
    if (!object.value("kind").isString() || !red || !green || !blue || !brightness) {
        return malformed(options.context, "\"keyboardEffect\" com campo ausente ou inválido");
    }
    return KeyboardEffectSetting{.kind = object.value("kind").toString().toStdString(), .red = *red, .green = *green,
                                 .blue = *blue, .brightness = *brightness};
}

shared_kernel::Result<PointerDpiSetting> parsePointerDpi(const QJsonValue& value, const ParseOptions& options)
{
    if (!value.isObject()) {
        return malformed(options.context, "\"pointerDpi\" deve ser um objeto");
    }
    const QJsonObject object = value.toObject();
    if (!checkKeys(object, {"reportRateHz", "dpiStages", "defaultStage", "dpiShiftStage"}, options, "pointerDpi.")) {
        return malformed(options.context, "chave desconhecida em \"pointerDpi\"");
    }
    const auto rate = intField(object, "reportRateHz");
    const auto defaultStage = intField(object, "defaultStage");
    if (!rate || !defaultStage || !object.value("dpiStages").isArray()) {
        return malformed(options.context, "\"pointerDpi\" com campo ausente ou inválido");
    }
    PointerDpiSetting setting{.reportRateHz = *rate, .dpiStages = {}, .defaultStage = *defaultStage,
                              .dpiShiftStage = std::nullopt};
    for (const QJsonValue& stage : object.value("dpiStages").toArray()) {
        const auto dpi = exactInteger(stage, kIntMin, kIntMax);
        if (!dpi) {
            return malformed(options.context, "estágio de DPI não é um inteiro válido");
        }
        setting.dpiStages.push_back(static_cast<int>(*dpi));
    }
    if (object.contains("dpiShiftStage")) {
        const auto shift = intField(object, "dpiShiftStage");
        if (!shift) {
            return malformed(options.context, "\"dpiShiftStage\" não é um inteiro válido");
        }
        setting.dpiShiftStage = *shift;
    }
    return setting;
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
    std::set<QString> allowedKeys{"name", "camera", "equalizer"};
    if (readApplications) {
        allowedKeys.insert("applications");
    }
    if (options.allowHardware) {
        allowedKeys.insert({"sidetone", "headsetLights", "keyboardEffect", "pointerDpi"});
    }
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
    HardwareSections hardware;
    if (options.allowHardware) {
        if (object.contains("sidetone")) {
            const auto level = intField(object, "sidetone");
            if (!level) {
                return malformed(options.context, "\"sidetone\" não é um inteiro válido");
            }
            hardware.sidetone = *level;
        }
        if (object.contains("headsetLights")) {
            auto parsed = parseHeadsetLights(object.value("headsetLights"), options);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            hardware.headsetLights = std::move(*parsed);
        }
        if (object.contains("keyboardEffect")) {
            auto parsed = parseKeyboardEffect(object.value("keyboardEffect"), options);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            hardware.keyboardEffect = std::move(*parsed);
        }
        if (object.contains("pointerDpi")) {
            auto parsed = parsePointerDpi(object.value("pointerDpi"), options);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            hardware.pointerDpi = std::move(*parsed);
        }
    }
    auto profile =
        Profile::create(std::move(*name), std::move(camera), equalizer, std::move(applications), std::move(hardware));
    if (!profile) {
        return malformed(options.context, QString::fromStdString(profile.error().detail));
    }
    return profile;
}

QString sectionKey(ProfileSection section)
{
    switch (section) {
    case ProfileSection::Camera:
        return QStringLiteral("camera");
    case ProfileSection::Equalizer:
        return QStringLiteral("equalizer");
    case ProfileSection::Sidetone:
        return QStringLiteral("sidetone");
    case ProfileSection::HeadsetLights:
        return QStringLiteral("headsetLights");
    case ProfileSection::KeyboardEffect:
        return QStringLiteral("keyboardEffect");
    case ProfileSection::PointerDpi:
        return QStringLiteral("pointerDpi");
    }
    return {};
}

std::optional<ProfileSection> sectionFromKey(const QString& key, bool hardwareSections)
{
    for (std::size_t i = 0; i < profileSectionCount; ++i) {
        const auto section = static_cast<ProfileSection>(i);
        const bool hardware = section != ProfileSection::Camera && section != ProfileSection::Equalizer;
        if (sectionKey(section) == key && (!hardware || hardwareSections)) {
            return section;
        }
    }
    return std::nullopt;
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
    const HardwareSections& hardware = profile.hardware();
    if (hardware.sidetone.has_value()) {
        object.insert("sidetone", *hardware.sidetone);
    }
    if (!hardware.headsetLights.empty()) {
        QJsonArray lights;
        for (const HeadsetLightSetting& light : hardware.headsetLights) {
            lights.append(QJsonObject{{"zone", light.zone},
                                      {"effect", light.effect},
                                      {"red", light.red},
                                      {"green", light.green},
                                      {"blue", light.blue},
                                      {"periodMs", light.periodMs},
                                      {"brightness", light.brightness}});
        }
        object.insert("headsetLights", lights);
    }
    if (hardware.keyboardEffect.has_value()) {
        const KeyboardEffectSetting& effect = *hardware.keyboardEffect;
        object.insert("keyboardEffect", QJsonObject{{"kind", QString::fromStdString(effect.kind)},
                                                    {"red", effect.red},
                                                    {"green", effect.green},
                                                    {"blue", effect.blue},
                                                    {"brightness", effect.brightness}});
    }
    if (hardware.pointerDpi.has_value()) {
        const PointerDpiSetting& dpi = *hardware.pointerDpi;
        QJsonArray stages;
        for (const int stage : dpi.dpiStages) {
            stages.append(stage);
        }
        QJsonObject pointer{{"reportRateHz", dpi.reportRateHz}, {"dpiStages", stages}, {"defaultStage", dpi.defaultStage}};
        if (dpi.dpiShiftStage.has_value()) {
            pointer.insert("dpiShiftStage", *dpi.dpiShiftStage);
        }
        object.insert("pointerDpi", pointer);
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
