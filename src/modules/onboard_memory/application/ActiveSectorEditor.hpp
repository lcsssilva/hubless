#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/onboard_memory/application/OnboardBackupStore.hpp"
#include "modules/onboard_memory/application/OnboardMemory.hpp"
#include "modules/onboard_memory/domain/OnboardSector.hpp"
#include "shared_kernel/Result.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ghub::onboard_memory {

// Perfil ativo já inspecionado: número do setor (a partir de 1), tamanho de setor do dispositivo e os bytes lidos
// (CRC válido).
struct ActiveSector {
    std::uint16_t number;
    std::uint16_t sectorSize;
    OnboardSector sector;
};

// Verificações de segurança (ADR-0017 §4): descreve a memória, exige modo onboard, perfil ativo dentro da
// contagem de perfis e habilitado no diretório, e CRCs válidos. Só lê.
[[nodiscard]] shared_kernel::Result<ActiveSector> inspectActiveSector(OnboardMemory& memory);

// Identificação do dispositivo para o lease.
struct ActiveSectorTarget {
    std::string interfaceId; // ex.: "hidpp:046d:c539"
    std::string leaseHolder; // ex.: "pointer.hidpp"
    std::chrono::seconds leaseDuration{15};
};

struct ActiveSectorSnapshot {
    std::uint16_t number;
    OnboardSector sector;
    bool backupExists;
};

// Caso de uso genérico do setor do perfil ativo (ADR-0017, ADR-0027): ler, gravar com cópia automática, releitura e
// comparação, e restaurar a cópia. A interpretação dos bytes é do módulo dono do dispositivo, que entrega funções
// de edição e de validação. Toda operação: probe de donos externos -> DeviceLease -> abrir o nó -> verificações.
// Só a gravação escreve, uma única vez, sem retentativa, e sempre relê e compara.
class ActiveSectorEditor {
public:
    // Recebe os bytes do setor ativo (CRC válido) e devolve os bytes editados; o CRC é recalculado pelo editor.
    using Edit = std::function<shared_kernel::Result<std::vector<std::uint8_t>>(std::span<const std::uint8_t>)>;
    // Recusa bytes que o módulo dono não entende (usada antes de restaurar uma cópia).
    using Validate = std::function<shared_kernel::Result<void>(std::span<const std::uint8_t>)>;

    // Os colaboradores precisam sobreviver ao caso de uso.
    ActiveSectorEditor(device_access::ExternalProcessProbe& probe, device_access::AccessRegistry& registry,
                       OnboardMemoryConnector& connector, OnboardBackupStore& backup, ActiveSectorTarget target)
        : probe_(&probe), registry_(&registry), connector_(&connector), backup_(&backup), target_(std::move(target))
    {
    }

    [[nodiscard]] shared_kernel::Result<ActiveSectorSnapshot> read() const;
    // Devolve o setor relido do dispositivo (ou o ativo, se a edição não muda nada e nada foi escrito).
    [[nodiscard]] shared_kernel::Result<OnboardSector> write(const Edit& edit) const;
    [[nodiscard]] shared_kernel::Result<OnboardSector> restoreBackup(const Validate& validate) const;

private:
    device_access::ExternalProcessProbe* probe_;
    device_access::AccessRegistry* registry_;
    OnboardMemoryConnector* connector_;
    OnboardBackupStore* backup_;
    ActiveSectorTarget target_;
};

} // namespace ghub::onboard_memory
