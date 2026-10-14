#pragma once

#include "modules/profiles/application/CameraSettingsPort.hpp"
#include "modules/profiles/application/ProfileRepository.hpp"
#include "modules/profiles/domain/ProfileLibrary.hpp"
#include "shared_kernel/Result.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace ghub::profiles {

// Caso de uso com estado: mantém a biblioteca em memória e a sincroniza com o repositório (R4, R8).
// Não é thread-safe: uma thread por vez. Todos os métodos bloqueiam (arquivo e câmera): thread de trabalho.
class ProfileBook {
public:
    struct SaveReport {
        bool cameraIncluded = false;
        std::optional<std::string> cameraOmittedReason;
        bool equalizerIncluded = false;
    };

    ProfileBook(ProfileRepository& repository, CameraSettingsPort& camera) noexcept;

    // Lê a biblioteca. Em erro, guarda o erro: enquanto ele existir, saveCurrent e remove são recusados
    // sem chamar `save` do repositório, para não sobrescrever um arquivo que o app não entendeu (R8).
    [[nodiscard]] shared_kernel::Result<void> load();

    // R4: lê a câmera pelo port (erro -> seção omitida, com o motivo no relatório); os ganhos do EQ vêm do
    // chamador. Sem nenhuma seção -> InvalidArgument e nada é gravado. Se o repositório falhar, a
    // biblioteca em memória continua como estava.
    [[nodiscard]] shared_kernel::Result<SaveReport>
    saveCurrent(std::string_view name, std::optional<std::array<int, 10>> equalizerGains, bool replace);

    // Mesma regra de falha do repositório que saveCurrent.
    [[nodiscard]] shared_kernel::Result<void> remove(std::string_view name);

    // Associação de aplicativos e chave da troca automática (ADR-0022): mesma regra de falha do repositório que
    // remove. Aplicativo inválido -> InvalidArgument.
    [[nodiscard]] shared_kernel::Result<void> assignApplication(std::string_view profileName,
                                                                std::string_view application);
    [[nodiscard]] shared_kernel::Result<void> unassignApplication(std::string_view application);
    [[nodiscard]] shared_kernel::Result<void> setAutomaticSwitching(bool enabled);

    [[nodiscard]] const ProfileLibrary& library() const noexcept { return library_; }
    [[nodiscard]] std::optional<shared_kernel::DomainError> loadError() const { return loadError_; }

private:
    // Grava o candidato e, só se o repositório aceitar, troca a biblioteca em memória.
    [[nodiscard]] shared_kernel::Result<void> commit(ProfileLibrary candidate);

    ProfileRepository& repository_;
    CameraSettingsPort& camera_;
    ProfileLibrary library_;
    std::optional<shared_kernel::DomainError> loadError_;
};

} // namespace ghub::profiles
