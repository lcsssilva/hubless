#include "modules/headset/infrastructure/HidppHeadsetStatusSource.hpp"

#include "modules/headset/infrastructure/G733StatusPolicy.hpp"
#include "modules/headset/infrastructure/HidppHeadsetErrors.hpp"
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
