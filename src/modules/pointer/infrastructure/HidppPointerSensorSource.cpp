#include "modules/pointer/infrastructure/HidppPointerSensorSource.hpp"

#include "modules/pointer/infrastructure/G703OnboardPolicy.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ghub::pointer::infrastructure {

namespace {

using platform::hidpp::HidppChannel;
using platform::hidpp::HidppError;
using platform::hidpp::HidppErrorKind;

constexpr std::uint16_t featureSensorDpi = 0x2201;
constexpr std::uint16_t featureReportRate = 0x8060;
// G915/G703 dormindo levaram ~1,1 s para a primeira resposta (INC-015 parte 1); ADR-0025 decisão 4.
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
        return {.code = ErrorCode::InvalidArgument, .detail = "pedido HID++ recusado pela política de leitura"};
    case HidppErrorKind::FeatureNotPresent:
        return {.code = ErrorCode::MalformedData, .detail = "feature HID++ ausente no dispositivo"};
    case HidppErrorKind::Timeout:
        return {.code = ErrorCode::BackendUnavailable, .detail = "sem resposta (dispositivo dormindo ou desligado?)"};
    case HidppErrorKind::Io:
        return {.code = ErrorCode::BackendUnavailable, .detail = error.detail};
    case HidppErrorKind::DeviceErrorHidpp20:
        return {.code = ErrorCode::BackendUnavailable,
                .detail = "o dispositivo respondeu erro HID++ 2.0 (código " + hexCode(error.code) + ")"};
    case HidppErrorKind::DeviceErrorHidpp10:
        return {.code = ErrorCode::BackendUnavailable,
                .detail = "o dispositivo respondeu erro HID++ 1.0 (código " + hexCode(error.code) + ")"};
    }
    return {.code = ErrorCode::BackendUnavailable, .detail = "falha desconhecida na leitura HID++"};
}

} // namespace

SensorIoOpener sensorHidrawOpener()
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

HidppPointerSensorSource::HidppPointerSensorSource(platform::hidpp::HidppNodeLocator locator,
                                                   const shared_kernel::Clock& clock, SensorIoOpener opener)
    : locator_(std::move(locator)), clock_(&clock), opener_(std::move(opener))
{
}

shared_kernel::Result<PointerSensorStatus> HidppPointerSensorSource::read()
{
    const auto node = locator_.locateG703();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto io = opener_(node->devicePath);
    if (!io.has_value()) {
        return std::unexpected(io.error());
    }
    const G703OnboardPolicy policy; // antes do canal: a política precisa viver mais que ele
    HidppChannel channel{std::move(*io), *clock_, policy, requestTimeout};
    const std::uint8_t deviceIndex = node->deviceIndex;
    PointerSensorStatus status;

    // nullopt = feature ausente ("não informado"); erro = qualquer outra falha.
    const auto call =
        [&](std::uint16_t feature, std::uint8_t function,
            std::span<const std::uint8_t> params) -> std::expected<std::optional<HidppChannel::Response>, HidppError> {
        const auto response = channel.call(deviceIndex, feature, function, params);
        if (response.has_value()) {
            return std::optional<HidppChannel::Response>{*response};
        }
        if (response.error().kind == HidppErrorKind::FeatureNotPresent) {
            return std::optional<HidppChannel::Response>{};
        }
        return std::unexpected(response.error());
    };

    constexpr std::array<std::uint8_t, 1> sensorZero{0x00};
    const auto list = call(featureSensorDpi, 1, sensorZero);
    if (!list.has_value()) {
        return std::unexpected(toDomainError(list.error()));
    }
    if (list->has_value()) {
        status.dpiList = DpiList::parse(**list); // lista inválida não é erro: a janela mostra os bytes brutos
        const auto setting = call(featureSensorDpi, 2, sensorZero);
        if (!setting.has_value()) {
            return std::unexpected(toDomainError(setting.error()));
        }
        if (setting->has_value()) {
            auto parsed = parseDpiSetting(**setting);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            status.dpiSetting = *parsed;
        }
    }

    const auto rates = call(featureReportRate, 0, {});
    if (!rates.has_value()) {
        return std::unexpected(toDomainError(rates.error()));
    }
    if (rates->has_value()) {
        auto parsed = parseReportRateSupport(**rates);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        status.reportRateSupport = *parsed;
        const auto period = call(featureReportRate, 1, {});
        if (!period.has_value()) {
            return std::unexpected(toDomainError(period.error()));
        }
        if (period->has_value()) {
            auto value = parseReportPeriodMs(**period);
            if (!value.has_value()) {
                return std::unexpected(value.error());
            }
            status.currentPeriodMs = *value;
        }
    }
    return status;
}

} // namespace ghub::pointer::infrastructure
