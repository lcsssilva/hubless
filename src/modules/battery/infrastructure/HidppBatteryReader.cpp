#include "modules/battery/infrastructure/HidppBatteryReader.hpp"

#include "modules/battery/domain/AdcMeasurement.hpp"
#include "modules/battery/infrastructure/G733BatteryPolicy.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace ghub::battery {

namespace {

using platform::hidpp::HidppChannel;
using platform::hidpp::HidppError;
using platform::hidpp::HidppErrorKind;

constexpr std::uint16_t featureAdcMeasurement = 0x1F20;
constexpr std::chrono::milliseconds requestTimeout{2000};

std::string hexCode(std::uint8_t code)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    return std::string{"0x"} + digits.at(code >> 4U) + digits.at(code & 0x0FU);
}

shared_kernel::DomainError toDomainError(const HidppError& error)
{
    using shared_kernel::ErrorCode;
    switch (error.kind) {
    case HidppErrorKind::PolicyDenied:
    case HidppErrorKind::InvalidArgument:
        return {.code = ErrorCode::InvalidArgument, .detail = "pedido HID++ recusado pela política da bateria"};
    case HidppErrorKind::FeatureNotPresent:
        return {.code = ErrorCode::MalformedData, .detail = "feature de bateria (0x1f20) ausente no dispositivo"};
    case HidppErrorKind::Timeout:
        return {.code = ErrorCode::BackendUnavailable, .detail = "sem resposta (dispositivo dormindo ou desligado?)"};
    case HidppErrorKind::Io:
        return {.code = ErrorCode::BackendUnavailable, .detail = error.detail};
    case HidppErrorKind::DeviceErrorHidpp20:
        if (error.code == 0x05) {
            return {.code = ErrorCode::BackendUnavailable, .detail = "headset desligado ou fora de alcance"};
        }
        return {.code = ErrorCode::BackendUnavailable,
                .detail = "o dispositivo respondeu erro HID++ 2.0 (código " + hexCode(error.code) + ")"};
    case HidppErrorKind::DeviceErrorHidpp10:
        return {.code = ErrorCode::BackendUnavailable,
                .detail = "o dispositivo respondeu erro HID++ 1.0 (código " + hexCode(error.code) + ")"};
    }
    return {.code = ErrorCode::BackendUnavailable, .detail = "falha desconhecida na leitura HID++"};
}

} // namespace

BatteryIoOpener batteryHidrawOpener()
{
    return
        [](const std::filesystem::path& path) -> shared_kernel::Result<std::unique_ptr<platform::hidpp::HidReportIo>> {
            auto device = platform::hidpp::HidrawDevice::open(path);
            if (!device.has_value()) {
                return std::unexpected(device.error());
            }
            return std::unique_ptr<platform::hidpp::HidReportIo>{std::move(*device)};
        };
}

HidppBatteryReader::HidppBatteryReader(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                                       BatteryIoOpener opener)
    : locator_(std::move(locator)), clock_(&clock), opener_(std::move(opener))
{
}

shared_kernel::Result<BatteryReading> HidppBatteryReader::read()
{
    const auto node = locator_.locateG733();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto io = opener_(node->devicePath);
    if (!io.has_value()) {
        return std::unexpected(io.error());
    }
    const G733BatteryPolicy policy; // antes do canal: a política precisa viver mais que ele
    HidppChannel channel{std::move(*io), *clock_, policy, requestTimeout};
    const auto response = channel.call(node->deviceIndex, featureAdcMeasurement, 0);
    if (!response.has_value()) {
        return std::unexpected(toDomainError(response.error()));
    }
    return readingFromAdc(*response);
}

} // namespace ghub::battery
