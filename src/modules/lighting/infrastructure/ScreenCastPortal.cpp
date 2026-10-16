#include "modules/lighting/infrastructure/ScreenCastPortal.hpp"

namespace ghub::lighting::infrastructure {

shared_kernel::Result<ScreenCastGrant> requestScreenCast(ScreenCastPortalApi& api, const std::atomic<bool>& cancelled)
{
    auto session = api.createSession(cancelled);
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    // Daqui em diante a sessão existe no portal: qualquer saída sem concessão precisa fechá-la.
    const auto closeOnFailure = [&](const shared_kernel::DomainError& error) {
        api.closeSession(*session);
        return std::unexpected(error);
    };
    if (auto selected = api.selectSources(*session, cancelled); !selected.has_value()) {
        return closeOnFailure(selected.error());
    }
    auto node = api.start(*session, cancelled);
    if (!node.has_value()) {
        return closeOnFailure(node.error());
    }
    auto fd = api.openPipeWireRemote(*session);
    if (!fd.has_value()) {
        return closeOnFailure(fd.error());
    }
    return ScreenCastGrant{api, *std::move(session), *node, *std::move(fd)};
}

} // namespace ghub::lighting::infrastructure
