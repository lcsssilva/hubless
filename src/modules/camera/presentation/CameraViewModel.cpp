#include "modules/camera/presentation/CameraViewModel.hpp"

#include "modules/camera/domain/VideoModeSummary.hpp"

#include <QVariantMap>

#include <cstdint>
#include <utility>

namespace ghub::camera {

namespace {

// Taxa em fps com vírgula decimal, no máximo 2 casas e sem zeros à direita ("7,5", "29,97", "30").
QString rateText(const FrameRate& rate)
{
    const std::uint64_t seconds = rate.seconds();
    const std::uint64_t hundredths = ((std::uint64_t{rate.frames()} * 100) + (seconds / 2)) / seconds;
    const auto whole = static_cast<qulonglong>(hundredths / 100);
    const auto fraction = static_cast<int>(hundredths % 100);
    auto text = QString::number(whole);
    if (fraction != 0) {
        text += QLatin1Char(',') + QStringLiteral("%1").arg(fraction, 2, 10, QLatin1Char('0'));
        if (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
    }
    return text;
}

QString modeText(const VideoModeSummary& summary)
{
    return CameraViewModel::tr("%1: até %2×%3 a %4 fps")
        .arg(QString::fromUtf8(summary.format.fourcc().data(), static_cast<qsizetype>(summary.format.fourcc().size())))
        .arg(summary.width)
        .arg(summary.height)
        .arg(rateText(summary.maxFrameRate));
}

QString rangeText(const CameraControl& control)
{
    return CameraViewModel::tr("%1–%2, padrão %3")
        .arg(control.minimum())
        .arg(control.maximum())
        .arg(control.defaultValue());
}

QString valueText(const CameraControl& control)
{
    const auto current = control.currentValue();
    switch (control.kind()) {
    case ControlKind::Integer:
        if (!current.has_value()) {
            return CameraViewModel::tr("valor indisponível (%1)").arg(rangeText(control));
        }
        return QStringLiteral("%1 (%2)").arg(*current).arg(rangeText(control));
    case ControlKind::Boolean:
        if (!current.has_value()) {
            return CameraViewModel::tr("valor indisponível");
        }
        return *current != 0 ? CameraViewModel::tr("ligado") : CameraViewModel::tr("desligado");
    case ControlKind::Menu:
        if (!current.has_value()) {
            return CameraViewModel::tr("valor indisponível");
        }
        if (const auto option = control.currentOption(); option.has_value()) {
            return QString::fromStdString(option->label);
        }
        return QString::number(*current);
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

QString controlSummary(const CameraControl& control)
{
    auto text = valueText(control);
    if (control.inactive()) {
        text += CameraViewModel::tr(" · inativo");
    }
    if (control.readOnly()) {
        text += CameraViewModel::tr(" · somente leitura");
    }
    return text;
}

QString kindText(ControlKind kind)
{
    switch (kind) {
    case ControlKind::Integer:
        return QStringLiteral("integer");
    case ControlKind::Boolean:
        return QStringLiteral("boolean");
    case ControlKind::Menu:
        return QStringLiteral("menu");
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

QVariantMap controlMap(const CameraControl& control)
{
    QVariantList options;
    for (const auto& option : control.options()) {
        options.append(QVariantMap{
            {QStringLiteral("value"), static_cast<qlonglong>(option.value)},
            {QStringLiteral("label"), QString::fromStdString(option.label)},
        });
    }
    const auto current = control.currentValue();
    QVariantMap map{
        {QStringLiteral("name"), QString::fromStdString(control.name())},
        {QStringLiteral("summary"), controlSummary(control)},
        {QStringLiteral("id"), control.id()},
        {QStringLiteral("kind"), kindText(control.kind())},
        {QStringLiteral("minimum"), static_cast<qlonglong>(control.minimum())},
        {QStringLiteral("maximum"), static_cast<qlonglong>(control.maximum())},
        {QStringLiteral("step"), static_cast<qlonglong>(control.step())},
        {QStringLiteral("defaultValue"), static_cast<qlonglong>(control.defaultValue())},
        {QStringLiteral("options"), options},
        {QStringLiteral("editable"), !control.readOnly() && !control.inactive() && current.has_value()},
    };
    if (current.has_value()) {
        map.insert(QStringLiteral("value"), static_cast<qlonglong>(*current));
    }
    return map;
}

} // namespace

CameraViewModel::CameraViewModel(const GetCameraStatus& getCameraStatus, std::chrono::milliseconds refreshInterval,
                                 QObject* parent)
    : QObject(parent), getCameraStatus_(&getCameraStatus)
{
    timer_.setInterval(refreshInterval);
    connect(&timer_, &QTimer::timeout, this, &CameraViewModel::refresh);
    refresh();
    timer_.start();
}

void CameraViewModel::refresh()
{
    const auto cameraStatus = getCameraStatus_->execute();
    QString status;
    QVariantList modes;
    QVariantList controls;
    if (const auto* description = cameraStatus.description()) {
        status = QString::fromStdString(description->name);
        for (const auto& summary : summarizeModes(description->modes)) {
            modes.append(QVariantMap{{QStringLiteral("text"), modeText(summary)}});
        }
        for (const auto& control : description->controls) {
            controls.append(controlMap(control));
        }
    } else if (cameraStatus.state() == CameraState::Unavailable) {
        status = tr("leitura indisponível");
    } else {
        status = tr("não detectada");
    }
    if (status == status_ && modes == modes_ && controls == controls_) {
        return;
    }
    status_ = std::move(status);
    modes_ = modes;
    controls_ = controls;
    emit cameraChanged();
}

} // namespace ghub::camera
