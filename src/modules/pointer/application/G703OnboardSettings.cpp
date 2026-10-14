#include "modules/pointer/application/G703OnboardSettings.hpp"

#include "modules/pointer/domain/OnboardDirectory.hpp"

#include <chrono>
#include <string>
#include <utility>

namespace ghub::pointer {

namespace {

constexpr auto kInterfaceId = "hidpp:046d:c539";
constexpr auto kLeaseHolder = "pointer.hidpp";
constexpr std::chrono::seconds kLeaseDuration{15};

// Tudo que uma operação precisa depois das recusas: o lease e o nó abertos vivem enquanto o objeto viver.
struct Session {
    device_access::DeviceLease lease;
    std::unique_ptr<OnboardMemory> memory;
};

shared_kernel::Result<Session> openSession(device_access::ExternalProcessProbe& probe,
                                           device_access::AccessRegistry& registry, OnboardMemoryConnector& connector,
                                           std::string_view purpose)
{
    const auto owners = probe.runningOwners();
    if (!owners.has_value()) {
        return std::unexpected(owners.error());
    }
    if (!owners->empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::DeviceBusy,
                                   "em uso por " + std::string{device_access::processName(owners->front())});
    }
    auto interfaceId = device_access::PhysicalInterfaceId::create(kInterfaceId);
    if (!interfaceId.has_value()) {
        return std::unexpected(interfaceId.error());
    }
    auto holder = device_access::LeaseHolder::create(kLeaseHolder);
    if (!holder.has_value()) {
        return std::unexpected(holder.error());
    }
    auto lease = registry.acquire({
        .interfaceId = std::move(*interfaceId),
        .holder = std::move(*holder),
        .purpose = std::string{purpose},
        .maxDuration = kLeaseDuration,
    });
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    auto memory = connector.open();
    if (!memory.has_value()) {
        return std::unexpected(memory.error());
    }
    return Session{.lease = std::move(*lease), .memory = std::move(*memory)};
}

constexpr std::uint16_t kDirectorySector = 0;
constexpr std::uint8_t kOnboardMode = 1;

// Setor do perfil ativo, já validado, e o tamanho de setor do mouse.
struct ActiveProfile {
    std::uint16_t number;
    std::uint16_t sectorSize;
    OnboardSector sector;
};

shared_kernel::Result<OnboardSector> readValidSector(OnboardMemory& memory, std::uint16_t number, std::uint16_t size)
{
    auto raw = memory.readSector(number, size);
    if (!raw.has_value()) {
        return std::unexpected(raw.error());
    }
    if (raw->size() != size) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "setor " + std::to_string(number) +
                                       " com tamanho diferente do informado pelo mouse");
    }
    return OnboardSector::fromBytes(std::move(*raw));
}

// Verificações de segurança (ADR-0017 §4): só devolve o perfil se tudo estiver coerente.
shared_kernel::Result<ActiveProfile> inspectActiveProfile(OnboardMemory& memory)
{
    using shared_kernel::ErrorCode;
    const auto layout = memory.describe();
    if (!layout.has_value()) {
        return std::unexpected(layout.error());
    }
    if (layout->sectorSize < OnboardSector::kMinSize || layout->sectorSize > OnboardSector::kMaxSize) {
        return shared_kernel::fail(ErrorCode::MalformedData,
                                   "tamanho de setor onboard implausível: " + std::to_string(layout->sectorSize));
    }
    const auto mode = memory.onboardMode();
    if (!mode.has_value()) {
        return std::unexpected(mode.error());
    }
    if (*mode != kOnboardMode) {
        return shared_kernel::fail(ErrorCode::BackendUnavailable, "mouse fora do modo onboard");
    }
    const auto current = memory.currentProfileSector();
    if (!current.has_value()) {
        return std::unexpected(current.error());
    }
    if (*current < 1 || *current > layout->profileCount) {
        return shared_kernel::fail(ErrorCode::MalformedData,
                                   "perfil ativo fora dos perfis do mouse: " + std::to_string(*current));
    }
    const auto directorySector = readValidSector(memory, kDirectorySector, layout->sectorSize);
    if (!directorySector.has_value()) {
        return std::unexpected(directorySector.error());
    }
    const auto directory = OnboardDirectory::parse(*directorySector);
    if (!directory.has_value()) {
        return std::unexpected(directory.error());
    }
    if (!directory->isEnabled(*current)) {
        return shared_kernel::fail(ErrorCode::MalformedData, "o perfil ativo está desabilitado no diretório");
    }
    auto sector = readValidSector(memory, *current, layout->sectorSize);
    if (!sector.has_value()) {
        return std::unexpected(sector.error());
    }
    if (!sector->hasValidCrc()) {
        return shared_kernel::fail(ErrorCode::MalformedData, "CRC do perfil ativo inválido");
    }
    return ActiveProfile{.number = *current, .sectorSize = layout->sectorSize, .sector = std::move(*sector)};
}

// Uma gravação, uma releitura, uma comparação; nunca regrava (ADR-0017 §6).
shared_kernel::Result<G703ProfileSettings> writeAndVerify(Session& session, const ActiveProfile& active,
                                                          const OnboardSector& target)
{
    // Expiração ou revogação desde a aquisição impedem a escrita.
    if (const auto held = session.lease.verify(); !held.has_value()) {
        return std::unexpected(held.error());
    }
    if (const auto written = session.memory->writeSector(active.number, target.bytes()); !written.has_value()) {
        return std::unexpected(written.error());
    }
    const auto reread = readValidSector(*session.memory, active.number, active.sectorSize);
    if (!reread.has_value()) {
        return std::unexpected(reread.error());
    }
    if (*reread != target) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "mouse com dados diferentes do enviado — use Restaurar cópia");
    }
    return G703ProfileSettings::read(*reread);
}

} // namespace

shared_kernel::Result<G703OnboardSnapshot> G703OnboardSettings::read() const
{
    auto session = openSession(*probe_, *registry_, *connector_, "leitura do perfil onboard do mouse");
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    const auto active = inspectActiveProfile(*session->memory);
    if (!active.has_value()) {
        return std::unexpected(active.error());
    }
    auto settings = G703ProfileSettings::read(active->sector);
    if (!settings.has_value()) {
        return std::unexpected(settings.error());
    }
    const auto saved = backup_->load(active->number);
    if (!saved.has_value()) {
        return std::unexpected(saved.error());
    }
    return G703OnboardSnapshot{
        .sector = active->number,
        .settings = std::move(*settings),
        .backupExists = saved->has_value(),
    };
}

shared_kernel::Result<G703ProfileSettings> G703OnboardSettings::write(const G703ProfileSettings& settings) const
{
    auto session = openSession(*probe_, *registry_, *connector_, "gravação do perfil onboard do mouse");
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    const auto active = inspectActiveProfile(*session->memory);
    if (!active.has_value()) {
        return std::unexpected(active.error());
    }
    const auto edited = settings.applyTo(active->sector);
    if (!edited.has_value()) {
        return std::unexpected(edited.error());
    }
    if (*edited == active->sector) {
        return G703ProfileSettings::read(active->sector); // nada a mudar: não escreve
    }
    if (const auto saved = backup_->saveIfAbsent(active->number, active->sector.bytes()); !saved.has_value()) {
        return std::unexpected(saved.error());
    }
    return writeAndVerify(*session, *active, *edited);
}

shared_kernel::Result<G703ProfileSettings> G703OnboardSettings::restoreBackup() const
{
    auto session = openSession(*probe_, *registry_, *connector_, "restauração do perfil onboard do mouse");
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    const auto active = inspectActiveProfile(*session->memory);
    if (!active.has_value()) {
        return std::unexpected(active.error());
    }
    auto saved = backup_->load(active->number);
    if (!saved.has_value()) {
        return std::unexpected(saved.error());
    }
    if (!saved->has_value()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "sem cópia de segurança");
    }
    if ((*saved)->size() != active->sectorSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "cópia de segurança com tamanho diferente do setor do mouse");
    }
    auto target = OnboardSector::fromBytes(std::move(**saved));
    if (!target.has_value()) {
        return std::unexpected(target.error());
    }
    if (!target->hasValidCrc()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "cópia de segurança com CRC inválido");
    }
    // Só grava uma cópia que o domínio entende (report rate, estágios e padrão válidos).
    if (const auto readable = G703ProfileSettings::read(*target); !readable.has_value()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "cópia de segurança ilegível: " + readable.error().detail);
    }
    if (*target == active->sector) {
        return G703ProfileSettings::read(active->sector); // o mouse já está com a cópia: não escreve
    }
    return writeAndVerify(*session, *active, *target);
}

} // namespace ghub::pointer
