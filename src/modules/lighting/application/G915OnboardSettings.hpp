#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/lighting/domain/G915ProfileSettings.hpp"
#include "modules/onboard_memory/api/OnboardMemory.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>

namespace ghub::lighting::application {

struct G915OnboardSnapshot {
    std::uint16_t sector;
    domain::G915ProfileSettings settings;
    bool backupExists;
};

// Casos de uso do perfil onboard do G915 (INC-019 R4, INC-022 R1/R4, ADR-0029, ADR-0017, ADR-0027): ler, gravar o efeito
// gravado e as teclas G1-G5 no setor do perfil ativo e restaurar a cópia de segurança. O caminho seguro (probe, lease do
// teclado, verificações, cópia automática, releitura e comparação) é do ActiveSectorEditor do módulo onboard_memory; aqui
// só se interpreta o setor do G915. Só por ação do usuário; nada acontece na construção.
class G915OnboardSettings {
public:
    // Os colaboradores precisam sobreviver ao caso de uso. `backup` deve guardar só setores do G915.
    G915OnboardSettings(device_access::ExternalProcessProbe& probe, device_access::AccessRegistry& registry,
                        onboard_memory::OnboardMemoryConnector& connector, onboard_memory::OnboardBackupStore& backup)
        : editor_(probe, registry, connector, backup,
                  {.interfaceId = "hidpp:046d:407c", .leaseHolder = "lighting.hidpp"}),
          backup_(&backup)
    {
    }

    [[nodiscard]] shared_kernel::Result<G915OnboardSnapshot> read() const;
    // Lê o setor ativo, usa a cópia (ou, sem cópia, o próprio setor) como referência de "Original" e grava só o que
    // mudou. Se o setor ativo mudou entre a leitura e a gravação, recusa. Devolve as configurações relidas.
    [[nodiscard]] shared_kernel::Result<domain::G915ProfileSettings> write(const domain::G915ProfileSettings& settings) const;
    [[nodiscard]] shared_kernel::Result<domain::G915ProfileSettings> restoreBackup() const;

private:
    onboard_memory::ActiveSectorEditor editor_;
    onboard_memory::OnboardBackupStore* backup_; // não nulo
};

} // namespace ghub::lighting::application
