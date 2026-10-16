#include "modules/pointer/presentation/G703OnboardController.hpp"

#include <QByteArray>
#include <QDebug>
#include <QRegularExpression>
#include <QtConcurrent>

#include <array>
#include <cstring>
#include <expected>
#include <utility>
#include <vector>

namespace ghub::pointer {

namespace {

QString describe(const G703ProfileSettings& settings)
{
    QStringList stages;
    for (const auto dpi : settings.dpiStages()) {
        stages.append(QString::number(dpi));
    }
    return QObject::tr("%1 Hz, DPI %2, padrão: estágio %3")
        .arg(settings.reportRateHz())
        .arg(stages.join(QStringLiteral(" / ")))
        .arg(settings.defaultStage() + 1);
}

QVariantMap ledMap(const LedSetting& led)
{
    QVariantMap map{{QStringLiteral("kind"), QStringLiteral("off")},
                    {QStringLiteral("color"), QStringLiteral("#000000")},
                    {QStringLiteral("periodMs"), 0},
                    {QStringLiteral("brightness"), 0},
                    {QStringLiteral("raw"), QString{}}};
    switch (led.kind()) {
    case LedSetting::Kind::Off:
        break;
    case LedSetting::Kind::Fixed:
        map[QStringLiteral("kind")] = QStringLiteral("fixed");
        map[QStringLiteral("color")] = QStringLiteral("#%1%2%3")
                                           .arg(led.red(), 2, 16, QLatin1Char('0'))
                                           .arg(led.green(), 2, 16, QLatin1Char('0'))
                                           .arg(led.blue(), 2, 16, QLatin1Char('0'));
        break;
    case LedSetting::Kind::Cycle:
        map[QStringLiteral("kind")] = QStringLiteral("cycle");
        map[QStringLiteral("periodMs")] = static_cast<int>(led.periodMs());
        map[QStringLiteral("brightness")] = static_cast<int>(led.brightness());
        break;
    case LedSetting::Kind::Raw: {
        const auto bytes = led.encode();
        const QByteArray hex =
            QByteArray{reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())}.toHex(' ');
        map[QStringLiteral("kind")] = QStringLiteral("raw");
        map[QStringLiteral("raw")] = QString::fromLatin1(hex);
        break;
    }
    }
    return map;
}

// Converte o mapa do QML em LedSetting; texto vazio no erro. "raw" volta como foi lido (bytes em hexadecimal).
std::expected<LedSetting, QString> ledFromMap(const QVariantMap& map)
{
    const QString kind = map.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("off")) {
        return LedSetting::off();
    }
    if (kind == QLatin1String("fixed")) {
        const QString color = map.value(QStringLiteral("color")).toString();
        static const QRegularExpression pattern{QStringLiteral("^#[0-9a-fA-F]{6}$")};
        if (!pattern.match(color).hasMatch()) {
            return std::unexpected(QObject::tr("cor do LED deve ser #rrggbb"));
        }
        const auto channel = [&color](int at) {
            return static_cast<std::uint8_t>(color.mid(at, 2).toUInt(nullptr, 16));
        };
        return LedSetting::fixed(channel(1), channel(3), channel(5));
    }
    if (kind == QLatin1String("cycle")) {
        bool periodOk = false;
        bool brightnessOk = false;
        const int period = map.value(QStringLiteral("periodMs")).toInt(&periodOk);
        const int brightness = map.value(QStringLiteral("brightness")).toInt(&brightnessOk);
        if (!periodOk || !brightnessOk || period < 0 || brightness < 0) {
            return std::unexpected(QObject::tr("período ou brilho do LED inválido"));
        }
        auto cycle = LedSetting::cycle(static_cast<std::uint32_t>(period), static_cast<std::uint32_t>(brightness));
        if (!cycle.has_value()) {
            return std::unexpected(QString::fromStdString(cycle.error().detail));
        }
        return *cycle;
    }
    if (kind == QLatin1String("raw")) {
        const QByteArray bytes = QByteArray::fromHex(map.value(QStringLiteral("raw")).toString().toLatin1());
        if (bytes.size() != static_cast<qsizetype>(LedSetting::kEntrySize)) {
            return std::unexpected(QObject::tr("bytes do LED inválidos"));
        }
        std::array<std::uint8_t, LedSetting::kEntrySize> entry{};
        std::memcpy(entry.data(), bytes.constData(), entry.size());
        return LedSetting::decode(entry);
    }
    return std::unexpected(QObject::tr("tipo de LED desconhecido"));
}

QVariantMap buttonMap(const ButtonAction& action)
{
    QVariantMap map{{QStringLiteral("kind"), QStringLiteral("unknown")},
                    {QStringLiteral("number"), 0},
                    {QStringLiteral("modifiers"), 0},
                    {QStringLiteral("key"), 0},
                    {QStringLiteral("raw"), QString{}}};
    switch (action.kind()) {
    case ButtonActionKind::Button:
        map[QStringLiteral("kind")] = QStringLiteral("mouse");
        map[QStringLiteral("number")] = static_cast<int>(action.value());
        break;
    case ButtonActionKind::Key:
        map[QStringLiteral("kind")] = QStringLiteral("key");
        map[QStringLiteral("modifiers")] = static_cast<int>(action.value() >> 8U);
        map[QStringLiteral("key")] = static_cast<int>(action.value() & 0xFFU);
        break;
    case ButtonActionKind::None:
        map[QStringLiteral("kind")] = QStringLiteral("none");
        break;
    default: {
        const std::array<std::uint8_t, 4> bytes{
            static_cast<std::uint8_t>(action.value() >> 24U), static_cast<std::uint8_t>(action.value() >> 16U),
            static_cast<std::uint8_t>(action.value() >> 8U), static_cast<std::uint8_t>(action.value())};
        map[QStringLiteral("raw")] = QString::fromLatin1(
            QByteArray{reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())}.toHex(' '));
        break;
    }
    }
    return map;
}

QVariantMap keepButtonMap()
{
    return {{QStringLiteral("kind"), QStringLiteral("keep")}};
}

// Converte os mapas do QML em atribuições; a validade (botão 1..5, tecla da lista) é do domínio.
std::expected<std::vector<ButtonAssignment>, QString> assignmentsFromList(const QVariantList& list)
{
    std::vector<ButtonAssignment> result;
    result.reserve(static_cast<std::size_t>(list.size()));
    for (const QVariant& item : list) {
        const QVariantMap map = item.toMap();
        const QString kind = map.value(QStringLiteral("kind")).toString();
        const int number = map.value(QStringLiteral("number")).toInt();
        const int modifiers = map.value(QStringLiteral("modifiers")).toInt();
        const int key = map.value(QStringLiteral("key")).toInt();
        if (kind == QLatin1String("keep")) {
            result.push_back(ButtonAssignment::keep());
        } else if (kind == QLatin1String("default")) {
            result.push_back(ButtonAssignment::factoryDefault());
        } else if (kind == QLatin1String("mouse") || kind == QLatin1String("key")) {
            if (number < 0 || modifiers < 0 || key < 0) {
                return std::unexpected(QObject::tr("valores de botão fora do intervalo"));
            }
            auto assignment =
                kind == QLatin1String("mouse")
                    ? ButtonAssignment::mouseButton(static_cast<std::uint32_t>(number))
                    : ButtonAssignment::key(static_cast<std::uint32_t>(modifiers), static_cast<std::uint32_t>(key));
            if (!assignment.has_value()) {
                return std::unexpected(QString::fromStdString(assignment.error().detail));
            }
            result.push_back(*assignment);
        } else {
            return std::unexpected(QObject::tr("tipo de atribuição de botão desconhecido"));
        }
    }
    return result;
}

} // namespace

QVariantList G703OnboardController::keyChoices() const
{
    QVariantList choices;
    for (const OnboardKey& key : onboardKeyCatalog()) {
        choices.append(QVariantMap{
            {QStringLiteral("code"), static_cast<int>(key.code)},
            {QStringLiteral("label"), QString::fromUtf8(key.label.data(), static_cast<qsizetype>(key.label.size()))}});
    }
    return choices;
}

G703OnboardController::G703OnboardController(const G703OnboardSettings& settings, QObject* parent)
    : QObject(parent), settings_(&settings), primaryLed_(ledMap(LedSetting::off())), logoLed_(ledMap(LedSetting::off()))
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &G703OnboardController::onFinished);
}

G703OnboardController::~G703OnboardController()
{
    // A gravação não é cancelável: espera terminar, pois o caso de uso deve continuar válido até lá.
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

bool G703OnboardController::acceptRequest()
{
    if (running_) {
        status_ = tr("aguarde a operação em andamento");
        emit stateChanged();
        return false;
    }
    return true;
}

void G703OnboardController::fail(const QString& message)
{
    status_ = message;
    emit stateChanged();
}

void G703OnboardController::readFromMouse()
{
    if (acceptRequest()) {
        start(Kind::Read, std::nullopt);
    }
}

void G703OnboardController::writeToMouse(int reportRateHz, const QVariantList& dpiStages, int defaultStage,
                                         int dpiShiftStage, const QVariantMap& primaryLed, const QVariantMap& logoLed)
{
    writeToMouse(reportRateHz, dpiStages, defaultStage, dpiShiftStage, primaryLed, logoLed,
                 QVariantList(static_cast<qsizetype>(ButtonAssignments::kCount), keepButtonMap()), false);
}

void G703OnboardController::writeToMouse(int reportRateHz, const QVariantList& dpiStages, int defaultStage,
                                         int dpiShiftStage, const QVariantMap& primaryLed, const QVariantMap& logoLed,
                                         const QVariantList& buttons, bool confirmPrimaryChange)
{
    if (!acceptRequest()) {
        return;
    }
    if (reportRateHz < 0 || defaultStage < 0 || dpiShiftStage < -1) {
        fail(tr("Valores inválidos: taxa, estágio padrão ou estágio do shift fora do intervalo"));
        return;
    }
    std::vector<std::uint16_t> dpis;
    dpis.reserve(static_cast<std::size_t>(dpiStages.size()));
    for (const QVariant& value : dpiStages) {
        bool ok = false;
        const int dpi = value.toInt(&ok);
        if (!ok || dpi < 0 || dpi > 0xFFFF) {
            fail(tr("Valores inválidos: DPI fora do intervalo"));
            return;
        }
        dpis.push_back(static_cast<std::uint16_t>(dpi));
    }
    const auto primary = ledFromMap(primaryLed);
    const auto logo = ledFromMap(logoLed);
    if (!primary.has_value() || !logo.has_value()) {
        fail(tr("Valores inválidos: %1").arg(!primary.has_value() ? primary.error() : logo.error()));
        return;
    }
    std::optional<std::uint32_t> shift;
    if (dpiShiftStage >= 0) {
        shift = static_cast<std::uint32_t>(dpiShiftStage);
    }
    auto valid = G703ProfileSettings::make(static_cast<std::uint32_t>(reportRateHz), std::move(dpis),
                                           static_cast<std::uint32_t>(defaultStage), shift, *primary, *logo);
    if (!valid.has_value()) {
        fail(tr("Valores inválidos: %1").arg(QString::fromStdString(valid.error().detail)));
        return;
    }
    auto parsed = assignmentsFromList(buttons);
    if (!parsed.has_value()) {
        fail(tr("Valores inválidos: %1").arg(parsed.error()));
        return;
    }
    auto assignments = ButtonAssignments::make(std::move(*parsed), confirmPrimaryChange);
    if (!assignments.has_value()) {
        const QString detail = QString::fromStdString(assignments.error().detail);
        fail(assignments.error().detail.find("confirma") != std::string::npos
                 ? tr("Falta confirmar: %1").arg(detail)
                 : tr("Valores inválidos: %1").arg(detail));
        return;
    }
    start(Kind::Write, WriteRequest{std::move(*valid), std::move(*assignments)});
}

void G703OnboardController::restoreBackup()
{
    if (acceptRequest()) {
        start(Kind::Restore, std::nullopt);
    }
}

void G703OnboardController::start(Kind kind, std::optional<WriteRequest> request)
{
    running_ = true;
    status_ = tr("Aguarde: operação no mouse em andamento…");
    const G703OnboardSettings& useCase = *settings_;
    watcher_.setFuture(QtConcurrent::run([&useCase, kind, request = std::move(request)] -> Outcome {
        if (kind == Kind::Read) {
            auto snapshot = useCase.read();
            if (!snapshot.has_value()) {
                return {.kind = kind, .settings = std::nullopt, .error = snapshot.error().detail};
            }
            return {
                .kind = kind,
                .settings = snapshot->settings,
                .buttons = snapshot->buttons,
                .sector = snapshot->sector,
                .backupExists = snapshot->backupExists,
            };
        }
        auto done = kind == Kind::Write ? useCase.write(request->settings, request->buttons) : useCase.restoreBackup();
        if (!done.has_value()) {
            return {.kind = kind, .settings = std::nullopt, .error = done.error().detail};
        }
        // Releitura (só leitura) para publicar o setor e se a cópia existe de fato: gravar valores iguais aos do
        // mouse não escreve nem cria cópia.
        const auto after = useCase.read();
        if (!after.has_value()) {
            return {.kind = kind, .settings = *done, .error = after.error().detail};
        }
        return {.kind = kind,
                .settings = *done,
                .buttons = after->buttons,
                .sector = after->sector,
                .backupExists = after->backupExists};
    }));
    emit stateChanged();
}

void G703OnboardController::onFinished()
{
    const Outcome outcome = watcher_.result();
    running_ = false;
    if (!outcome.settings.has_value()) {
        qWarning("pointer: operação onboard falhou: %s", outcome.error.c_str());
        status_ = tr("Falha: %1").arg(QString::fromStdString(outcome.error));
        emit stateChanged();
        return;
    }
    const G703ProfileSettings& applied = *outcome.settings;
    loaded_ = true;
    reportRateHz_ = static_cast<int>(applied.reportRateHz());
    defaultStage_ = static_cast<int>(applied.defaultStage());
    dpiShiftStage_ = applied.dpiShiftStage().has_value() ? static_cast<int>(*applied.dpiShiftStage()) : -1;
    primaryLed_ = ledMap(applied.primaryLed());
    logoLed_ = ledMap(applied.logoLed());
    dpiStages_.clear();
    for (const auto dpi : applied.dpiStages()) {
        dpiStages_.append(static_cast<int>(dpi));
    }
    if (outcome.buttons.has_value()) {
        buttons_.clear();
        for (const ButtonAction& action : *outcome.buttons) {
            buttons_.append(buttonMap(action));
        }
    }
    activeSector_ = static_cast<int>(outcome.sector.value_or(0));
    backupExists_ = outcome.backupExists;
    if (outcome.kind == Kind::Read) {
        status_ = tr("Lido do mouse: %1").arg(describe(applied));
    } else {
        status_ = outcome.kind == Kind::Write ? tr("Gravado e conferido no mouse: %1").arg(describe(applied))
                                              : tr("Cópia restaurada: %1").arg(describe(applied));
        if (!outcome.error.empty()) {
            status_ += tr(" (releitura do estado falhou: %1)").arg(QString::fromStdString(outcome.error));
        }
    }
    emit stateChanged();
}

} // namespace ghub::pointer
