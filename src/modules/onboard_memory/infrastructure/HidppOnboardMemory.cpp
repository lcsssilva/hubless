#include "modules/onboard_memory/infrastructure/HidppOnboardMemory.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace ghub::onboard_memory::infrastructure {

namespace {

namespace hidpp = platform::hidpp;

constexpr std::uint16_t onboardFeature = 0x8100;
constexpr std::uint16_t chunkSize = 16;
constexpr std::uint16_t maxSectorSize = 4096;

shared_kernel::DomainError mapError(const hidpp::HidppError& error)
{
    using hidpp::HidppErrorKind;
    switch (error.kind) {
    case HidppErrorKind::PolicyDenied:
    case HidppErrorKind::InvalidArgument:
        return {.code = shared_kernel::ErrorCode::InvalidArgument, .detail = error.detail};
    case HidppErrorKind::DeviceErrorHidpp10:
    case HidppErrorKind::DeviceErrorHidpp20:
        return {
            .code = shared_kernel::ErrorCode::BackendUnavailable,
            .detail = error.detail + " (código " + std::to_string(error.code) + ")",
        };
    case HidppErrorKind::FeatureNotPresent:
    case HidppErrorKind::Timeout:
    case HidppErrorKind::Io:
        break;
    }
    return {.code = shared_kernel::ErrorCode::BackendUnavailable, .detail = error.detail};
}

std::uint16_t joinBigEndian(std::uint8_t highByte, std::uint8_t lowByte)
{
    return static_cast<std::uint16_t>((static_cast<unsigned>(highByte) << 8U) | lowByte);
}

std::uint8_t high(unsigned value)
{
    return static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

std::uint8_t low(unsigned value)
{
    return static_cast<std::uint8_t>(value & 0xFFU);
}

} // namespace

HidppOnboardMemory::HidppOnboardMemory(std::unique_ptr<hidpp::HidReportIo> io, const shared_kernel::Clock& clock,
                                       std::uint8_t deviceIndex, std::unique_ptr<OnboardProfilePolicy> policy)
    : policy_(std::move(policy)), policyView_(*policy_), channel_(std::move(io), clock, policyView_),
      deviceIndex_(deviceIndex)
{
    policy_->setProfileCount(0); // nenhuma escrita de perfil até um describe() bem-sucedido
}

shared_kernel::Result<hidpp::HidppChannel::Response> HidppOnboardMemory::call(std::uint8_t function,
                                                                              std::span<const std::uint8_t> params)
{
    auto response = channel_.call(deviceIndex_, onboardFeature, function, params);
    if (!response.has_value()) {
        return std::unexpected(mapError(response.error()));
    }
    return *response;
}

shared_kernel::Result<OnboardLayout> HidppOnboardMemory::describe()
{
    const auto response = call(0);
    if (!response.has_value()) {
        return std::unexpected(response.error());
    }
    const auto& bytes = *response;
    const OnboardLayout layout{
        .sectorSize = joinBigEndian(std::get<7>(bytes), std::get<8>(bytes)),
        .profileCount = std::get<3>(bytes),
    };
    layout_ = layout;
    policy_->setProfileCount(layout.profileCount);
    return layout;
}

shared_kernel::Result<std::uint8_t> HidppOnboardMemory::onboardMode()
{
    const auto response = call(2);
    if (!response.has_value()) {
        return std::unexpected(response.error());
    }
    return std::get<0>(*response);
}

shared_kernel::Result<std::uint16_t> HidppOnboardMemory::currentProfileSector()
{
    const auto response = call(4);
    if (!response.has_value()) {
        return std::unexpected(response.error());
    }
    return joinBigEndian(std::get<0>(*response), std::get<1>(*response));
}

shared_kernel::Result<std::vector<std::uint8_t>> HidppOnboardMemory::readSector(std::uint16_t sector,
                                                                                std::uint16_t size)
{
    if (size < chunkSize || size > maxSectorSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "tamanho de setor fora de 16..4096");
    }
    std::vector<std::uint8_t> data(size, 0);
    for (unsigned offset = 0; offset < size; offset += chunkSize) {
        // O último trecho curto é relido a partir de size-16 (libratbag hidpp20.c:1900-1915).
        offset = (size - offset < chunkSize) ? size - chunkSize : offset;
        const std::array<std::uint8_t, 4> params{high(sector), low(sector), high(offset), low(offset)};
        const auto response = call(5, params);
        if (!response.has_value()) {
            return std::unexpected(response.error());
        }
        std::ranges::copy(*response, data.begin() + offset);
    }
    return data;
}

shared_kernel::Result<void> HidppOnboardMemory::writeSector(std::uint16_t sector, std::span<const std::uint8_t> bytes)
{
    if (!layout_.has_value()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "describe() não foi executado");
    }
    if (bytes.size() != layout_->sectorSize || bytes.size() < chunkSize || bytes.size() > maxSectorSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "tamanho dos bytes diferente do sector_size do mouse");
    }
    const auto size = static_cast<unsigned>(bytes.size());
    const std::array<std::uint8_t, 6> address{high(sector), low(sector), 0, 0, high(size), low(size)};
    if (const auto sent = call(6, address); !sent.has_value()) {
        return std::unexpected(sent.error());
    }
    for (std::size_t offset = 0; offset < bytes.size(); offset += chunkSize) {
        std::array<std::uint8_t, chunkSize> block{}; // o que passar do fim do setor segue como 0x00
        const auto count = std::min<std::size_t>(chunkSize, bytes.size() - offset);
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), count, block.begin());
        if (const auto sent = call(7, block); !sent.has_value()) {
            return std::unexpected(sent.error());
        }
    }
    if (const auto sent = call(8); !sent.has_value()) {
        return std::unexpected(sent.error());
    }
    return {};
}

} // namespace ghub::onboard_memory::infrastructure

namespace ghub::onboard_memory {

std::unique_ptr<OnboardMemory> makeHidppOnboardMemory(std::unique_ptr<platform::hidpp::HidReportIo> io,
                                                      const shared_kernel::Clock& clock, std::uint8_t deviceIndex,
                                                      std::unique_ptr<OnboardProfilePolicy> policy)
{
    return std::make_unique<infrastructure::HidppOnboardMemory>(std::move(io), clock, deviceIndex, std::move(policy));
}

} // namespace ghub::onboard_memory
