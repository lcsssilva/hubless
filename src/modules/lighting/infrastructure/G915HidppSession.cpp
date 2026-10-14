#include "modules/lighting/infrastructure/G915HidppSession.hpp"

#include "modules/lighting/infrastructure/G915EffectFrames.hpp"
#include "modules/lighting/infrastructure/G915PerKeyFrames.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

namespace ghub::lighting::infrastructure {

namespace {

constexpr std::uint16_t featureEffects = 0x8071;
constexpr std::uint16_t featurePerKey = 0x8081;
constexpr std::uint16_t featureReconnect = 0x1D4B;
constexpr std::uint8_t shortReportId = 0x10;
constexpr std::uint8_t longReportId = 0x11;
constexpr std::uint8_t receiverConnectionNotice = 0x41;
constexpr std::uint8_t swControlFunction = 5;
constexpr std::uint8_t commitFunction = 7;
constexpr std::uint8_t getInfoFunction = 0;
constexpr std::uint8_t setEffectFunction = 1;
// Byte final do SetEffect: 01 como OpenRGB/Solaar; com 00 o firmware aceita sem aplicar (OpenRGB
// RGBController_LogitechHIDPP20.cpp:3180-3192@81bbe18a). Nesta unidade o efeito vale só sob controle do host.
constexpr std::uint8_t persistFlag = 0x01;
constexpr std::uint8_t infoSelector = 0xFF;
constexpr std::size_t maxEffectsPerCluster = 16;
// Localização do cluster no GetInfo (evidência de 2026-10-14): 0x0001 teclado, 0x0002 logo. Cluster 1 = teclado e
// cluster 0 = logo (OpenRGB LogitechG915Controller.h:23-24).
constexpr std::uint16_t locationKeys = 0x0001;
constexpr std::uint16_t locationLogo = 0x0002;

// SetSWControl [set=1, modo=3, bits]: efeito (0x01) + NV config (0x04) sob o host; energia (0x02) fica com o
// firmware (ADR-0013 decisão 4; bits de Solaar 1.1.20 rgb_power.py:48-56). O OpenRGB envia 0x07
// (LogitechG915Controller.cpp:212-214); o valor aceito pelo G915 é conferido no teste físico C.
constexpr std::array<std::uint8_t, 3> takeControl{0x01, 0x03, 0x05};
constexpr std::array<std::uint8_t, 3> releaseControl{0x01, 0x00, 0x00};

std::uint16_t bigEndian(std::uint8_t high, std::uint8_t low) noexcept
{
    return static_cast<std::uint16_t>((static_cast<unsigned>(high) * 256U) + low);
}

shared_kernel::DomainError toDomainError(const platform::hidpp::HidppError& error)
{
    switch (error.kind) {
    case platform::hidpp::HidppErrorKind::PolicyDenied:
    case platform::hidpp::HidppErrorKind::InvalidArgument:
        return {.code = shared_kernel::ErrorCode::InvalidArgument, .detail = error.detail};
    case platform::hidpp::HidppErrorKind::DeviceErrorHidpp20:
    case platform::hidpp::HidppErrorKind::DeviceErrorHidpp10:
        return {
            .code = shared_kernel::ErrorCode::BackendUnavailable,
            .detail = error.detail + " (código " + std::to_string(error.code) + ")",
        };
    case platform::hidpp::HidppErrorKind::FeatureNotPresent:
    case platform::hidpp::HidppErrorKind::Timeout:
    case platform::hidpp::HidppErrorKind::Io:
        break;
    }
    return {.code = shared_kernel::ErrorCode::BackendUnavailable, .detail = error.detail};
}

} // namespace

shared_kernel::Result<void> G915HidppSession::beginLive()
{
    auto taken = send(featureEffects, swControlFunction, takeControl);
    if (!taken.has_value()) {
        return taken;
    }
    // Só leitura (Root fn0). Ausente ou com erro: apenas o critério (a) vale; não é falha do modo ao vivo.
    const auto index = channel_->featureIndex(deviceIndex_, featureReconnect);
    restIndex_ = index.has_value() ? std::optional{*index} : std::nullopt;
    controlLost_ = false;
    channel_->setNotificationHandler([this](std::span<const std::uint8_t> report) { onNotification(report); });
    return taken;
}

shared_kernel::Result<bool> G915HidppSession::controlLost()
{
    if (const auto polled = channel_->pollNotifications(); !polled.has_value()) {
        return std::unexpected(toDomainError(polled.error()));
    }
    return controlLost_;
}

void G915HidppSession::onNotification(std::span<const std::uint8_t> report) noexcept
{
    if (report.size() < 4) {
        return;
    }
    // O tamanho foi conferido acima: os índices 0..3 são válidos.
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (report[1] != deviceIndex_) {
        return;
    }
    // (a) 10 <dev> 41 ...: link do teclado com o receptor mudou (repouso ou despertar), qualquer flag.
    if (report[0] == shortReportId && report[2] == receiverConnectionNotice) {
        controlLost_ = true;
        return;
    }
    // (b) 11 <dev> <idx 0x1D4B> 00 ...: evento 0 da feature, reconexão do teclado.
    if (report[0] == longReportId && restIndex_.has_value() && report[2] == *restIndex_ && report[3] == 0x00) {
        controlLost_ = true;
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
}

shared_kernel::Result<void> G915HidppSession::endLive()
{
    return send(featureEffects, swControlFunction, releaseControl);
}

shared_kernel::Result<void> G915HidppSession::setEffect(domain::KeyboardZone zone, const domain::KeyboardEffect& effect)
{
    const bool keys = zone == domain::KeyboardZone::Keys;
    const std::uint8_t cluster = keys ? 1 : 0;
    const std::uint16_t location = keys ? locationKeys : locationLogo;
    const auto encoded = encodeG915Effect(effect);

    // O índice vem do GetInfo do cluster (R10); efeito ausente -> nada é escrito.
    const auto index = effectIndex(cluster, location, encoded.effectId);
    if (!index.has_value()) {
        return std::unexpected(index.error());
    }
    std::array<std::uint8_t, 2 + std::tuple_size_v<decltype(encoded.params)> + 1> frame{};
    frame[0] = cluster;
    frame[1] = *index;
    for (std::size_t i = 0; i < encoded.params.size(); ++i) {
        frame.at(2 + i) = encoded.params.at(i);
    }
    frame.back() = persistFlag;
    return send(featureEffects, setEffectFunction, frame);
}

shared_kernel::Result<std::uint8_t> G915HidppSession::effectIndex(std::uint8_t cluster, std::uint16_t location,
                                                                  std::uint16_t effectId)
{
    auto& cached = clusters_.at(cluster);
    if (!cached.has_value()) {
        auto read = readCluster(cluster, location);
        if (!read.has_value()) {
            return std::unexpected(read.error());
        }
        cached = std::move(*read);
    }
    const auto& ids = cached->ids;
    const auto found = std::ranges::find(ids, effectId);
    if (found == ids.end()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "o efeito não existe nesta zona do teclado");
    }
    return static_cast<std::uint8_t>(found - ids.begin());
}

shared_kernel::Result<G915HidppSession::ClusterEffects> G915HidppSession::readCluster(std::uint8_t cluster,
                                                                                      std::uint16_t location)
{
    const std::array<std::uint8_t, 3> summaryQuery{cluster, infoSelector, 0x00};
    const auto summary = channel_->call(deviceIndex_, featureEffects, getInfoFunction, summaryQuery);
    if (!summary.has_value()) {
        return std::unexpected(toDomainError(summary.error()));
    }
    const auto reported = bigEndian((*summary)[2], (*summary)[3]);
    const std::size_t count = (*summary)[4];
    if ((*summary)[0] != cluster || reported != location || count == 0 || count > maxEffectsPerCluster) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "GetInfo do cluster de iluminação diferente do esperado");
    }
    ClusterEffects effects;
    for (std::size_t i = 0; i < count; ++i) {
        const std::array<std::uint8_t, 3> query{cluster, static_cast<std::uint8_t>(i), 0x00};
        const auto info = channel_->call(deviceIndex_, featureEffects, getInfoFunction, query);
        if (!info.has_value()) {
            return std::unexpected(toDomainError(info.error()));
        }
        if ((*info)[0] != cluster || (*info)[1] != i) {
            return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                       "GetInfo de efeito de iluminação diferente do esperado");
        }
        effects.ids.push_back(bigEndian((*info)[2], (*info)[3]));
    }
    return effects;
}

shared_kernel::Result<void> G915HidppSession::paint(const domain::KeyColorMap& colors)
{
    const auto frames = buildG915PerKeyFrames(colors);
    if (frames.empty()) {
        return {};
    }
    // Para no primeiro erro: nada é reenviado e o commit não sai (R11).
    for (const auto& frame : frames) {
        if (auto sent = send(featurePerKey, frame.function, frame.params); !sent.has_value()) {
            return sent;
        }
    }
    return send(featurePerKey, commitFunction, {});
}

shared_kernel::Result<void> G915HidppSession::send(std::uint16_t featureId, std::uint8_t function,
                                                   std::span<const std::uint8_t> params)
{
    const auto reply = channel_->call(deviceIndex_, featureId, function, params);
    if (!reply.has_value()) {
        return std::unexpected(toDomainError(reply.error()));
    }
    return {};
}

} // namespace ghub::lighting::infrastructure
