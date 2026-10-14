#include "modules/pointer/infrastructure/RatbagPointerBackend.hpp"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QList>
#include <QMetaType>
#include <QVariant>
#include <QVariantMap>

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ghub::pointer {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;
using shared_kernel::Result;

constexpr int supportedApiVersion = 2;
constexpr std::size_t maxDevices = 16;
constexpr std::size_t maxProfiles = 16;
constexpr std::size_t maxItemsPerProfile = 64; // estágios, botões e LEDs
constexpr std::size_t maxListEntries = 1024;   // DPIs, taxas, modos

constexpr std::uint32_t buttonTypeNone = 0;
constexpr std::uint32_t buttonTypeButton = 1;
constexpr std::uint32_t buttonTypeSpecial = 2;
constexpr std::uint32_t buttonTypeKey = 3;
constexpr std::uint32_t buttonTypeMacro = 4;
constexpr std::uint32_t buttonTypeUnknown = 1000;

constexpr QLatin1StringView propertiesInterface{"org.freedesktop.DBus.Properties"};

std::string text(const QString& value)
{
    return value.toStdString();
}

// Remove os `v` aninhados (propriedade do tipo `v` dentro de GetAll).
QVariant unwrap(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
        value = value.value<QDBusVariant>().variant();
    }
    return value;
}

// Propriedades de um objeto (resultado de GetAll) com leitura tipada; `what` identifica o objeto nos erros.
class Properties {
public:
    Properties(QVariantMap values, QString what) : values_(std::move(values)), what_(std::move(what)) {}

    [[nodiscard]] Result<std::uint32_t> uintValue(const QString& key) const
    {
        const auto value = find(key);
        if (!value.has_value()) {
            return fail(ErrorCode::MalformedData, missing(key));
        }
        return asUint(*value, key);
    }

    [[nodiscard]] Result<bool> boolValue(const QString& key) const
    {
        const auto value = find(key);
        if (!value.has_value()) {
            return fail(ErrorCode::MalformedData, missing(key));
        }
        if (value->metaType() != QMetaType::fromType<bool>()) {
            return fail(ErrorCode::MalformedData, wrongType(key));
        }
        return value->toBool();
    }

    [[nodiscard]] Result<QString> stringValue(const QString& key) const
    {
        const auto value = find(key);
        if (!value.has_value()) {
            return fail(ErrorCode::MalformedData, missing(key));
        }
        if (value->metaType() != QMetaType::fromType<QString>()) {
            return fail(ErrorCode::MalformedData, wrongType(key));
        }
        return value->toString();
    }

    [[nodiscard]] Result<std::vector<std::uint32_t>> uintList(const QString& key) const
    {
        const auto value = find(key);
        if (!value.has_value()) {
            return fail(ErrorCode::MalformedData, missing(key));
        }
        if (!isArgument(*value, "au")) {
            return fail(ErrorCode::MalformedData, wrongType(key));
        }
        const auto argument = value->value<QDBusArgument>();
        std::vector<std::uint32_t> list;
        bool tooLong = false;
        argument.beginArray();
        while (!argument.atEnd()) {
            if (list.size() >= maxListEntries) {
                tooLong = true;
                break;
            }
            uint entry = 0;
            argument >> entry;
            list.push_back(entry);
        }
        // endArray também no excesso: sem ele o demarshaller aninhado do Qt vaza (visto no ASan).
        argument.endArray();
        if (tooLong) {
            return fail(ErrorCode::MalformedData, what() + "." + text(key) + ": lista com mais de 1024 itens");
        }
        return list;
    }

    [[nodiscard]] Result<std::vector<QString>> pathList(const QString& key, std::size_t limit) const
    {
        const auto value = find(key);
        if (!value.has_value()) {
            return fail(ErrorCode::MalformedData, missing(key));
        }
        if (!isArgument(*value, "ao")) {
            return fail(ErrorCode::MalformedData, wrongType(key));
        }
        const auto argument = value->value<QDBusArgument>();
        std::vector<QString> paths;
        bool tooLong = false;
        argument.beginArray();
        while (!argument.atEnd()) {
            if (paths.size() >= limit) {
                tooLong = true;
                break;
            }
            QDBusObjectPath path;
            argument >> path;
            paths.push_back(path.path());
        }
        argument.endArray();
        if (tooLong) {
            return fail(ErrorCode::MalformedData,
                        what() + "." + text(key) + ": mais de " + std::to_string(limit) + " itens");
        }
        return paths;
    }

    // Valor já desembrulhado, para propriedades de tipo composto (`v`, `(uv)`, `(uuu)`).
    [[nodiscard]] Result<QVariant> raw(const QString& key) const
    {
        const auto value = find(key);
        if (!value.has_value()) {
            return fail(ErrorCode::MalformedData, missing(key));
        }
        return *value;
    }

    [[nodiscard]] std::string what() const { return text(what_); }
    [[nodiscard]] std::string wrongType(const QString& key) const
    {
        return what() + "." + text(key) + ": tipo D-Bus inesperado";
    }

    [[nodiscard]] static Result<std::uint32_t> asUint(const QVariant& value, const QString& label)
    {
        if (value.metaType() != QMetaType::fromType<uint>()) {
            return fail(ErrorCode::MalformedData, text(label) + ": tipo D-Bus inesperado");
        }
        return value.toUInt();
    }

    // O valor é um QDBusArgument com a assinatura esperada. Quem lê cria a cópia local de leitura
    // (`value.value<QDBusArgument>()`) no próprio escopo: copiar o QDBusArgument adiante perde o estado.
    [[nodiscard]] static bool isArgument(const QVariant& value, const char* signature)
    {
        return value.metaType() == QMetaType::fromType<QDBusArgument>() &&
               value.value<QDBusArgument>().currentSignature() == QLatin1String(signature);
    }

private:
    [[nodiscard]] std::optional<QVariant> find(const QString& key) const
    {
        const auto found = values_.constFind(key);
        if (found == values_.cend()) {
            return std::nullopt;
        }
        return unwrap(*found);
    }

    [[nodiscard]] std::string missing(const QString& key) const { return what() + "." + text(key) + ": ausente"; }

    QVariantMap values_;
    QString what_;
};

// Converte um erro de domínio (valor fora das regras) em dado inesperado do serviço.
template <typename T> Result<T> asMalformed(Result<T> result, const std::string& what)
{
    if (result.has_value()) {
        return result;
    }
    return fail(ErrorCode::MalformedData, what + ": " + result.error().detail);
}

class Reader {
public:
    Reader(QDBusConnection connection, QString service, std::chrono::milliseconds timeout)
        : connection_(std::move(connection)), service_(std::move(service)),
          timeoutMs_(static_cast<int>(timeout.count()))
    {
    }

    Result<std::vector<DiscoveredPointer>> readAll()
    {
        const auto version =
            getProperty(ratbagRoot(), QStringLiteral("org.freedesktop.ratbag1.Manager"), QStringLiteral("APIVersion"));
        if (!version.has_value()) {
            return fail(version.error().code, version.error().detail);
        }
        if (unwrap(*version).metaType() != QMetaType::fromType<int>()) {
            return fail(ErrorCode::MalformedData, "Manager.APIVersion: tipo D-Bus inesperado");
        }
        if (unwrap(*version).toInt() != supportedApiVersion) {
            return fail(ErrorCode::BackendUnavailable,
                        "ratbagd: APIVersion " + std::to_string(unwrap(*version).toInt()) + " não suportada");
        }
        const auto devices =
            getProperty(ratbagRoot(), QStringLiteral("org.freedesktop.ratbag1.Manager"), QStringLiteral("Devices"));
        if (!devices.has_value()) {
            return fail(devices.error().code, devices.error().detail);
        }
        auto paths = Properties{QVariantMap{{QStringLiteral("Devices"), *devices}}, QStringLiteral("Manager")}.pathList(
            QStringLiteral("Devices"), maxDevices);
        if (!paths.has_value()) {
            return fail(paths.error().code, paths.error().detail);
        }
        std::vector<DiscoveredPointer> found;
        for (const QString& path : *paths) {
            auto device = readDevice(path);
            if (device.has_value()) {
                found.push_back(std::move(*device));
                continue;
            }
            if (device.error().code == ErrorCode::BackendUnavailable) {
                return fail(device.error().code, device.error().detail);
            }
            found.push_back({
                .usbId = std::nullopt,
                .name = text(path),
                .configuration = fail(device.error().code, device.error().detail),
            });
        }
        return found;
    }

private:
    static QString ratbagRoot() { return QStringLiteral("/org/freedesktop/ratbag1"); }

    // Dados do dispositivo: erro BackendUnavailable aborta a leitura; o resto fica no dispositivo.
    Result<DiscoveredPointer> readDevice(const QString& path)
    {
        const auto deviceProps = getAll(path, QStringLiteral("org.freedesktop.ratbag1.Device"), "Device");
        if (!deviceProps.has_value()) {
            return fail(deviceProps.error().code, deviceProps.error().detail);
        }
        const auto model = deviceProps->stringValue(QStringLiteral("Model"));
        const auto name = deviceProps->stringValue(QStringLiteral("Name"));
        if (!model.has_value() || !name.has_value()) {
            return fail(ErrorCode::MalformedData, (model.has_value() ? name : model).error().detail);
        }
        DiscoveredPointer pointer{
            .usbId = parseUsbModel(*model),
            .name = text(*name),
            .configuration = readConfiguration(*deviceProps, *name),
        };
        if (!pointer.configuration.has_value() && pointer.configuration.error().code == ErrorCode::BackendUnavailable) {
            return fail(pointer.configuration.error().code, pointer.configuration.error().detail);
        }
        return pointer;
    }

    Result<PointerConfiguration> readConfiguration(const Properties& deviceProps, const QString& name)
    {
        const auto profilePaths = deviceProps.pathList(QStringLiteral("Profiles"), maxProfiles);
        if (!profilePaths.has_value()) {
            return fail(profilePaths.error().code, profilePaths.error().detail);
        }
        std::vector<PointerProfile> profiles;
        for (const QString& path : *profilePaths) {
            auto profile = readProfile(path);
            if (!profile.has_value()) {
                return fail(profile.error().code, profile.error().detail);
            }
            profiles.push_back(std::move(*profile));
        }
        return asMalformed(PointerConfiguration::create(text(name), std::move(profiles)), "configuração");
    }

    Result<PointerProfile> readProfile(const QString& path)
    {
        const auto props = getAll(path, QStringLiteral("org.freedesktop.ratbag1.Profile"), "Profile");
        if (!props.has_value()) {
            return fail(props.error().code, props.error().detail);
        }
        const auto index = props->uintValue(QStringLiteral("Index"));
        const auto name = props->stringValue(QStringLiteral("Name"));
        const auto disabled = props->boolValue(QStringLiteral("Disabled"));
        const auto active = props->boolValue(QStringLiteral("IsActive"));
        const auto rate = props->uintValue(QStringLiteral("ReportRate"));
        const auto rates = props->uintList(QStringLiteral("ReportRates"));
        const auto resolutionPaths = props->pathList(QStringLiteral("Resolutions"), maxItemsPerProfile);
        const auto buttonPaths = props->pathList(QStringLiteral("Buttons"), maxItemsPerProfile);
        const auto ledPaths = props->pathList(QStringLiteral("Leds"), maxItemsPerProfile);
        for (const auto* field : {&index, &rate}) {
            if (!field->has_value()) {
                return fail(field->error().code, field->error().detail);
            }
        }
        for (const auto* field : {&disabled, &active}) {
            if (!field->has_value()) {
                return fail(field->error().code, field->error().detail);
            }
        }
        if (!name.has_value()) {
            return fail(name.error().code, name.error().detail);
        }
        if (!rates.has_value()) {
            return fail(rates.error().code, rates.error().detail);
        }
        if (!resolutionPaths.has_value()) {
            return fail(resolutionPaths.error().code, resolutionPaths.error().detail);
        }
        if (!buttonPaths.has_value()) {
            return fail(buttonPaths.error().code, buttonPaths.error().detail);
        }
        if (!ledPaths.has_value()) {
            return fail(ledPaths.error().code, ledPaths.error().detail);
        }

        PointerProfileSpec spec{
            .index = *index,
            .name = text(*name),
            .active = *active,
            .enabled = !*disabled,
            .reportRate = *rate,
            .supportedReportRates = *rates,
            .stages = {},
            .buttons = {},
            .leds = {},
        };
        for (const QString& resolutionPath : *resolutionPaths) {
            auto stage = readStage(resolutionPath);
            if (!stage.has_value()) {
                return fail(stage.error().code, stage.error().detail);
            }
            spec.stages.push_back(std::move(*stage));
        }
        for (const QString& buttonPath : *buttonPaths) {
            auto button = readButton(buttonPath);
            if (!button.has_value()) {
                return fail(button.error().code, button.error().detail);
            }
            spec.buttons.push_back(std::move(*button));
        }
        for (const QString& ledPath : *ledPaths) {
            auto led = readLed(ledPath);
            if (!led.has_value()) {
                return fail(led.error().code, led.error().detail);
            }
            spec.leds.push_back(std::move(*led));
        }
        return asMalformed(PointerProfile::create(std::move(spec)), "perfil " + std::to_string(*index));
    }

    Result<ResolutionStage> readStage(const QString& path)
    {
        const auto props = getAll(path, QStringLiteral("org.freedesktop.ratbag1.Resolution"), "Resolution");
        if (!props.has_value()) {
            return fail(props.error().code, props.error().detail);
        }
        const auto index = props->uintValue(QStringLiteral("Index"));
        const auto active = props->boolValue(QStringLiteral("IsActive"));
        const auto isDefault = props->boolValue(QStringLiteral("IsDefault"));
        const auto disabled = props->boolValue(QStringLiteral("IsDisabled"));
        const auto supported = props->uintList(QStringLiteral("Resolutions"));
        const auto value = props->raw(QStringLiteral("Resolution"));
        if (!index.has_value()) {
            return fail(index.error().code, index.error().detail);
        }
        if (!active.has_value()) {
            return fail(active.error().code, active.error().detail);
        }
        if (!isDefault.has_value()) {
            return fail(isDefault.error().code, isDefault.error().detail);
        }
        if (!disabled.has_value()) {
            return fail(disabled.error().code, disabled.error().detail);
        }
        if (!supported.has_value()) {
            return fail(supported.error().code, supported.error().detail);
        }
        if (!value.has_value()) {
            return fail(value.error().code, value.error().detail);
        }

        ResolutionStageSpec spec{
            .index = *index,
            .dpiX = 0,
            .dpiY = std::nullopt,
            .active = *active,
            .isDefault = *isDefault,
            .disabled = *disabled,
            .supportedDpis = *supported,
        };
        if (value->metaType() == QMetaType::fromType<uint>()) {
            spec.dpiX = value->toUInt();
        } else if (Properties::isArgument(*value, "(uu)")) {
            const auto pair = value->value<QDBusArgument>();
            uint x = 0;
            uint y = 0;
            pair.beginStructure();
            pair >> x >> y;
            pair.endStructure();
            spec.dpiX = x;
            spec.dpiY = y;
        } else {
            return fail(ErrorCode::MalformedData, props->wrongType(QStringLiteral("Resolution")));
        }
        return asMalformed(ResolutionStage::create(std::move(spec)), "estágio " + std::to_string(*index));
    }

    Result<ButtonMapping> readButton(const QString& path)
    {
        const auto props = getAll(path, QStringLiteral("org.freedesktop.ratbag1.Button"), "Button");
        if (!props.has_value()) {
            return fail(props.error().code, props.error().detail);
        }
        const auto index = props->uintValue(QStringLiteral("Index"));
        const auto types = props->uintList(QStringLiteral("ActionTypes"));
        const auto mapping = props->raw(QStringLiteral("Mapping"));
        if (!index.has_value()) {
            return fail(index.error().code, index.error().detail);
        }
        if (!types.has_value()) {
            return fail(types.error().code, types.error().detail);
        }
        if (!mapping.has_value()) {
            return fail(mapping.error().code, mapping.error().detail);
        }
        auto action = readAction(*mapping, *props);
        if (!action.has_value()) {
            return fail(action.error().code, action.error().detail);
        }
        ButtonMappingSpec spec{.index = *index, .action = *action, .supportedKinds = {}};
        for (const std::uint32_t type : *types) {
            const auto kind = kindOf(type);
            if (type == buttonTypeUnknown) {
                continue; // o ratbagd não anuncia 1000 como suportado; ignora se vier
            }
            if (!kind.has_value()) {
                return fail(ErrorCode::MalformedData, props->what() + ".ActionTypes: tipo " + std::to_string(type));
            }
            spec.supportedKinds.push_back(*kind);
        }
        return asMalformed(ButtonMapping::create(std::move(spec)), "botão " + std::to_string(*index));
    }

    static std::optional<ButtonActionKind> kindOf(std::uint32_t type)
    {
        switch (type) {
        case buttonTypeNone:
            return ButtonActionKind::None;
        case buttonTypeButton:
            return ButtonActionKind::Button;
        case buttonTypeSpecial:
            return ButtonActionKind::Special;
        case buttonTypeKey:
            return ButtonActionKind::Key;
        case buttonTypeMacro:
            return ButtonActionKind::Macro;
        default:
            return std::nullopt;
        }
    }

    // `Mapping` é `(uv)`: tipo + valor (u; para macro, `a(uu)` — guarda só o número de passos).
    static Result<ButtonAction> readAction(const QVariant& mapping, const Properties& props)
    {
        if (!Properties::isArgument(mapping, "(uv)")) {
            return fail(ErrorCode::MalformedData, props.wrongType(QStringLiteral("Mapping")));
        }
        const auto argument = mapping.value<QDBusArgument>();
        uint type = 0;
        QDBusVariant content;
        argument.beginStructure();
        argument >> type >> content;
        argument.endStructure();
        const QVariant value = unwrap(content.variant());

        if (type == buttonTypeMacro) {
            if (!Properties::isArgument(value, "a(uu)")) {
                return fail(ErrorCode::MalformedData, props.wrongType(QStringLiteral("Mapping")));
            }
            const auto steps = value.value<QDBusArgument>();
            std::uint32_t count = 0;
            bool tooLong = false;
            steps.beginArray();
            while (!steps.atEnd()) {
                if (count >= maxListEntries) {
                    tooLong = true;
                    break;
                }
                uint kind = 0;
                uint stepValue = 0;
                steps.beginStructure();
                steps >> kind >> stepValue;
                steps.endStructure();
                ++count;
            }
            steps.endArray();
            if (tooLong) {
                return fail(ErrorCode::MalformedData, props.what() + ".Mapping: macro com mais de 1024 passos");
            }
            return ButtonAction::macro(count);
        }
        if (type == buttonTypeUnknown) {
            return ButtonAction::unknown();
        }
        const auto number = Properties::asUint(value, QStringLiteral("Mapping"));
        if (!number.has_value()) {
            return fail(number.error().code, props.what() + ".Mapping: tipo D-Bus inesperado");
        }
        switch (type) {
        case buttonTypeNone:
            return ButtonAction::none();
        case buttonTypeButton:
            return ButtonAction::button(*number);
        case buttonTypeSpecial:
            return ButtonAction::special(*number);
        case buttonTypeKey:
            return ButtonAction::key(*number);
        default:
            return fail(ErrorCode::MalformedData, props.what() + ".Mapping: tipo " + std::to_string(type));
        }
    }

    Result<LedState> readLed(const QString& path)
    {
        const auto props = getAll(path, QStringLiteral("org.freedesktop.ratbag1.Led"), "Led");
        if (!props.has_value()) {
            return fail(props.error().code, props.error().detail);
        }
        const auto index = props->uintValue(QStringLiteral("Index"));
        const auto modes = props->uintList(QStringLiteral("Modes"));
        const auto mode = props->uintValue(QStringLiteral("Mode"));
        const auto depth = props->uintValue(QStringLiteral("ColorDepth"));
        const auto duration = props->uintValue(QStringLiteral("EffectDuration"));
        const auto brightness = props->uintValue(QStringLiteral("Brightness"));
        const auto color = props->raw(QStringLiteral("Color"));
        for (const auto* field : {&index, &mode, &depth, &duration, &brightness}) {
            if (!field->has_value()) {
                return fail(field->error().code, field->error().detail);
            }
        }
        if (!modes.has_value()) {
            return fail(modes.error().code, modes.error().detail);
        }
        if (!color.has_value()) {
            return fail(color.error().code, color.error().detail);
        }
        if (!Properties::isArgument(*color, "(uuu)")) {
            return fail(ErrorCode::MalformedData, props->wrongType(QStringLiteral("Color")));
        }
        const auto triple = color->value<QDBusArgument>();
        uint red = 0;
        uint green = 0;
        uint blue = 0;
        triple.beginStructure();
        triple >> red >> green >> blue;
        triple.endStructure();

        LedStateSpec spec{
            .index = *index,
            .mode = ledModeFromCode(*mode),
            .rawMode = *mode,
            .red = red,
            .green = green,
            .blue = blue,
            .colorDepth = *depth,
            .brightness = *brightness,
            .effectDuration = *duration,
            .supportedModes = {},
        };
        for (const std::uint32_t code : *modes) {
            const LedMode known = ledModeFromCode(code);
            if (known != LedMode::Unknown) {
                spec.supportedModes.push_back(known); // códigos desconhecidos não entram na lista
            }
        }
        return asMalformed(LedState::create(std::move(spec)), "LED " + std::to_string(*index));
    }

    // `usb:VVVV:PPPP:N` (hex) → UsbId; qualquer outro formato ou barramento → vazio.
    static std::optional<device_inventory::UsbId> parseUsbModel(const QString& model)
    {
        const std::string raw = model.toStdString();
        const std::string_view view{raw};
        constexpr std::string_view prefix = "usb:";
        if (!view.starts_with(prefix)) {
            return std::nullopt;
        }
        const auto rest = view.substr(prefix.size());
        const auto firstColon = rest.find(':');
        const auto secondColon = firstColon == std::string_view::npos ? firstColon : rest.find(':', firstColon + 1);
        if (secondColon == std::string_view::npos) {
            return std::nullopt;
        }
        const auto vendor = parseHex16(rest.substr(0, firstColon));
        const auto product = parseHex16(rest.substr(firstColon + 1, secondColon - firstColon - 1));
        if (!vendor.has_value() || !product.has_value()) {
            return std::nullopt;
        }
        return device_inventory::UsbId{.vendorId = *vendor, .productId = *product};
    }

    static std::optional<std::uint16_t> parseHex16(std::string_view digits)
    {
        constexpr std::size_t maxDigits = 4;
        if (digits.empty() || digits.size() > maxDigits) {
            return std::nullopt;
        }
        std::uint16_t value = 0;
        const auto* const first = digits.data();
        const auto* const last = first + digits.size(); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const auto [end, error] = std::from_chars(first, last, value, 16);
        if (error != std::errc{} || end != last) {
            return std::nullopt;
        }
        return value;
    }

    // Properties.Get: devolve o valor da propriedade (ainda embrulhado em `v`).
    Result<QVariant> getProperty(const QString& path, const QString& interface, const QString& name)
    {
        auto message = QDBusMessage::createMethodCall(service_, path, propertiesInterface, QStringLiteral("Get"));
        message.setArguments({interface, name});
        auto reply = send(message, path);
        if (!reply.has_value()) {
            return fail(reply.error().code, reply.error().detail);
        }
        return reply->arguments().at(0);
    }

    // Properties.GetAll de uma interface.
    Result<Properties> getAll(const QString& path, const QString& interface, const char* label)
    {
        auto message = QDBusMessage::createMethodCall(service_, path, propertiesInterface, QStringLiteral("GetAll"));
        message.setArguments({interface});
        auto reply = send(message, path);
        if (!reply.has_value()) {
            return fail(reply.error().code, reply.error().detail);
        }
        if (!Properties::isArgument(reply->arguments().at(0), "a{sv}")) {
            return fail(ErrorCode::MalformedData, std::string{label} + ": GetAll sem a{sv}");
        }
        return Properties{qdbus_cast<QVariantMap>(reply->arguments().at(0)), QString::fromUtf8(label)};
    }

    Result<QDBusMessage> send(const QDBusMessage& message, const QString& path)
    {
        auto reply = connection_.call(message, QDBus::Block, timeoutMs_);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            return fail(ErrorCode::BackendUnavailable,
                        text(reply.errorName() + QStringLiteral(": ") + reply.errorMessage()) + " (" + text(path) +
                            ")");
        }
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1) {
            return fail(ErrorCode::MalformedData, "resposta D-Bus inesperada de " + text(path));
        }
        return reply;
    }

    QDBusConnection connection_;
    QString service_;
    int timeoutMs_;
};

} // namespace

RatbagPointerBackend::RatbagPointerBackend(QDBusConnection connection, QString service,
                                           std::chrono::milliseconds callTimeout)
    : connection_(std::move(connection)), service_(std::move(service)), callTimeout_(callTimeout)
{
}

Result<std::vector<DiscoveredPointer>> RatbagPointerBackend::readDevices()
{
    return Reader{connection_, service_, callTimeout_}.readAll();
}

} // namespace ghub::pointer
