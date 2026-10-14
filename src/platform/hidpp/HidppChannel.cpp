#include "platform/hidpp/HidppChannel.hpp"

#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace ghub::platform::hidpp {

namespace {

constexpr std::uint8_t longReportId = 0x11;
constexpr std::uint8_t rootFeatureIndex = 0;
constexpr std::uint8_t hidpp20ErrorMarker = 0xFF;
constexpr std::uint8_t hidpp10ErrorMarker = 0x8F;
constexpr std::size_t readBufferSize = 64;

HidppError makeError(HidppErrorKind kind, std::string detail, std::uint8_t code = 0)
{
    return HidppError{.kind = kind, .code = code, .detail = std::move(detail)};
}

HidppError fromIo(const HidIoError& error)
{
    if (error.failure == HidIoFailure::TimedOut) {
        return makeError(HidppErrorKind::Timeout, "prazo esgotado sem resposta");
    }
    return makeError(HidppErrorKind::Io, "falha de E/S no hidraw (errno " + std::to_string(error.systemError) + ")");
}

} // namespace

HidppChannel::HidppChannel(std::unique_ptr<HidReportIo> io, const shared_kernel::Clock& clock,
                           const HidppCommandPolicy& policy, std::chrono::milliseconds timeout, HidppTraceSink trace)
    : io_(std::move(io)), clock_(clock), policy_(policy), timeout_(timeout), trace_(std::move(trace))
{
}

std::expected<HidppChannel::Response, HidppError> HidppChannel::call(std::uint8_t deviceIndex, std::uint16_t featureId,
                                                                     std::uint8_t function,
                                                                     std::span<const std::uint8_t> params)
{
    auto result = perform(deviceIndex, featureId, function, params);
    if (trace_) {
        HidppCommandTrace trace{
            .deviceIndex = deviceIndex,
            .featureId = featureId,
            .function = function,
            .paramCount = params.size(),
            .failure = std::nullopt,
        };
        if (!result.has_value()) {
            trace.failure = result.error().kind;
            trace.errorCode = result.error().code;
        }
        trace_(trace);
    }
    return result;
}

std::expected<HidppChannel::Response, HidppError> HidppChannel::perform(std::uint8_t deviceIndex,
                                                                        std::uint16_t featureId, std::uint8_t function,
                                                                        std::span<const std::uint8_t> params)
{
    // (a) política antes de qualquer write, inclusive da consulta GetFeature.
    if (!policy_.isAllowed(featureId, function, params)) {
        return std::unexpected(
            makeError(HidppErrorKind::PolicyDenied, "par (feature, função) fora da lista permitida"));
    }
    if (params.size() > maxParams) {
        return std::unexpected(makeError(HidppErrorKind::InvalidArgument, "mais de 16 bytes de parâmetros"));
    }
    const auto deadline = clock_.now() + timeout_;
    // (b) índice da feature, resolvido pela Root nesta sessão.
    const auto featureIndex = resolve(deviceIndex, featureId, deadline);
    if (!featureIndex.has_value()) {
        return std::unexpected(featureIndex.error());
    }
    return exchange(deviceIndex, *featureIndex, function, params, deadline);
}

void HidppChannel::notify(std::span<const std::uint8_t> report) const
{
    if (handler_) {
        handler_(report);
    }
}

std::expected<void, HidppError> HidppChannel::pollNotifications()
{
    std::array<std::uint8_t, readBufferSize> buffer{};
    for (;;) {
        const auto got = io_->read(buffer, std::chrono::milliseconds{0});
        if (!got.has_value()) {
            if (got.error().failure == HidIoFailure::TimedOut) {
                return {}; // nada pendente
            }
            return std::unexpected(fromIo(got.error()));
        }
        notify(std::span<const std::uint8_t>{buffer}.first(*got));
    }
}

std::expected<std::uint8_t, HidppError> HidppChannel::featureIndex(std::uint8_t deviceIndex, std::uint16_t featureId)
{
    return resolve(deviceIndex, featureId, clock_.now() + timeout_);
}

std::expected<std::uint8_t, HidppError> HidppChannel::resolve(std::uint8_t deviceIndex, std::uint16_t featureId,
                                                              shared_kernel::Instant deadline)
{
    if (featureId == 0x0000) {
        return rootFeatureIndex; // Root é sempre o índice 0 por especificação.
    }
    auto cached = features_.find(deviceIndex, featureId);
    if (!cached.has_value()) {
        const std::array<std::uint8_t, 2> query{static_cast<std::uint8_t>(featureId >> 8),
                                                static_cast<std::uint8_t>(featureId & 0xFF)};
        if (!policy_.isAllowed(0x0000, 0, query)) {
            return std::unexpected(makeError(HidppErrorKind::PolicyDenied, "Root GetFeature fora da lista"));
        }
        const auto response = exchange(deviceIndex, rootFeatureIndex, 0, query, deadline);
        if (!response.has_value()) {
            return std::unexpected(response.error());
        }
        features_.record(deviceIndex, featureId, (*response)[0]);
        cached = (*response)[0];
    }
    if (*cached == 0) {
        return std::unexpected(makeError(HidppErrorKind::FeatureNotPresent, "feature ausente no dispositivo"));
    }
    return *cached;
}

std::expected<HidppChannel::Response, HidppError>
HidppChannel::exchange(std::uint8_t deviceIndex, std::uint8_t featureIndex, std::uint8_t function,
                       std::span<const std::uint8_t> params, shared_kernel::Instant deadline)
{
    const auto functionAndSoftwareId = static_cast<std::uint8_t>((function << 4) | softwareId);
    std::array<std::uint8_t, reportSize> frame{};
    frame[0] = longReportId;
    frame[1] = deviceIndex;
    frame[2] = featureIndex;
    frame[3] = functionAndSoftwareId;
    std::ranges::copy(params, frame.begin() + 4);

    if (const auto written = io_->write(frame); !written.has_value()) {
        return std::unexpected(fromIo(written.error()));
    }

    std::array<std::uint8_t, readBufferSize> buffer{};
    for (;;) {
        const auto remaining = deadline - clock_.now();
        if (remaining <= shared_kernel::Duration::zero()) {
            return std::unexpected(makeError(HidppErrorKind::Timeout, "prazo total esgotado"));
        }
        const auto wait = std::chrono::ceil<std::chrono::milliseconds>(remaining);
        const auto got = io_->read(buffer, wait);
        if (!got.has_value()) {
            return std::unexpected(fromIo(got.error()));
        }
        const auto size = *got;
        const auto report = std::span<const std::uint8_t>{buffer}.first(size);
        if (size < 6 || buffer[1] != deviceIndex) {
            notify(report);
            continue;
        }
        // Erro HID++ 1.0: 10 dev 8F idx fnsw code (aceita também em quadro longo).
        if ((buffer[0] == 0x10 || buffer[0] == longReportId) && buffer[2] == hidpp10ErrorMarker &&
            buffer[3] == featureIndex && buffer[4] == functionAndSoftwareId) {
            return std::unexpected(
                makeError(HidppErrorKind::DeviceErrorHidpp10, "erro HID++ 1.0 do dispositivo", buffer[5]));
        }
        if (buffer[0] != longReportId || size < reportSize) {
            notify(report);
            continue;
        }
        // Erro HID++ 2.0: 11 dev FF idx fnsw code.
        if (buffer[2] == hidpp20ErrorMarker && buffer[3] == featureIndex && buffer[4] == functionAndSoftwareId) {
            return std::unexpected(
                makeError(HidppErrorKind::DeviceErrorHidpp20, "erro HID++ 2.0 do dispositivo", buffer[5]));
        }
        if (buffer[2] == featureIndex && buffer[3] == functionAndSoftwareId) {
            Response response{};
            std::copy_n(buffer.begin() + 4, response.size(), response.begin());
            return response;
        }
        // Notificação, outro software ID ou outra feature: entregue ao observador e ignorada aqui.
        notify(report);
    }
}

} // namespace ghub::platform::hidpp
