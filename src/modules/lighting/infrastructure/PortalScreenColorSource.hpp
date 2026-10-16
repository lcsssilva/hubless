#pragma once

#include "modules/lighting/application/ScreenColorSource.hpp"
#include "modules/lighting/infrastructure/ScreenCastPortal.hpp"
#include "modules/lighting/infrastructure/ScreenStream.hpp"

#include <atomic>
#include <mutex>
#include <vector>

namespace ghub::lighting::infrastructure {

// ScreenColorSource sobre o portal ScreenCast + um fluxo de vídeo (INC-021 R4, ADR-0030). start() faz a sequência do
// portal (o usuário decide a cada sessão) e abre o fluxo; stop()/destruição fecham o fluxo e depois a sessão do portal.
// Nada é aberto na construção. stop() pode vir de outra thread e cancela um start() pendente. O portal e a fábrica de
// fluxo precisam sobreviver a este objeto.
class PortalScreenColorSource final : public application::ScreenColorSource {
public:
    PortalScreenColorSource(ScreenCastPortalApi& portal, ScreenStreamFactory streamFactory);
    ~PortalScreenColorSource() override;
    PortalScreenColorSource(const PortalScreenColorSource&) = delete;
    PortalScreenColorSource& operator=(const PortalScreenColorSource&) = delete;
    PortalScreenColorSource(PortalScreenColorSource&&) = delete;
    PortalScreenColorSource& operator=(PortalScreenColorSource&&) = delete;

    [[nodiscard]] shared_kernel::Result<void> start() override;
    void stop() override;
    [[nodiscard]] bool running() const override { return running_; }
    [[nodiscard]] std::vector<domain::RgbColor> regions() const override;

private:
    void onRegions(std::vector<domain::RgbColor> regions);

    ScreenCastPortalApi* portal_; // não nulo
    ScreenStreamFactory streamFactory_;
    std::mutex lifecycle_; // serializa start()/stop(); start() o segura durante a espera pelo usuário
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> running_{false};
    mutable std::mutex regionsMutex_;
    std::vector<domain::RgbColor> regions_;
    // Destruição inversa: o fluxo fecha antes da sessão do portal.
    std::unique_ptr<ScreenCastGrant> grant_;
    std::unique_ptr<ScreenStream> stream_;
};

} // namespace ghub::lighting::infrastructure
