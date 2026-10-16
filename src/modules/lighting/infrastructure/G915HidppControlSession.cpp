#include "modules/lighting/infrastructure/G915HidppControlSession.hpp"

#include <array>
#include <string>

namespace ghub::lighting::infrastructure {

namespace {

// Feature do modo de jogo: única definição fora da política (o teste de fonte do módulo permite este arquivo).
constexpr std::uint16_t featureGameMode = 0x4522;
constexpr std::uint16_t featureOnboardProfiles = 0x8100;
constexpr std::uint8_t gameModeSetFunction = 1;
constexpr std::uint8_t gameModeClearFunction = 3;
constexpr std::uint8_t selectProfileFunction = 3;
constexpr std::uint8_t currentProfileFunction = 4;
constexpr std::uint8_t memoryReadFunction = 5;
// Entradas do diretório (setor 0): [setor BE16][habilitado][reservado]; só as quatro primeiras cabem no primeiro
// trecho de 16 bytes, mais que os 3 perfis do G915.
constexpr std::size_t directoryEntrySize = 4;
constexpr std::size_t directoryEntries = 4;
constexpr std::uint16_t directorySector = 0x0000;

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

shared_kernel::Result<platform::hidpp::HidppChannel::Response>
G915HidppControlSession::call(std::uint16_t featureId, std::uint8_t function, std::span<const std::uint8_t> params)
{
    auto reply = channel_->call(deviceIndex_, featureId, function, params);
    if (!reply.has_value()) {
        return std::unexpected(toDomainError(reply.error()));
    }
    return *reply;
}

shared_kernel::Result<void> G915HidppControlSession::setGameMode(const domain::GameModeKeys& keys)
{
    // Para no primeiro erro: sem a limpeza confirmada, a lista não é enviada e nada é reenviado.
    if (const auto cleared = call(featureGameMode, gameModeClearFunction); !cleared.has_value()) {
        return std::unexpected(cleared.error());
    }
    if (const auto set = call(featureGameMode, gameModeSetFunction, keys.hidCodes()); !set.has_value()) {
        return std::unexpected(set.error());
    }
    return {};
}

shared_kernel::Result<void> G915HidppControlSession::selectSlot(domain::OnboardSlot slot)
{
    const std::array<std::uint8_t, 2> sector{0x00, slot.number()};
    if (const auto selected = call(featureOnboardProfiles, selectProfileFunction, sector); !selected.has_value()) {
        return std::unexpected(selected.error());
    }
    return {};
}

shared_kernel::Result<domain::OnboardSlots> G915HidppControlSession::readSlots()
{
    const std::array<std::uint8_t, 4> address{static_cast<std::uint8_t>(directorySector >> 8),
                                              static_cast<std::uint8_t>(directorySector & 0xFF), 0x00, 0x00};
    const auto directory = call(featureOnboardProfiles, memoryReadFunction, address);
    if (!directory.has_value()) {
        return std::unexpected(directory.error());
    }
    domain::OnboardSlots slots;
    for (std::size_t entry = 0; entry < directoryEntries; ++entry) {
        const std::size_t at = entry * directoryEntrySize;
        const std::uint8_t high = directory->at(at);
        const std::uint8_t low = directory->at(at + 1);
        if (high == 0xFF && low == 0xFF) {
            break; // fim da lista
        }
        const bool enabled = directory->at(at + 2) != 0;
        if (!enabled || high != 0) {
            continue; // desabilitada, ou setor fora de 0x00nn
        }
        if (const auto slot = domain::OnboardSlot::create(low); slot.has_value()) {
            slots.enabled.push_back(*slot);
        }
    }

    const auto current = call(featureOnboardProfiles, currentProfileFunction);
    if (!current.has_value()) {
        return std::unexpected(current.error());
    }
    if (current->at(0) == 0) {
        if (const auto active = domain::OnboardSlot::create(current->at(1)); active.has_value()) {
            slots.active = *active;
        }
    }
    return slots;
}

} // namespace ghub::lighting::infrastructure
