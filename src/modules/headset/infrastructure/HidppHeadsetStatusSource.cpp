#include "modules/headset/infrastructure/HidppHeadsetStatusSource.hpp"

#include "modules/headset/infrastructure/G733StatusPolicy.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ghub::headset::infrastructure {

namespace {

using platform::hidpp::HidppChannel;
using platform::hidpp::HidppError;
using platform::hidpp::HidppErrorKind;

constexpr std::uint16_t featureEqualizer = 0x8310;
constexpr std::uint16_t featureUninterpreted = 0x8300;
// Prazo por pedido de ADR-0025 decisão 4 (G915/G703 dormindo levaram ~1,1 s; o G733 usa o mesmo prazo).
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
        // Com o G733 desligado a resposta é o erro 0x05 (pista da captura; ADR-0023).
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

HeadsetIoOpener headsetHidrawOpener()
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

HidppHeadsetStatusSource::HidppHeadsetStatusSource(platform::hidpp::HidppNodeLocator locator,
                                                   const shared_kernel::Clock& clock, HeadsetIoOpener opener)
    : locator_(std::move(locator)), clock_(&clock), opener_(std::move(opener))
{
}

shared_kernel::Result<HeadsetStatus> HidppHeadsetStatusSource::read()
{
    const auto node = locator_.locateG733();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto io = opener_(node->devicePath);
    if (!io.has_value()) {
        return std::unexpected(io.error());
    }
    const G733StatusPolicy policy; // antes do canal: a política precisa viver mais que ele
    HidppChannel channel{std::move(*io), *clock_, policy, requestTimeout};
    const std::uint8_t deviceIndex = node->deviceIndex;
    HeadsetStatus status;

    // nullopt = feature ausente ("não informado"); erro = qualquer outra falha.
    const auto call = [&](std::uint16_t feature) -> std::expected<std::optional<HidppChannel::Response>, HidppError> {
        const auto response = channel.call(deviceIndex, feature, 0);
        if (response.has_value()) {
            return std::optional<HidppChannel::Response>{*response};
        }
        if (response.error().kind == HidppErrorKind::FeatureNotPresent) {
            return std::optional<HidppChannel::Response>{};
        }
        return std::unexpected(response.error());
    };

    const auto equalizer = call(featureEqualizer);
    if (!equalizer.has_value()) {
        return std::unexpected(toDomainError(equalizer.error()));
    }
    if (equalizer->has_value()) {
        auto parsed = parseEqualizerInfo(**equalizer);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        status.equalizer = *parsed;
    }

    const auto raw = call(featureUninterpreted);
    if (!raw.has_value()) {
        return std::unexpected(toDomainError(raw.error()));
    }
    if (raw->has_value()) {
        auto parsed = parseUninterpretedRead(**raw);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        status.uninterpreted = *parsed;
    }
    return status;
}

} // namespace ghub::headset::infrastructure
