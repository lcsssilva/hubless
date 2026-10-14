#pragma once

#include "modules/pointer/application/OnboardBackupStore.hpp"

#include <filesystem>
#include <utility>

namespace ghub::pointer::infrastructure {

// Cópia de segurança em arquivos `g703-4086-sector-XXXX.bin` (XXXX = setor em hexadecimal) num diretório injetado
// (produção: $XDG_DATA_HOME/ghub-linux/backup/). O diretório é criado com 0700 e os arquivos com 0600. A gravação
// é atômica e nunca sobrescreve: arquivo temporário no mesmo diretório + link() ao nome final (falha com EEXIST se
// já houver cópia, o que conta como sucesso).
class FileOnboardBackupStore final : public OnboardBackupStore {
public:
    static constexpr std::size_t maxBackupSize = 4096;

    explicit FileOnboardBackupStore(std::filesystem::path directory) : directory_(std::move(directory)) {}

    [[nodiscard]] shared_kernel::Result<std::optional<std::vector<std::uint8_t>>> load(std::uint16_t sector) override;
    [[nodiscard]] shared_kernel::Result<void> saveIfAbsent(std::uint16_t sector,
                                                           std::span<const std::uint8_t> bytes) override;

private:
    [[nodiscard]] std::filesystem::path pathFor(std::uint16_t sector) const;

    std::filesystem::path directory_;
};

} // namespace ghub::pointer::infrastructure
