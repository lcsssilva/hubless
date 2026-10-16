#include "modules/lighting/infrastructure/PortalScreenColorSource.hpp"

#include <utility>

namespace ghub::lighting::infrastructure {

PortalScreenColorSource::PortalScreenColorSource(ScreenCastPortalApi& portal, ScreenStreamFactory streamFactory)
    : portal_(&portal), streamFactory_(std::move(streamFactory))
{
}

PortalScreenColorSource::~PortalScreenColorSource()
{
    stop();
}

shared_kernel::Result<void> PortalScreenColorSource::start()
{
    const std::scoped_lock lock{lifecycle_};
    if (running_) {
        return {};
    }
    cancelled_ = false;
    auto grant = requestScreenCast(*portal_, cancelled_);
    if (!grant.has_value()) {
        return std::unexpected(grant.error());
    }
    grant_ = std::make_unique<ScreenCastGrant>(*std::move(grant));
    auto stream = streamFactory_(grant_->takeFd(), grant_->nodeId(),
                                 [this](std::vector<domain::RgbColor> regions) { onRegions(std::move(regions)); });
    if (!stream.has_value()) {
        grant_.reset(); // fecha a sessão do portal
        return std::unexpected(stream.error());
    }
    stream_ = *std::move(stream);
    running_ = true;
    return {};
}

void PortalScreenColorSource::stop()
{
    cancelled_ = true; // interrompe um start() que esteja esperando o usuário (ele solta `lifecycle_` logo)
    const std::scoped_lock lock{lifecycle_};
    running_ = false;
    stream_.reset(); // o fluxo fecha antes da sessão do portal
    grant_.reset();
    const std::scoped_lock regionsLock{regionsMutex_};
    regions_.clear();
}

std::vector<domain::RgbColor> PortalScreenColorSource::regions() const
{
    const std::scoped_lock lock{regionsMutex_};
    return regions_;
}

void PortalScreenColorSource::onRegions(std::vector<domain::RgbColor> regions)
{
    if (!running_) {
        return; // chamada tardia de um fluxo já parado
    }
    const std::scoped_lock lock{regionsMutex_};
    regions_ = std::move(regions);
}

} // namespace ghub::lighting::infrastructure
