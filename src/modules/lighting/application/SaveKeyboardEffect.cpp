#include "modules/lighting/application/SaveKeyboardEffect.hpp"

#include "modules/lighting/application/KeyboardAccess.hpp"

#include <utility>

namespace ghub::lighting::application {

SaveKeyboardEffect::SaveKeyboardEffect(KeyboardLighting& lighting, device_access::AccessRegistry& registry,
                                       device_access::ExternalProcessProbe& probe,
                                       device_access::PhysicalInterfaceId interfaceId)
    : lighting_(&lighting), registry_(&registry), probe_(&probe), interfaceId_(std::move(interfaceId))
{
}

shared_kernel::Result<void> SaveKeyboardEffect::save(domain::KeyboardZone zone,
                                                     const domain::KeyboardEffect& effect) const
{
    if (const auto free = detail::ensureNoExternalOwner(*probe_); !free.has_value()) {
        return free;
    }
    auto request = detail::keyboardLeaseRequest(interfaceId_, "gravar efeito de hardware do teclado");
    if (!request.has_value()) {
        return std::unexpected(request.error());
    }
    // O lease vive até o fim da função (RAII), depois da sessão, que é destruída antes dele.
    auto lease = registry_->acquire(*request);
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    auto opened = lighting_->open();
    if (!opened.has_value()) {
        return std::unexpected(opened.error());
    }
    if (*opened == nullptr) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "sessão do teclado ausente");
    }
    const auto session = std::move(*opened);

    // R11: o comando pode ter chegado mesmo com erro; uma única tentativa de devolver o controle, sem reenvio.
    if (auto begun = session->beginLive(); !begun.has_value()) {
        (void)session->endLive(); // NOLINT(bugprone-unused-return-value): o erro relatado é o do início
        return begun;
    }
    if (auto saved = session->setEffect(zone, effect); !saved.has_value()) {
        (void)session->endLive(); // NOLINT(bugprone-unused-return-value): o erro relatado é o da gravação
        return saved;
    }
    return session->endLive();
}

} // namespace ghub::lighting::application
