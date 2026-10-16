#include "modules/lighting/infrastructure/HidppKeyboardStatusSource.hpp"

#include "modules/lighting/infrastructure/LightingHidppPolicy.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ghub::lighting::infrastructure {

namespace {

using platform::hidpp::HidppChannel;
using platform::hidpp::HidppError;
using platform::hidpp::HidppErrorKind;

constexpr std::uint16_t featureBrightness = 0x8040;
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

StatusIoOpener statusHidrawOpener()
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

HidppKeyboardStatusSource::HidppKeyboardStatusSource(platform::hidpp::HidppNodeLocator locator,
                                                     const shared_kernel::Clock& clock, StatusIoOpener opener)
    : locator_(std::move(locator)), clock_(&clock), opener_(std::move(opener))
{
}

shared_kernel::Result<KeyboardStatus> HidppKeyboardStatusSource::read()
{
    const auto node = locator_.locateG915();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto io = opener_(node->devicePath);
    if (!io.has_value()) {
        return std::unexpected(io.error());
    }
    const LightingHidppPolicy policy; // antes do canal: a política precisa viver mais que ele
    HidppChannel channel{std::move(*io), *clock_, policy, requestTimeout};
    const std::uint8_t deviceIndex = node->deviceIndex;
    KeyboardStatus status;

    // Lê uma função sem parâmetros. nullopt = feature ausente; erro = qualquer outra falha.
    const auto call = [&](std::uint16_t feature,
                          std::uint8_t function) -> std::expected<std::optional<HidppChannel::Response>, HidppError> {
        const auto response = channel.call(deviceIndex, feature, function);
        if (response.has_value()) {
            return std::optional<HidppChannel::Response>{*response};
        }
        if (response.error().kind == HidppErrorKind::FeatureNotPresent) {
            return std::optional<HidppChannel::Response>{};
        }
        return std::unexpected(response.error());
    };

    const auto limits = call(featureBrightness, 0);
    if (!limits.has_value()) {
        return std::unexpected(toDomainError(limits.error()));
    }
    if (limits->has_value()) {
        auto parsed = parseBrightnessLimits(**limits);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        status.brightnessLimits = *parsed;
        const auto current = call(featureBrightness, 1);
        if (!current.has_value()) {
            return std::unexpected(toDomainError(current.error()));
        }
        if (current->has_value()) {
            auto value = parseCurrentBrightness(**current);
            if (!value.has_value()) {
                return std::unexpected(value.error());
            }
            status.currentBrightness = *value;
        }
    }

    const auto rates = call(featureReportRate, 0);
    if (!rates.has_value()) {
        return std::unexpected(toDomainError(rates.error()));
    }
    if (rates->has_value()) {
        auto parsed = parseReportRateSupport(**rates);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        status.reportRateSupport = *parsed;
        const auto period = call(featureReportRate, 1);
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

} // namespace ghub::lighting::infrastructure
