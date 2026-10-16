#include "modules/headset/infrastructure/HidppHeadsetSettingsWriter.hpp"

#include "modules/headset/infrastructure/G733StatusPolicy.hpp"
#include "modules/headset/infrastructure/HidppHeadsetErrors.hpp"
#include "platform/hidpp/HidppChannel.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace ghub::headset::infrastructure {

namespace {

using platform::hidpp::HidppChannel;

constexpr std::uint16_t featureSidetone = 0x8300;
constexpr std::uint16_t featureLighting = 0x8070;
constexpr std::uint8_t functionSetSidetone = 1;
constexpr std::uint8_t functionSetLighting = 3;
constexpr std::chrono::milliseconds requestTimeout{2000};

} // namespace

HidppHeadsetSettingsWriter::HidppHeadsetSettingsWriter(platform::hidpp::HidppNodeLocator locator,
                                                       const shared_kernel::Clock& clock, HeadsetIoOpener opener)
    : locator_(std::move(locator)), clock_(&clock), opener_(std::move(opener))
{
}

namespace {

// Abre o canal do G733, executa `send(channel, deviceIndex)` e fecha (RAII) ao sair.
template <typename Send>
shared_kernel::Result<void> withChannel(const platform::hidpp::HidppNodeLocator& locator,
                                        const shared_kernel::Clock& clock, const HeadsetIoOpener& opener,
                                        const Send& send)
{
    const auto node = locator.locateG733();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto io = opener(node->devicePath);
    if (!io.has_value()) {
        return std::unexpected(io.error());
    }
    const G733StatusPolicy policy; // antes do canal: a política precisa viver mais que ele
    HidppChannel channel{std::move(*io), clock, policy, requestTimeout};
    return send(channel, node->deviceIndex);
}

shared_kernel::Result<void> sendLighting(HidppChannel& channel, std::uint8_t deviceIndex,
                                         const HeadsetLighting& lighting)
{
    const auto payload = lighting.payload();
    const auto response = channel.call(deviceIndex, featureLighting, functionSetLighting, payload);
    if (!response.has_value()) {
        return std::unexpected(toDomainError(response.error()));
    }
    return {};
}

} // namespace

shared_kernel::Result<void> HidppHeadsetSettingsWriter::applySidetone(SidetoneLevel level)
{
    return withChannel(locator_, *clock_, opener_,
                       [&](HidppChannel& channel, std::uint8_t deviceIndex) -> shared_kernel::Result<void> {
                           const std::array<std::uint8_t, 1> params{static_cast<std::uint8_t>(level.value())};
                           const auto response =
                               channel.call(deviceIndex, featureSidetone, functionSetSidetone, params);
                           if (!response.has_value()) {
                               return std::unexpected(toDomainError(response.error()));
                           }
                           return {};
                       });
}

shared_kernel::Result<void> HidppHeadsetSettingsWriter::applyLights(HeadsetLights lights)
{
    return withChannel(locator_, *clock_, opener_,
                       [&](HidppChannel& channel, std::uint8_t deviceIndex) -> shared_kernel::Result<void> {
                           for (const auto zone : {LightingZone::Lower, LightingZone::Upper}) {
                               const auto lighting = lights == HeadsetLights::On ? HeadsetLighting::defaultCycle(zone)
                                                                                 : HeadsetLighting::off(zone);
                               if (auto done = sendLighting(channel, deviceIndex, lighting); !done.has_value()) {
                                   return done;
                               }
                           }
                           return {};
                       });
}

shared_kernel::Result<void> HidppHeadsetSettingsWriter::applyLighting(HeadsetLighting lighting)
{
    return withChannel(locator_, *clock_, opener_,
                       [&](HidppChannel& channel, std::uint8_t deviceIndex) -> shared_kernel::Result<void> {
                           return sendLighting(channel, deviceIndex, lighting);
                       });
}

} // namespace ghub::headset::infrastructure
