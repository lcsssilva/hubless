#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/onboard_memory/api/OnboardMemory.hpp"
#include "modules/pointer/domain/ButtonAssignments.hpp"
#include "modules/pointer/domain/G703ProfileSettings.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <vector>

namespace ghub::pointer {

struct G703OnboardSnapshot {
    std::uint16_t sector;
    G703ProfileSettings settings;
    bool backupExists;
    std::vector<ButtonAction> buttons; // os 6 botões do setor (INC-022)
};

// Casos de uso do perfil onboard do G703 (ADR-0017, ADR-0027): ler, gravar taxa/estágios/shift/LED no setor do perfil
// ativo e restaurar a cópia de segurança. O caminho seguro (probe, DeviceLease, verificações, cópia, releitura e
// comparação) é do ActiveSectorEditor do módulo onboard_memory; aqui só se interpreta o setor do G703.
class G703OnboardSettings {
public:
    // Os colaboradores precisam sobreviver ao caso de uso.
    G703OnboardSettings(device_access::ExternalProcessProbe& probe, device_access::AccessRegistry& registry,
                        onboard_memory::OnboardMemoryConnector& connector, onboard_memory::OnboardBackupStore& backup)
        : backup_(&backup), editor_(probe, registry, connector, backup,
                                    {.interfaceId = "hidpp:046d:c539", .leaseHolder = "pointer.hidpp"})
    {
    }

    [[nodiscard]] shared_kernel::Result<G703OnboardSnapshot> read() const;
    // Devolve as configurações relidas do mouse.
    [[nodiscard]] shared_kernel::Result<G703ProfileSettings> write(const G703ProfileSettings& settings) const;
    // Grava também as atribuições dos botões (INC-022); as entradas "manter" ficam como estão e "padrão" copia a
    // entrada da cópia original (se ainda não há cópia, o setor atual é o original).
    [[nodiscard]] shared_kernel::Result<G703ProfileSettings> write(const G703ProfileSettings& settings,
                                                                   const ButtonAssignments& buttons) const;
    [[nodiscard]] shared_kernel::Result<G703ProfileSettings> restoreBackup() const;

private:
    onboard_memory::OnboardBackupStore* backup_;
    onboard_memory::ActiveSectorEditor editor_;
};

} // namespace ghub::pointer
