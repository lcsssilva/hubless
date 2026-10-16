#include "modules/lighting/application/ControlKeyboard.hpp"

#include "modules/lighting/application/KeyboardAccess.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace ghub::lighting::application {

namespace {

constexpr const char* leasePurpose = "controles do teclado (modo de jogo e slots)";

} // namespace

ControlKeyboard::ControlKeyboard(KeyboardControl& control, device_access::AccessRegistry& registry,
                                 device_access::ExternalProcessProbe& probe,
                                 device_access::PhysicalInterfaceId interfaceId)
    : control_(&control), registry_(&registry), probe_(&probe), interfaceId_(std::move(interfaceId))
{
}

template <typename Result>
Result ControlKeyboard::run(const std::function<Result(KeyboardControlSession&)>& operation) const
{
    if (const auto free = detail::ensureNoExternalOwner(*probe_); !free.has_value()) {
        return std::unexpected(free.error());
    }
    auto request = detail::keyboardLeaseRequest(interfaceId_, leasePurpose);
    if (!request.has_value()) {
        return std::unexpected(request.error());
    }
    const auto lease = registry_->acquire(*request);
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    // `lease` vive até o fim do escopo: a interface fica reservada durante a operação.
    auto opened = control_->open();
    if (!opened.has_value()) {
        return std::unexpected(opened.error());
    }
    if (*opened == nullptr) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "sessão do teclado ausente");
    }
    return operation(**opened);
}

shared_kernel::Result<void> ControlKeyboard::setGameMode(const domain::GameModeKeys& keys) const
{
    return run<shared_kernel::Result<void>>(
        [&keys](KeyboardControlSession& session) { return session.setGameMode(keys); });
}

shared_kernel::Result<domain::OnboardSlots> ControlKeyboard::listSlots() const
{
    return run<shared_kernel::Result<domain::OnboardSlots>>(
        [](KeyboardControlSession& session) { return session.readSlots(); });
}

shared_kernel::Result<void> ControlKeyboard::selectSlot(domain::OnboardSlot slot) const
{
    return run<shared_kernel::Result<void>>([slot](KeyboardControlSession& session) -> shared_kernel::Result<void> {
        const auto slots = session.readSlots();
        if (!slots.has_value()) {
            return std::unexpected(slots.error());
        }
        if (std::ranges::find(slots->enabled, slot) == slots->enabled.end()) {
            return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                       "o slot " + std::to_string(slot.number()) + " não está habilitado no teclado");
        }
        return session.selectSlot(slot);
    });
}

} // namespace ghub::lighting::application
