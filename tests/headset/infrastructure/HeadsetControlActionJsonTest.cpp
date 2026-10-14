#include "modules/headset/infrastructure/HeadsetControlActionJson.hpp"

#include <QByteArray>

#include <gtest/gtest.h>

#include <string>

using ghub::headset::parseHeadsetControlAction;
using ghub::shared_kernel::ErrorCode;

namespace {

// Formato de `headsetcontrol -o json` (HeadsetControl 4.1.0): `actions` fica na raiz, entre `hidapi_version` e
// `device_count` (cli/output/output_data.hpp:289-310); cada ação é {capability, device, status, [value > 0],
// [error_message não vazio]} (output_data.hpp:89-102); capability é o nome do enum (lib/device.cpp:17-25);
// api_version "1.5" (cli/output/output.cpp:29); status "success"/"failure" (output_data.hpp:26-38).
QByteArray document(const std::string& actions, int deviceCount = 1, const std::string& apiVersion = "1.5")
{
    const std::string actionsField = actions.empty() ? "" : R"("actions": [)" + actions + "], ";
    return QByteArray::fromStdString(R"({"name": "HeadsetControl", "version": "4.1.0", "api_version": ")" + apiVersion +
                                     R"(", "hidapi_version": "0.15.0", )" + actionsField + R"("device_count": )" +
                                     std::to_string(deviceCount) +
                                     R"(, "devices": [{"status": "success", "device": "Logitech G733"}]})");
}

constexpr auto sidetoneSuccess =
    R"({"capability": "CAP_SIDETONE", "device": "Logitech G733 Gaming Headset", "status": "success", "value": 40})";
constexpr auto lightsOffSuccess =
    R"({"capability": "CAP_LIGHTS", "device": "Logitech G733 Gaming Headset", "status": "success"})";

} // namespace

TEST(HeadsetControlActionJson, SuccessWithValue)
{
    EXPECT_TRUE(parseHeadsetControlAction(document(sidetoneSuccess), "CAP_SIDETONE").has_value());
}

TEST(HeadsetControlActionJson, SuccessWithoutValueWhenParameterIsZero)
{
    EXPECT_TRUE(parseHeadsetControlAction(document(lightsOffSuccess), "CAP_LIGHTS").has_value());
}

TEST(HeadsetControlActionJson, FailureIsBackendUnavailableWithTheErrorMessage)
{
    const auto json = document(
        R"({"capability": "CAP_LIGHTS", "device": "G733", "status": "failure", "error_message": "hid_write falhou"})");

    const auto result = parseHeadsetControlAction(json, "CAP_LIGHTS");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("hid_write falhou"), std::string::npos);
}

TEST(HeadsetControlActionJson, FailureMessageIsTruncatedToTwoHundredCharacters)
{
    const std::string longMessage(500, 'x');
    const auto json = document(R"({"capability": "CAP_SIDETONE", "device": "G733", "status": "failure", )"
                               R"("error_message": ")" +
                               longMessage + R"("})");

    const auto result = parseHeadsetControlAction(json, "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_LE(result.error().detail.size(), std::size_t{200});
}

TEST(HeadsetControlActionJson, FailureWithoutMessageStillFails)
{
    const auto json = document(R"({"capability": "CAP_SIDETONE", "device": "G733", "status": "failure"})");

    const auto result = parseHeadsetControlAction(json, "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
}

TEST(HeadsetControlActionJson, InvalidJsonIsMalformed)
{
    const auto result = parseHeadsetControlAction("não é json", "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST(HeadsetControlActionJson, JsonArrayInsteadOfObjectIsMalformed)
{
    const auto result = parseHeadsetControlAction("[]", "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST(HeadsetControlActionJson, MissingApiVersionIsMalformed)
{
    const auto result = parseHeadsetControlAction(
        R"({"actions": [{"capability": "CAP_SIDETONE", "status": "success"}], "device_count": 1})", "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST(HeadsetControlActionJson, ApiVersionTwoIsMalformed)
{
    const auto result = parseHeadsetControlAction(document(sidetoneSuccess, 1, "2.0"), "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST(HeadsetControlActionJson, ApiVersionOneWithOtherMinorIsAccepted)
{
    EXPECT_TRUE(parseHeadsetControlAction(document(sidetoneSuccess, 1, "1.9"), "CAP_SIDETONE").has_value());
}

TEST(HeadsetControlActionJson, ZeroDevicesIsHeadsetNotFound)
{
    const auto result = parseHeadsetControlAction(document("", 0), "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "headset não encontrado");
}

TEST(HeadsetControlActionJson, MissingDeviceCountIsMalformed)
{
    const auto result = parseHeadsetControlAction(
        R"({"api_version": "1.5", "actions": [{"capability": "CAP_SIDETONE", "status": "success"}]})", "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST(HeadsetControlActionJson, MissingActionsIsMalformed)
{
    const auto result = parseHeadsetControlAction(document(""), "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST(HeadsetControlActionJson, ActionOfAnotherCapabilityIsMalformed)
{
    const auto result = parseHeadsetControlAction(document(lightsOffSuccess), "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST(HeadsetControlActionJson, PicksTheRequestedCapabilityAmongSeveralActions)
{
    const auto json =
        document(std::string{lightsOffSuccess} + "," +
                 R"({"capability": "CAP_SIDETONE", "device": "G733", "status": "failure", "error_message": "ruim"})");

    EXPECT_TRUE(parseHeadsetControlAction(json, "CAP_LIGHTS").has_value());
    EXPECT_FALSE(parseHeadsetControlAction(json, "CAP_SIDETONE").has_value());
}

TEST(HeadsetControlActionJson, UnknownStatusIsMalformed)
{
    const auto json = document(R"({"capability": "CAP_SIDETONE", "device": "G733", "status": "partial"})");

    const auto result = parseHeadsetControlAction(json, "CAP_SIDETONE");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}
