#include "modules/onboard_memory/application/ActiveSectorEditor.hpp"

#include "modules/onboard_memory/domain/OnboardDirectory.hpp"

#include <memory>
#include <string>
#include <utility>

namespace ghub::onboard_memory {

namespace {

constexpr std::uint16_t kDirectorySector = 0;
constexpr std::uint8_t kOnboardMode = 1;

// Tudo que uma operação precisa depois das recusas: o lease e o nó abertos vivem enquanto o objeto viver.
struct Session {
    device_access::DeviceLease lease;
    std::unique_ptr<OnboardMemory> memory;
};

shared_kernel::Result<Session> openSession(device_access::ExternalProcessProbe& probe,
                                           device_access::AccessRegistry& registry, OnboardMemoryConnector& connector,
                                           const ActiveSectorTarget& target, std::string_view purpose)
{
    const auto owners = probe.runningOwners();
    if (!owners.has_value()) {
        return std::unexpected(owners.error());
    }
    if (!owners->empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::DeviceBusy,
                                   "em uso por " + std::string{device_access::processName(owners->front())});
    }
    auto interfaceId = device_access::PhysicalInterfaceId::create(target.interfaceId);
    if (!interfaceId.has_value()) {
        return std::unexpected(interfaceId.error());
    }
    auto holder = device_access::LeaseHolder::create(target.leaseHolder);
    if (!holder.has_value()) {
        return std::unexpected(holder.error());
    }
    auto lease = registry.acquire({
        .interfaceId = std::move(*interfaceId),
        .holder = std::move(*holder),
        .purpose = std::string{purpose},
        .maxDuration = target.leaseDuration,
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

// Uma gravação, uma releitura, uma comparação; nunca regrava (ADR-0017 §6).
shared_kernel::Result<OnboardSector> writeAndVerify(Session& session, const ActiveSector& active,
                                                    const OnboardSector& target)
{
    // Expiração ou revogação desde a aquisição impedem a escrita.
    if (const auto held = session.lease.verify(); !held.has_value()) {
        return std::unexpected(held.error());
    }
    if (const auto written = session.memory->writeSector(active.number, target.bytes()); !written.has_value()) {
        return std::unexpected(written.error());
    }
    auto reread = readValidSector(*session.memory, active.number, active.sectorSize);
    if (!reread.has_value()) {
        return std::unexpected(reread.error());
    }
    if (*reread != target) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "mouse com dados diferentes do enviado — use Restaurar cópia");
    }
    return reread;
}

} // namespace

shared_kernel::Result<ActiveSector> inspectActiveSector(OnboardMemory& memory)
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
    return ActiveSector{.number = *current, .sectorSize = layout->sectorSize, .sector = std::move(*sector)};
}

shared_kernel::Result<ActiveSectorSnapshot> ActiveSectorEditor::read() const
{
    auto session = openSession(*probe_, *registry_, *connector_, target_, "leitura do perfil onboard do mouse");
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    auto active = inspectActiveSector(*session->memory);
    if (!active.has_value()) {
        return std::unexpected(active.error());
    }
    const auto saved = backup_->load(active->number);
    if (!saved.has_value()) {
        return std::unexpected(saved.error());
    }
    return ActiveSectorSnapshot{
        .number = active->number,
        .sector = std::move(active->sector),
        .backupExists = saved->has_value(),
    };
}

shared_kernel::Result<OnboardSector> ActiveSectorEditor::write(const Edit& edit) const
{
    auto session = openSession(*probe_, *registry_, *connector_, target_, "gravação do perfil onboard do mouse");
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    auto active = inspectActiveSector(*session->memory);
    if (!active.has_value()) {
        return std::unexpected(active.error());
    }
    auto editedBytes = edit(active->sector.bytes());
    if (!editedBytes.has_value()) {
        return std::unexpected(editedBytes.error());
    }
    auto raw = OnboardSector::fromBytes(std::move(*editedBytes));
    if (!raw.has_value()) {
        return std::unexpected(raw.error());
    }
    const auto edited = raw->withCrc();
    if (edited == active->sector) {
        return std::move(active->sector); // nada a mudar: não escreve
    }
    if (edited.size() != active->sector.size()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "a edição mudou o tamanho do setor; nada foi gravado");
    }
    if (const auto saved = backup_->saveIfAbsent(active->number, active->sector.bytes()); !saved.has_value()) {
        return std::unexpected(saved.error());
    }
    return writeAndVerify(*session, *active, edited);
}

shared_kernel::Result<OnboardSector> ActiveSectorEditor::restoreBackup(const Validate& validate) const
{
    auto session = openSession(*probe_, *registry_, *connector_, target_, "restauração do perfil onboard do mouse");
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    auto active = inspectActiveSector(*session->memory);
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
    // Só grava uma cópia que o módulo dono do dispositivo entende.
    if (const auto readable = validate(target->bytes()); !readable.has_value()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "cópia de segurança ilegível: " + readable.error().detail);
    }
    if (*target == active->sector) {
        return std::move(active->sector); // o mouse já está com a cópia: não escreve
    }
    return writeAndVerify(*session, *active, *target);
}

} // namespace ghub::onboard_memory
