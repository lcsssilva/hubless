#include "modules/battery/infrastructure/HeadsetControlJson.hpp"

#include <gtest/gtest.h>

#include <QByteArray>

#include <string>

using ghub::battery::BatteryLevel;
using ghub::battery::BatteryReading;
using ghub::battery::ChargeState;
using ghub::battery::parseHeadsetControlBattery;
using ghub::shared_kernel::ErrorCode;

namespace {

// Estrutura da saída real de 2026-10-13 (evidência G733 §3), completada com api_version e ids.
std::string g733Json(const std::string& batteryObject, const std::string& apiVersion = "1.5")
{
    return R"({"name": "HeadsetControl", "version": "0.0.0-unknown", "api_version": ")" + apiVersion +
           R"(", "device_count": 1, "devices": [{"device": "Logitech G633/G635/G733/G933/G935", )"
           R"("manufacturer": "Logitech", "product": "G733 Gaming Headset", "id_vendor": "0x046d", )"
           R"("id_product": "0x0afe", "capabilities": ["CAP_SIDETONE", "CAP_BATTERY_STATUS", "CAP_LIGHTS"], )"
           R"("battery": )" +
           batteryObject + "}]}";
}

QByteArray bytes(const std::string& s)
{
    return QByteArray::fromStdString(s);
}

BatteryReading reading(ChargeState state, int percent)
{
    return BatteryReading{state, BatteryLevel::fromPercent(percent).value()};
}

TEST(HeadsetControlJson, ParsesAvailableBattery)
{
    const auto json =
        g733Json(R"({"status": "BATTERY_AVAILABLE", "level": 40, "voltage_mv": 3754, "time_to_empty_min": 360})");

    const auto result = parseHeadsetControlBattery(bytes(json));

    ASSERT_TRUE(result.has_value());
    const auto& parsed = result.value();
    if (!parsed.has_value()) {
        FAIL() << "sem leitura";
    }
    EXPECT_EQ(parsed.value(), reading(ChargeState::Discharging, 40));
}

TEST(HeadsetControlJson, ParsesChargingBattery)
{
    const auto result = parseHeadsetControlBattery(bytes(g733Json(R"({"status": "BATTERY_CHARGING", "level": 75})")));

    ASSERT_TRUE(result.has_value());
    const auto& parsed = result.value();
    if (!parsed.has_value()) {
        FAIL() << "sem leitura";
    }
    EXPECT_EQ(parsed.value(), reading(ChargeState::Charging, 75));
}

TEST(HeadsetControlJson, ParsesUnavailableBattery)
{
    const auto result =
        parseHeadsetControlBattery(bytes(g733Json(R"({"status": "BATTERY_UNAVAILABLE", "level": -1})")));

    ASSERT_TRUE(result.has_value());
    const auto& parsed = result.value();
    if (!parsed.has_value()) {
        FAIL() << "sem leitura";
    }
    EXPECT_EQ(parsed.value(), (BatteryReading{ChargeState::Unknown, std::nullopt}));
}

// Saída real de 2026-10-13 com o dongle ligado e o headset desligado ("partial" + errors.battery).
TEST(HeadsetControlJson, ParsesOfflineHeadset_AsUnknownState)
{
    const std::string offline = R"({"name": "HeadsetControl", "version": "0.0.0-unknown",
        "api_version": "1.5", "hidapi_version": "0.15.0", "device_count": 1, "devices": [{
        "status": "partial", "device": "Logitech G633/G635/G733/G933/G935", "vendor": "Logitech",
        "product": "G733 Gaming Headset", "id_vendor": "0x046d", "id_product": "0x0afe",
        "capabilities": ["CAP_SIDETONE", "CAP_BATTERY_STATUS", "CAP_LIGHTS"],
        "capabilities_str": ["sidetone", "battery", "lights"],
        "battery": {"status": "BATTERY_UNAVAILABLE", "level": -1},
        "errors": {"battery": "Device is offline or not responding"}}]})";

    const auto result = parseHeadsetControlBattery(bytes(offline));

    ASSERT_TRUE(result.has_value());
    const auto& parsed = result.value();
    if (!parsed.has_value()) {
        FAIL() << "sem leitura";
    }
    EXPECT_EQ(parsed.value(), (BatteryReading{ChargeState::Unknown, std::nullopt}));
}

TEST(HeadsetControlJson, ReturnsNothing_WhenNoDevice)
{
    const auto result = parseHeadsetControlBattery(
        bytes(R"({"name": "HeadsetControl", "api_version": "1.5", "device_count": 0, "devices": []})"));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
}

TEST(HeadsetControlJson, IgnoresOtherHeadsets)
{
    const std::string json =
        R"({"api_version": "1.5", "device_count": 2, "devices": [)"
        R"({"id_vendor": "0x1038", "id_product": "0x0afe", "battery": {"status": "BATTERY_AVAILABLE", "level": 10}},)"
        R"({"id_vendor": "0x046d", "id_product": "0x0a5b", "battery": {"status": "BATTERY_AVAILABLE", "level": 20}}]})";

    const auto result = parseHeadsetControlBattery(bytes(json));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
}

TEST(HeadsetControlJson, MatchesIdsIgnoringCase_AmongSeveralDevices)
{
    const std::string json =
        R"({"api_version": "1.5", "device_count": 2, "devices": [)"
        R"({"id_vendor": "0x1038", "id_product": "0x0afe", "battery": {"status": "BATTERY_AVAILABLE", "level": 10}},)"
        R"({"id_vendor": "0x046D", "id_product": "0x0AFE", "battery": {"status": "BATTERY_CHARGING", "level": 55}}]})";

    const auto result = parseHeadsetControlBattery(bytes(json));

    ASSERT_TRUE(result.has_value());
    const auto& parsed = result.value();
    if (!parsed.has_value()) {
        FAIL() << "sem leitura";
    }
    EXPECT_EQ(parsed.value(), reading(ChargeState::Charging, 55));
}

void expectMalformed(const std::string& json)
{
    const auto result = parseHeadsetControlBattery(bytes(json));

    ASSERT_FALSE(result.has_value()) << json;
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData) << json;
}

TEST(HeadsetControlJson, RejectsNonJson)
{
    expectMalformed("headsetcontrol: not json");
    expectMalformed("");
    expectMalformed("[1, 2]");
}

TEST(HeadsetControlJson, RejectsMissingDevices)
{
    expectMalformed(R"({"api_version": "1.5", "device_count": 0})");
    expectMalformed(R"({"api_version": "1.5", "devices": {}})");
}

TEST(HeadsetControlJson, RejectsMissingLevel)
{
    expectMalformed(g733Json(R"({"status": "BATTERY_AVAILABLE"})"));
    expectMalformed(g733Json(R"({"status": "BATTERY_AVAILABLE", "level": "40"})"));
    expectMalformed(g733Json(R"({"status": "BATTERY_CHARGING", "level": 40.5})"));
    expectMalformed(g733Json(R"("sem objeto battery")"));
}

TEST(HeadsetControlJson, RejectsLevelOutOfRange)
{
    expectMalformed(g733Json(R"({"status": "BATTERY_AVAILABLE", "level": 150})"));
    expectMalformed(g733Json(R"({"status": "BATTERY_CHARGING", "level": -1})"));
}

TEST(HeadsetControlJson, RejectsUnknownStatus)
{
    expectMalformed(g733Json(R"({"status": "UNKNOWN", "level": 40})"));
    expectMalformed(g733Json(R"({"status": "BATTERY_TIMEOUT", "level": -1})"));
    expectMalformed(g733Json(R"({"level": 40})"));
}

TEST(HeadsetControlJson, RejectsIncompatibleApiVersion)
{
    const auto* const battery = R"({"status": "BATTERY_AVAILABLE", "level": 40})";
    expectMalformed(g733Json(battery, "2.0"));
    expectMalformed(g733Json(battery, "10.1"));
    expectMalformed(g733Json(battery, ""));
    expectMalformed(R"({"device_count": 1, "devices": [{"id_vendor": "0x046d", "id_product": "0x0afe", )"
                    R"("battery": {"status": "BATTERY_AVAILABLE", "level": 40}}]})");
}

TEST(HeadsetControlJson, AcceptsOtherMinorVersionsOfApi1)
{
    const auto result =
        parseHeadsetControlBattery(bytes(g733Json(R"({"status": "BATTERY_AVAILABLE", "level": 40})", "1.0")));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_value());
}

} // namespace
