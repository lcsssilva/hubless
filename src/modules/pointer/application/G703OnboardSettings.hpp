#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/pointer/application/OnboardBackupStore.hpp"
#include "modules/pointer/application/OnboardMemory.hpp"
#include "modules/pointer/domain/G703ProfileSettings.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>

namespace ghub::pointer {

struct G703OnboardSnapshot {
    std::uint16_t sector;
    G703ProfileSettings settings;
    bool backupExists;
};

// Casos de uso do perfil onboard do G703 (ADR-0017): ler, gravar DPI/taxa no setor do perfil ativo e restaurar a
// cópia de segurança. Toda operação: probe de donos externos -> DeviceLease -> abrir o nó -> verificações de
// segurança (modo onboard, setor ativo habilitado, CRCs). Só a gravação escreve, uma única vez, sem retentativa,
// e sempre relê e compara.
class G703OnboardSettings {
public:
    // Os colaboradores precisam sobreviver ao caso de uso.
    G703OnboardSettings(device_access::ExternalProcessProbe& probe, device_access::AccessRegistry& registry,
                        OnboardMemoryConnector& connector, OnboardBackupStore& backup)
        : probe_(&probe), registry_(&registry), connector_(&connector), backup_(&backup)
    {
    }

    [[nodiscard]] shared_kernel::Result<G703OnboardSnapshot> read() const;
    // Devolve as configurações relidas do mouse.
    [[nodiscard]] shared_kernel::Result<G703ProfileSettings> write(const G703ProfileSettings& settings) const;
    [[nodiscard]] shared_kernel::Result<G703ProfileSettings> restoreBackup() const;

private:
    device_access::ExternalProcessProbe* probe_;
    device_access::AccessRegistry* registry_;
    OnboardMemoryConnector* connector_;
    OnboardBackupStore* backup_;
};

} // namespace ghub::pointer
