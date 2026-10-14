#include "modules/lighting/application/LiveKeyboardSession.hpp"

#include "modules/lighting/application/KeyboardAccess.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace ghub::lighting::application {

namespace {

constexpr std::string_view leasePurpose = "sessão ao vivo de iluminação";

} // namespace

LiveKeyboardSession::LiveKeyboardSession(std::unique_ptr<KeyboardLightingSession> session,
                                         device_access::DeviceLease lease, device_access::AccessRegistry& registry,
                                         device_access::LeaseRequest renewal)
    : session_(std::move(session)), lease_(std::move(lease)), registry_(&registry), renewal_(std::move(renewal))
{
}

LiveKeyboardSession::~LiveKeyboardSession()
{
    // Falha ao devolver o controle no destrutor não tem a quem ser informada; não há repetição.
    (void)close(); // NOLINT(bugprone-unused-return-value)
}

shared_kernel::Result<void> LiveKeyboardSession::paint(const domain::KeyColorMap& colors)
{
    return guarded([&colors](KeyboardLightingSession& session) { return session.paint(colors); });
}

shared_kernel::Result<void> LiveKeyboardSession::applyEffect(domain::KeyboardZone zone,
                                                             const domain::KeyboardEffect& effect)
{
    return guarded([zone, &effect](KeyboardLightingSession& session) { return session.setEffect(zone, effect); });
}

shared_kernel::Result<void>
LiveKeyboardSession::guarded(const std::function<shared_kernel::Result<void>(KeyboardLightingSession&)>& operation)
{
    if (session_ == nullptr) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "sessão ao vivo encerrada");
    }
    // Nas falhas abaixo o erro relatado é o da operação; o da devolução do controle é descartado (R11: uma tentativa).
    if (const auto leased = renewLease(); !leased.has_value()) {
        (void)close(); // NOLINT(bugprone-unused-return-value): descartado de propósito, ver acima
        return leased;
    }
    if (const auto done = operation(*session_); !done.has_value()) {
        (void)close(); // NOLINT(bugprone-unused-return-value): descartado de propósito, ver acima
        return done;
    }
    return {};
}

shared_kernel::Result<void> LiveKeyboardSession::end()
{
    return close();
}

shared_kernel::Result<bool> LiveKeyboardSession::checkControl()
{
    if (session_ == nullptr) {
        return false;
    }
    const auto lost = session_->controlLost(); // só leitura: não renova o lease
    if (!lost.has_value() || !*lost) {
        return lost;
    }
    // O firmware já tem o controle: descarta a sessão sem endLive e libera o lease.
    session_.reset();
    lease_.reset();
    return true;
}

shared_kernel::Result<void> LiveKeyboardSession::renewLease()
{
    if (lease_.has_value()) {
        // Revogação (suspensão, remoção) não é renovada: a operação em andamento aborta.
        if (const auto held = lease_->verify();
            !held.has_value() && held.error().code == shared_kernel::ErrorCode::LeaseRevoked) {
            lease_.reset();
            return held;
        }
        lease_.reset(); // libera antes de pedir de novo: o registro não renova um lease ativo
    }
    auto fresh = registry_->acquire(renewal_);
    if (!fresh.has_value()) {
        return std::unexpected(fresh.error());
    }
    lease_.emplace(std::move(*fresh));
    return {};
}

shared_kernel::Result<void> LiveKeyboardSession::close()
{
    if (session_ == nullptr) {
        return {};
    }
    const auto session = std::move(session_); // fechada antes de tentar: nunca repete
    session_ = nullptr;
    // Só devolve o controle com o lease em mãos; sem ele (outro dono), não escreve.
    auto result = renewLease();
    if (result.has_value()) {
        result = session->endLive();
    }
    lease_.reset();
    return result;
}

StartLiveKeyboardSession::StartLiveKeyboardSession(KeyboardLighting& lighting, device_access::AccessRegistry& registry,
                                                   device_access::ExternalProcessProbe& probe,
                                                   device_access::PhysicalInterfaceId interfaceId)
    : lighting_(&lighting), registry_(&registry), probe_(&probe), interfaceId_(std::move(interfaceId))
{
}

shared_kernel::Result<std::unique_ptr<LiveKeyboardSession>> StartLiveKeyboardSession::start() const
{
    if (const auto free = detail::ensureNoExternalOwner(*probe_); !free.has_value()) {
        return std::unexpected(free.error());
    }

    auto built = detail::keyboardLeaseRequest(interfaceId_, std::string{leasePurpose});
    if (!built.has_value()) {
        return std::unexpected(built.error());
    }
    device_access::LeaseRequest request = std::move(*built);
    auto lease = registry_->acquire(request);
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
    // Construtor privado: só este caso de uso cria sessões.
    std::unique_ptr<LiveKeyboardSession> session{
        new LiveKeyboardSession(std::move(*opened), std::move(*lease), *registry_, std::move(request))};
    if (const auto begun = session->session_->beginLive(); !begun.has_value()) {
        // R11: o comando pode ter chegado; o destrutor da sessão tenta uma vez devolver o controle.
        return std::unexpected(begun.error());
    }
    return session;
}

} // namespace ghub::lighting::application
