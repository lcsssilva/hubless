#include "modules/lighting/presentation/G915OnboardController.hpp"

#include <QByteArray>
#include <QDebug>
#include <QRegularExpression>
#include <QtConcurrent>

#include <array>
#include <cstring>
#include <expected>
#include <utility>

namespace ghub::lighting::presentation {

namespace {

using domain::G915KeyCatalog;
using domain::GKeyBinding;
using domain::KeyboardEffect;
using domain::KeyboardEffectKind;

constexpr std::uint8_t kKeyboardZone = 0x01;
constexpr char kKeyPrefix[] = "Key: ";

QString colorText(const domain::RgbColor& color)
{
    return QStringLiteral("#%1%2%3")
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'));
}

QVariantMap effectMap(const std::optional<KeyboardEffect>& effect)
{
    QVariantMap map{{QStringLiteral("kind"), QStringLiteral("raw")},
                    {QStringLiteral("color"), QStringLiteral("#000000")},
                    {QStringLiteral("periodMs"), 0},
                    {QStringLiteral("brightness"), 0},
                    {QStringLiteral("speed"), 0}};
    if (!effect.has_value()) {
        return map;
    }
    switch (effect->kind()) {
    case KeyboardEffectKind::Off:
        map[QStringLiteral("kind")] = QStringLiteral("off");
        break;
    case KeyboardEffectKind::Static:
        map[QStringLiteral("kind")] = QStringLiteral("static");
        break;
    case KeyboardEffectKind::Breathing:
        map[QStringLiteral("kind")] = QStringLiteral("breathing");
        break;
    case KeyboardEffectKind::Cycle:
        map[QStringLiteral("kind")] = QStringLiteral("cycle");
        break;
    case KeyboardEffectKind::Wave:
        map[QStringLiteral("kind")] = QStringLiteral("wave");
        break;
    case KeyboardEffectKind::Ripple:
        map[QStringLiteral("kind")] = QStringLiteral("ripple");
        break;
    }
    if (const auto color = effect->color(); color.has_value()) {
        map[QStringLiteral("color")] = colorText(*color);
    }
    if (const auto period = effect->period(); period.has_value()) {
        map[QStringLiteral("periodMs")] = static_cast<int>(period->milliseconds());
    }
    if (const auto brightness = effect->brightness(); brightness.has_value()) {
        map[QStringLiteral("brightness")] = static_cast<int>(brightness->percent());
    }
    if (const auto speed = effect->rippleSpeed(); speed.has_value()) {
        map[QStringLiteral("speed")] = static_cast<int>(speed->value());
    }
    return map;
}

std::expected<domain::RgbColor, QString> colorFrom(const QVariantMap& map)
{
    const QString color = map.value(QStringLiteral("color")).toString();
    static const QRegularExpression pattern{QStringLiteral("^#[0-9a-fA-F]{6}$")};
    if (!pattern.match(color).hasMatch()) {
        return std::unexpected(QObject::tr("cor deve ser #rrggbb"));
    }
    const auto channel = [&color](int at) { return static_cast<std::uint8_t>(color.mid(at, 2).toUInt(nullptr, 16)); };
    return domain::RgbColor{channel(1), channel(3), channel(5)};
}

std::expected<domain::EffectPeriod, QString> periodFrom(const QVariantMap& map)
{
    bool ok = false;
    const int value = map.value(QStringLiteral("periodMs")).toInt(&ok);
    if (!ok || value < 0) {
        return std::unexpected(QObject::tr("período inválido"));
    }
    auto period = domain::EffectPeriod::create(static_cast<std::uint32_t>(value));
    if (!period.has_value()) {
        return std::unexpected(QString::fromStdString(period.error().detail));
    }
    return *period;
}

std::expected<domain::EffectBrightness, QString> brightnessFrom(const QVariantMap& map)
{
    bool ok = false;
    const int value = map.value(QStringLiteral("brightness")).toInt(&ok);
    if (!ok || value < 0) {
        return std::unexpected(QObject::tr("brilho inválido"));
    }
    auto brightness = domain::EffectBrightness::create(static_cast<std::uint32_t>(value));
    if (!brightness.has_value()) {
        return std::unexpected(QString::fromStdString(brightness.error().detail));
    }
    return *brightness;
}

// Mapa do QML -> efeito; "raw" = sem alteração (nullopt). Texto de erro no inesperado.
std::expected<std::optional<KeyboardEffect>, QString> effectFrom(const QVariantMap& map)
{
    const QString kind = map.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("raw")) {
        return std::optional<KeyboardEffect>{};
    }
    if (kind == QLatin1String("off")) {
        return std::optional<KeyboardEffect>{KeyboardEffect::off()};
    }
    if (kind == QLatin1String("static") || kind == QLatin1String("breathing") || kind == QLatin1String("ripple")) {
        const auto color = colorFrom(map);
        if (!color.has_value()) {
            return std::unexpected(color.error());
        }
        if (kind == QLatin1String("static")) {
            return std::optional<KeyboardEffect>{KeyboardEffect::staticColor(*color)};
        }
        if (kind == QLatin1String("ripple")) {
            bool ok = false;
            const int value = map.value(QStringLiteral("speed")).toInt(&ok);
            auto speed = domain::RippleSpeed::create(ok && value >= 0 ? static_cast<std::uint32_t>(value) : 0);
            if (!speed.has_value()) {
                return std::unexpected(QString::fromStdString(speed.error().detail));
            }
            return std::optional<KeyboardEffect>{KeyboardEffect::ripple(*color, *speed)};
        }
        const auto period = periodFrom(map);
        const auto brightness = brightnessFrom(map);
        if (!period.has_value() || !brightness.has_value()) {
            return std::unexpected(!period.has_value() ? period.error() : brightness.error());
        }
        return std::optional<KeyboardEffect>{KeyboardEffect::breathing(*color, *period, *brightness)};
    }
    if (kind == QLatin1String("cycle") || kind == QLatin1String("wave")) {
        const auto period = periodFrom(map);
        const auto brightness = brightnessFrom(map);
        if (!period.has_value() || !brightness.has_value()) {
            return std::unexpected(!period.has_value() ? period.error() : brightness.error());
        }
        return std::optional<KeyboardEffect>{kind == QLatin1String("cycle")
                                                 ? KeyboardEffect::cycle(*period, *brightness)
                                                 : KeyboardEffect::wave(*period, *brightness)};
    }
    return std::unexpected(QObject::tr("tipo de efeito desconhecido"));
}

QString keyName(const domain::KeyId& key)
{
    const QString full = QString::fromUtf8(G915KeyCatalog::definition(key).name.data(),
                                           static_cast<qsizetype>(G915KeyCatalog::definition(key).name.size()));
    return full.startsWith(QLatin1String(kKeyPrefix)) ? full.mid(static_cast<qsizetype>(sizeof(kKeyPrefix)) - 1)
                                                      : full;
}

QVariantMap gKeyMap(const GKeyBinding& binding)
{
    QVariantMap map{{QStringLiteral("kind"), QStringLiteral("raw")},
                    {QStringLiteral("modifiers"), 0},
                    {QStringLiteral("key"), QString{}},
                    {QStringLiteral("raw"), QString{}}};
    if (binding.kind() == GKeyBinding::Kind::Key && binding.key().has_value()) {
        map[QStringLiteral("kind")] = QStringLiteral("key");
        map[QStringLiteral("modifiers")] = static_cast<int>(binding.modifiers());
        map[QStringLiteral("key")] = keyName(*binding.key());
        return map;
    }
    const auto bytes = binding.encoded();
    const QByteArray hex =
        QByteArray{reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())}.toHex(' ');
    map[QStringLiteral("raw")] = QString::fromLatin1(hex);
    return map;
}

std::expected<GKeyBinding, QString> gKeyFrom(const QVariantMap& map)
{
    const QString kind = map.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("default")) {
        return GKeyBinding::original();
    }
    if (kind == QLatin1String("raw")) {
        const QByteArray bytes = QByteArray::fromHex(map.value(QStringLiteral("raw")).toString().toLatin1());
        if (bytes.size() != 4) {
            return std::unexpected(QObject::tr("bytes da tecla G inválidos"));
        }
        std::array<std::uint8_t, 4> entry{};
        std::memcpy(entry.data(), bytes.constData(), entry.size());
        return GKeyBinding::raw(entry);
    }
    if (kind == QLatin1String("key")) {
        bool ok = false;
        const int modifiers = map.value(QStringLiteral("modifiers")).toInt(&ok);
        if (!ok || modifiers < 0 || modifiers > 0xFF) {
            return std::unexpected(QObject::tr("modificadores inválidos"));
        }
        const QString name = map.value(QStringLiteral("key")).toString();
        const auto key = G915KeyCatalog::findByName((QLatin1String(kKeyPrefix) + name).toStdString());
        if (!key.has_value()) {
            return std::unexpected(QObject::tr("tecla desconhecida: %1").arg(name));
        }
        auto binding = GKeyBinding::create(static_cast<std::uint8_t>(modifiers), *key);
        if (!binding.has_value()) {
            return std::unexpected(QString::fromStdString(binding.error().detail));
        }
        return *binding;
    }
    return std::unexpected(QObject::tr("tipo de atribuição desconhecido"));
}

} // namespace

G915OnboardController::G915OnboardController(const application::G915OnboardSettings& settings, QObject* parent)
    : QObject(parent), settings_(&settings), logoEffect_(effectMap(std::nullopt)), keysEffect_(effectMap(std::nullopt))
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &G915OnboardController::onFinished);
}

G915OnboardController::~G915OnboardController()
{
    // A gravação não é cancelável: espera terminar, pois o caso de uso deve continuar válido até lá.
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

QStringList G915OnboardController::keyChoices() const
{
    QStringList names;
    for (const auto& definition : G915KeyCatalog::keys()) {
        if (definition.zone == kKeyboardZone &&
            QString::fromUtf8(definition.name.data(), static_cast<qsizetype>(definition.name.size()))
                .startsWith(QLatin1String(kKeyPrefix))) {
            names.append(keyName(definition.id));
        }
    }
    return names;
}

bool G915OnboardController::acceptRequest()
{
    if (running_) {
        status_ = tr("aguarde a operação em andamento");
        emit stateChanged();
        return false;
    }
    return true;
}

void G915OnboardController::fail(const QString& message)
{
    status_ = message;
    emit stateChanged();
}

void G915OnboardController::readFromKeyboard()
{
    if (acceptRequest()) {
        start(Kind::Read, std::nullopt);
    }
}

void G915OnboardController::writeToKeyboard(const QVariantMap& logoEffect, const QVariantMap& keysEffect,
                                            const QVariantList& gKeys)
{
    if (!acceptRequest()) {
        return;
    }
    const auto logo = effectFrom(logoEffect);
    const auto keys = effectFrom(keysEffect);
    if (!logo.has_value() || !keys.has_value()) {
        fail(tr("Valores inválidos: %1").arg(!logo.has_value() ? logo.error() : keys.error()));
        return;
    }
    if (gKeys.size() != static_cast<qsizetype>(domain::G915ProfileSettings::kGKeyCount)) {
        fail(tr("Valores inválidos: são esperadas %1 teclas G").arg(domain::G915ProfileSettings::kGKeyCount));
        return;
    }
    domain::G915ProfileSettings::GKeys bindings{GKeyBinding::original(), GKeyBinding::original(),
                                                GKeyBinding::original(), GKeyBinding::original(),
                                                GKeyBinding::original()};
    for (qsizetype i = 0; i < gKeys.size(); ++i) {
        auto binding = gKeyFrom(gKeys.at(i).toMap());
        if (!binding.has_value()) {
            fail(tr("Valores inválidos: G%1: %2").arg(i + 1).arg(binding.error()));
            return;
        }
        bindings.at(static_cast<std::size_t>(i)) = *binding;
    }
    auto valid = domain::G915ProfileSettings::make(*logo, *keys, bindings);
    if (!valid.has_value()) {
        fail(tr("Valores inválidos: %1").arg(QString::fromStdString(valid.error().detail)));
        return;
    }
    start(Kind::Write, std::move(*valid));
}

void G915OnboardController::restoreBackup()
{
    if (acceptRequest()) {
        start(Kind::Restore, std::nullopt);
    }
}

void G915OnboardController::start(Kind kind, std::optional<domain::G915ProfileSettings> request)
{
    running_ = true;
    status_ = tr("Aguarde: operação no teclado em andamento…");
    const application::G915OnboardSettings& useCase = *settings_;
    watcher_.setFuture(QtConcurrent::run([&useCase, kind, request = std::move(request)] -> Outcome {
        if (kind == Kind::Read) {
            auto snapshot = useCase.read();
            if (!snapshot.has_value()) {
                return {.kind = kind, .settings = std::nullopt, .error = snapshot.error().detail};
            }
            return {
                .kind = kind,
                .settings = snapshot->settings,
                .sector = snapshot->sector,
                .backupExists = snapshot->backupExists,
            };
        }
        auto done = kind == Kind::Write ? useCase.write(*request) : useCase.restoreBackup();
        if (!done.has_value()) {
            return {.kind = kind, .settings = std::nullopt, .error = done.error().detail};
        }
        // Releitura (só leitura) para publicar o setor e se a cópia existe de fato: gravar valores iguais aos do
        // teclado não escreve nem cria cópia.
        const auto after = useCase.read();
        if (!after.has_value()) {
            return {.kind = kind, .settings = *done, .error = after.error().detail};
        }
        return {.kind = kind, .settings = *done, .sector = after->sector, .backupExists = after->backupExists};
    }));
    emit stateChanged();
}

void G915OnboardController::onFinished()
{
    const Outcome outcome = watcher_.result();
    running_ = false;
    if (!outcome.settings.has_value()) {
        qWarning("lighting: operação onboard do G915 falhou: %s", outcome.error.c_str());
        status_ = tr("Falha: %1").arg(QString::fromStdString(outcome.error));
        emit stateChanged();
        return;
    }
    const domain::G915ProfileSettings& applied = *outcome.settings;
    loaded_ = true;
    logoEffect_ = effectMap(applied.logo());
    keysEffect_ = effectMap(applied.keys());
    gKeys_.clear();
    for (const auto& binding : applied.gKeys()) {
        gKeys_.append(gKeyMap(binding));
    }
    activeSector_ = static_cast<int>(outcome.sector.value_or(0));
    backupExists_ = outcome.backupExists;
    if (outcome.kind == Kind::Read) {
        status_ = tr("Lido do teclado: perfil M%1").arg(activeSector_);
    } else {
        status_ = outcome.kind == Kind::Write ? tr("Gravado e conferido no teclado: perfil M%1").arg(activeSector_)
                                              : tr("Cópia restaurada no teclado: perfil M%1").arg(activeSector_);
        if (!outcome.error.empty()) {
            status_ += tr(" (releitura do estado falhou: %1)").arg(QString::fromStdString(outcome.error));
        }
    }
    emit stateChanged();
}

} // namespace ghub::lighting::presentation
