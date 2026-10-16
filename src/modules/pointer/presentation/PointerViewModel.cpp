#include "modules/pointer/presentation/PointerViewModel.hpp"

#include "modules/pointer/domain/PointerConfiguration.hpp"

#include <QVariantMap>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>

namespace ghub::pointer {

namespace {

const Ready* readyOf(const std::optional<PointerStatus>& status)
{
    return status.has_value() ? std::get_if<Ready>(&*status) : nullptr;
}

QString dpiValueText(const ResolutionStage& stage)
{
    if (stage.dpiY().has_value()) {
        return QStringLiteral("%1×%2").arg(stage.dpiX()).arg(*stage.dpiY());
    }
    return QString::number(stage.dpiX());
}

// Os 4 bytes de uma entrada de botão (big-endian) em hexadecimal, separados por espaço.
QString hexBytes(std::uint32_t value)
{
    QStringList parts;
    for (int shift = 24; shift >= 0; shift -= 8) {
        parts.append(
            QStringLiteral("%1").arg((value >> static_cast<unsigned>(shift)) & 0xFFU, 2, 16, QLatin1Char('0')));
    }
    return parts.join(QLatin1Char(' '));
}

QString buttonText(const ButtonAction& action)
{
    switch (action.kind()) {
    case ButtonActionKind::None:
        return PointerViewModel::tr("nenhuma");
    case ButtonActionKind::Button:
        return PointerViewModel::tr("botão %1").arg(action.value());
    case ButtonActionKind::Special:
        return PointerViewModel::tr("especial 0x%1").arg(action.value(), 0, 16);
    case ButtonActionKind::Key: {
        const auto modifiers = action.value() >> 8U;
        const auto key = action.value() & 0xFFU;
        if (modifiers == 0) {
            return PointerViewModel::tr("tecla %1").arg(key);
        }
        return PointerViewModel::tr("tecla %1 + modificadores 0x%2").arg(key).arg(modifiers, 2, 16, QLatin1Char('0'));
    }
    case ButtonActionKind::Macro:
        return PointerViewModel::tr("macro (%1 passos)").arg(action.value());
    case ButtonActionKind::Unknown:
        if (action.value() != 0) {
            return PointerViewModel::tr("não interpretado: %1").arg(hexBytes(action.value()));
        }
        return PointerViewModel::tr("desconhecida");
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

QString ledModeText(const LedState& led)
{
    switch (led.mode()) {
    case LedMode::Off:
        return PointerViewModel::tr("desligado");
    case LedMode::On:
        return PointerViewModel::tr("ligado");
    case LedMode::Cycle:
        return PointerViewModel::tr("ciclo");
    case LedMode::Breathing:
        return PointerViewModel::tr("respiração");
    case LedMode::Unknown:
        if (!led.rawBytes().empty()) {
            QStringList parts;
            for (const auto byte : led.rawBytes()) {
                parts.append(QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')));
            }
            return PointerViewModel::tr("não interpretado: %1").arg(parts.join(QLatin1Char(' ')));
        }
        return PointerViewModel::tr("desconhecido (%1)").arg(led.rawMode());
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

QString colorText(const LedState& led)
{
    const auto channel = [](std::uint32_t value) { return std::min<std::uint32_t>(value, 255U); };
    return QStringLiteral("#%1%2%3")
        .arg(channel(led.red()), 2, 16, QLatin1Char('0'))
        .arg(channel(led.green()), 2, 16, QLatin1Char('0'))
        .arg(channel(led.blue()), 2, 16, QLatin1Char('0'));
}

QVariantMap stageMap(const ResolutionStage& stage)
{
    return {
        {QStringLiteral("index"), stage.index()},
        {QStringLiteral("dpiText"), PointerViewModel::tr("%1 dpi").arg(dpiValueText(stage))},
        {QStringLiteral("active"), stage.isActive()},
        {QStringLiteral("isDefault"), stage.isDefault()},
        {QStringLiteral("disabled"), stage.isDisabled()},
        {QStringLiteral("listed"), stage.isDpiListed()},
    };
}

QVariantMap profileMap(const PointerProfile& profile)
{
    QVariantList rates;
    for (const auto rate : profile.supportedReportRates()) {
        rates.append(rate);
    }
    QVariantList stages;
    for (const auto& stage : profile.stages()) {
        stages.append(stageMap(stage));
    }
    QVariantList buttons;
    for (const auto& button : profile.buttons()) {
        buttons.append(QVariantMap{
            {QStringLiteral("index"), button.index()},
            {QStringLiteral("text"), buttonText(button.action())},
        });
    }
    QVariantList leds;
    for (const auto& led : profile.leds()) {
        leds.append(QVariantMap{
            {QStringLiteral("index"), led.index()},
            {QStringLiteral("mode"), ledModeText(led)},
            {QStringLiteral("color"), colorText(led)},
            {QStringLiteral("brightness"), led.brightness()},
        });
    }
    return {
        {QStringLiteral("index"), profile.index()},
        {QStringLiteral("name"), QString::fromStdString(profile.name())},
        {QStringLiteral("active"), profile.isActive()},
        {QStringLiteral("enabled"), profile.isEnabled()},
        {QStringLiteral("reportRate"), profile.reportRate()},
        {QStringLiteral("supportedReportRates"), rates},
        {QStringLiteral("stages"), stages},
        {QStringLiteral("buttons"), buttons},
        {QStringLiteral("leds"), leds},
    };
}

const ResolutionStage* findStage(const PointerConfiguration& configuration, const PointerWarning& warning)
{
    for (const auto& profile : configuration.profiles()) {
        if (profile.index() != warning.profileIndex) {
            continue;
        }
        for (const auto& stage : profile.stages()) {
            if (stage.index() == warning.stageIndex) {
                return &stage;
            }
        }
    }
    return nullptr;
}

} // namespace

PointerViewModel::PointerViewModel(PointerReadRunner& runner, QObject* parent) : QObject(parent), runner_(&runner) {}

bool PointerViewModel::reading() const
{
    return runner_->isReading();
}

QString PointerViewModel::state() const
{
    if (runner_->isReading()) {
        return QStringLiteral("reading");
    }
    const auto status = runner_->lastStatus();
    if (!status.has_value()) {
        return QStringLiteral("idle");
    }
    if (std::holds_alternative<NotDetected>(*status)) {
        return QStringLiteral("notDetected");
    }
    if (std::holds_alternative<Unavailable>(*status)) {
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("ready");
}

QString PointerViewModel::statusText() const
{
    const auto currentState = state();
    if (currentState == QLatin1String("reading")) {
        return tr("lendo a configuração do mouse…");
    }
    if (currentState == QLatin1String("notDetected")) {
        return tr("mouse G703 não detectado");
    }
    if (currentState == QLatin1String("unavailable")) {
        const auto status = runner_->lastStatus(); // por valor: o temporário não pode ser referenciado
        const auto* unavailable = status.has_value() ? std::get_if<Unavailable>(&*status) : nullptr;
        return tr("leitura indisponível: %1")
            .arg(unavailable != nullptr ? QString::fromStdString(unavailable->error.detail) : QString{});
    }
    if (currentState == QLatin1String("ready")) {
        return tr("configuração lida");
    }
    return tr("nenhuma leitura feita");
}

QString PointerViewModel::deviceName() const
{
    const auto status = runner_->lastStatus();
    const auto* ready = readyOf(status);
    return ready != nullptr ? QString::fromStdString(ready->configuration.name()) : QString{};
}

QString PointerViewModel::connectionText() const
{
    const auto status = runner_->lastStatus();
    const auto* ready = readyOf(status);
    if (ready == nullptr) {
        return {};
    }
    switch (ready->path) {
    case device_inventory::ConnectionPath::LightspeedReceiver:
        return tr("receptor Lightspeed");
    case device_inventory::ConnectionPath::UsbCable:
        return tr("cabo USB");
    case device_inventory::ConnectionPath::UsbDongle:
        return tr("receptor USB");
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

QVariantList PointerViewModel::profiles() const
{
    QVariantList result;
    const auto status = runner_->lastStatus();
    if (const auto* ready = readyOf(status)) {
        for (const auto& profile : ready->configuration.profiles()) {
            result.append(profileMap(profile));
        }
    }
    return result;
}

QStringList PointerViewModel::warnings() const
{
    QStringList result;
    const auto status = runner_->lastStatus();
    const auto* ready = readyOf(status);
    if (ready == nullptr) {
        return result;
    }
    for (const auto& warning : ready->configuration.warnings()) {
        switch (warning.kind) {
        case PointerWarningKind::NoActiveProfile:
            result.append(tr("nenhum perfil ativo"));
            break;
        case PointerWarningKind::ActiveProfileDisabled:
            result.append(tr("o ratbagd marca o perfil %1 como ativo, mas ele está desabilitado; o perfil em uso no "
                             "mouse pode ser outro")
                              .arg(warning.profileIndex.value_or(0)));
            break;
        case PointerWarningKind::ActiveDpiNotListed: {
            const auto* stage = findStage(ready->configuration, warning);
            if (stage != nullptr) {
                result.append(tr("DPI ativo %1 fora da lista do ratbagd (perfil %2)")
                                  .arg(dpiValueText(*stage))
                                  .arg(warning.profileIndex.value_or(0)));
            }
            break;
        }
        }
    }
    return result;
}

void PointerViewModel::read()
{
    runner_->start();
    emit pointerChanged(); // `reading` pode ter mudado
}

void PointerViewModel::onReadFinished()
{
    emit pointerChanged();
}

} // namespace ghub::pointer
