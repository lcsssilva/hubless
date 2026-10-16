#pragma once

#include "modules/onboard_memory/application/OnboardBackupStore.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace ghub::onboard_memory {

// Cópia de segurança em arquivos `<prefixo>XXXX.bin` (XXXX = setor em hexadecimal) num diretório (produção:
// $XDG_DATA_HOME/ghub-linux/backup/). Diretório 0700, arquivos 0600, gravação atômica e sem sobrescrever.
[[nodiscard]] std::unique_ptr<OnboardBackupStore> makeFileOnboardBackupStore(std::filesystem::path directory,
                                                                             std::string filePrefix);

} // namespace ghub::onboard_memory
