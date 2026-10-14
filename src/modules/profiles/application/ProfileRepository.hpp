#pragma once

#include "modules/profiles/domain/ProfileLibrary.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::profiles {

// Persistência da biblioteca inteira. Bloqueante (I/O de arquivo): thread de trabalho.
class ProfileRepository {
public:
    virtual ~ProfileRepository() = default;

    // Armazenamento ausente -> biblioteca vazia. Dados ilegíveis ou inválidos -> MalformedData.
    [[nodiscard]] virtual shared_kernel::Result<ProfileLibrary> load() = 0;

    // Falha de escrita -> BackendUnavailable.
    [[nodiscard]] virtual shared_kernel::Result<void> save(const ProfileLibrary& library) = 0;
};

} // namespace ghub::profiles
