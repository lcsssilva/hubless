#include "modules/battery/infrastructure/HeadsetControlJson.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cmath>

namespace ghub::battery {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;

constexpr auto g733Vendor = "0x046d";
constexpr auto g733Product = "0x0afe";

[[nodiscard]] bool isG733(const QJsonObject& device)
{
    return device.value("id_vendor").toString().compare(QLatin1String(g733Vendor), Qt::CaseInsensitive) == 0 &&
           device.value("id_product").toString().compare(QLatin1String(g733Product), Qt::CaseInsensitive) == 0;
}

[[nodiscard]] bool hasSupportedApiMajor(const QJsonObject& root)
{
    const auto version = root.value("api_version");
    return version.isString() && version.toString().startsWith(QLatin1String("1."));
}

[[nodiscard]] shared_kernel::Result<BatteryReading> readingFrom(const QJsonValue& batteryValue)
{
    if (!batteryValue.isObject()) {
        return fail(ErrorCode::MalformedData, "campo battery ausente");
    }
    const auto battery = batteryValue.toObject();
    const auto statusValue = battery.value("status");
    if (!statusValue.isString()) {
        return fail(ErrorCode::MalformedData, "battery.status ausente");
    }
    const auto status = statusValue.toString();
    if (status == QLatin1String("BATTERY_UNAVAILABLE")) {
        return BatteryReading{ChargeState::Unknown, std::nullopt};
    }
    if (status != QLatin1String("BATTERY_AVAILABLE") && status != QLatin1String("BATTERY_CHARGING")) {
        return fail(ErrorCode::MalformedData, "battery.status desconhecido");
    }
    const auto levelValue = battery.value("level");
    const auto number = levelValue.toDouble(-1.0);
    if (!levelValue.isDouble() || number != std::floor(number) || number < 0.0 || number > 100.0) {
        return fail(ErrorCode::MalformedData, "battery.level ausente ou fora de 0-100");
    }
    const auto level = BatteryLevel::fromPercent(static_cast<int>(number));
    if (!level) {
        return std::unexpected(level.error());
    }
    return BatteryReading{
        status == QLatin1String("BATTERY_CHARGING") ? ChargeState::Charging : ChargeState::Discharging, *level};
}

} // namespace

shared_kernel::Result<std::optional<BatteryReading>> parseHeadsetControlBattery(QByteArrayView json)
{
    const auto document = QJsonDocument::fromJson(json.toByteArray());
    if (!document.isObject()) {
        return fail(ErrorCode::MalformedData, "saída não é um objeto JSON");
    }
    const auto root = document.object();
    if (!hasSupportedApiMajor(root)) {
        return fail(ErrorCode::MalformedData, "api_version ausente ou incompatível");
    }
    if (!root.value("devices").isArray()) {
        return fail(ErrorCode::MalformedData, "campo devices ausente");
    }
    for (const auto& entry : root.value("devices").toArray()) {
        const auto device = entry.toObject();
        if (!isG733(device)) {
            continue;
        }
        auto parsed = readingFrom(device.value("battery"));
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        return std::optional<BatteryReading>{*parsed};
    }
    return std::optional<BatteryReading>{};
}

} // namespace ghub::battery
