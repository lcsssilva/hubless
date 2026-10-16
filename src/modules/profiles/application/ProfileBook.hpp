#pragma once

#include "modules/profiles/application/CameraSettingsPort.hpp"
#include "modules/profiles/application/HardwareSettingsPorts.hpp"
#include "modules/profiles/application/ProfileRepository.hpp"
#include "modules/profiles/application/ProfileTransfer.hpp"
#include "modules/profiles/domain/ProfileLibrary.hpp"
#include "shared_kernel/Result.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ghub::profiles {

// Caso de uso com estado: mantém a biblioteca em memória e a sincroniza com o repositório (R4, R8).
// Não é thread-safe: uma thread por vez. Todos os métodos bloqueiam (arquivo e câmera): thread de trabalho.
class ProfileBook {
public:
    // Resultado da captura de uma seção de hardware ao salvar (INC-017 parte 2, R10).
    struct SectionNote {
        ProfileSection section;
        bool included;
        std::string omittedReason; // vazio quando incluída
    };

    struct SaveReport {
        bool cameraIncluded = false;
        std::optional<std::string> cameraOmittedReason;
        bool equalizerIncluded = false;
        std::vector<SectionNote> hardware; // sidetone, luzes, efeito do G915 e DPI, nesta ordem
    };

    // Início do detalhe de InvalidArgument quando o nome já existe (a apresentação pede "Substituir?").
    static constexpr std::string_view alreadyExistsPrefix = "já existe um perfil com o nome ";

    struct ImportReport {
        std::string name;
        std::vector<std::string> ignoredFields;
        bool replaced = false;
    };

    // `transfer` (exportar/importar) é opcional; sem ele essas operações retornam BackendUnavailable.
    // `hardware` (sidetone, luzes, efeito do G915, DPI do G703) é opcional: porta nula = seção omitida ao salvar.
    ProfileBook(ProfileRepository& repository, CameraSettingsPort& camera, ProfileTransfer* transfer = nullptr,
                HardwarePorts hardware = {}) noexcept;

    // Lê a biblioteca. Em erro, guarda o erro: enquanto ele existir, saveCurrent e remove são recusados
    // sem chamar `save` do repositório, para não sobrescrever um arquivo que o app não entendeu (R8).
    [[nodiscard]] shared_kernel::Result<void> load();

    // R4: lê a câmera pelo port (erro -> seção omitida, com o motivo no relatório); os ganhos do EQ vêm do
    // chamador. Seções de hardware (INC-017 parte 2): o que cada dono conhece agora. Sidetone, luzes do G733 e efeito
    // do G915 não têm leitura no dispositivo: vale o último valor aplicado pela janela nesta sessão; o DPI do G703 é
    // lido do mouse (só leitura). Sem valor conhecido -> seção omitida, com o motivo no relatório. Sem nenhuma seção -> InvalidArgument e nada é gravado. Se o repositório falhar, a
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

    // INC-017: todas com a mesma regra de falha do repositório (recusadas enquanto houver erro de leitura).
    // Duplicar: nome novo validado; a cópia não herda aplicativos.
    [[nodiscard]] shared_kernel::Result<void> duplicate(std::string_view source, std::string_view copyName);
    // Exporta um perfil (sem aplicativos associados) para o arquivo; não altera a biblioteca.
    [[nodiscard]] shared_kernel::Result<void> exportProfile(std::string_view name, const std::string& path);
    // Importa um perfil do arquivo (entrada não confiável, validada pelo ProfileTransfer e pelos objetos de valor).
    // Nome existente sem `replace` -> InvalidArgument ("já existe"). Aplicativos associados nunca vêm do arquivo;
    // ao substituir, os do perfil existente são mantidos. Nada é aplicado.
    [[nodiscard]] shared_kernel::Result<ImportReport> importProfile(const std::string& path, bool replace);
    // Perfil padrão da troca automática (nullopt = nenhum).
    [[nodiscard]] shared_kernel::Result<void> setDefaultProfile(std::optional<std::string_view> name);
    // Marca/desmarca a seção como "igual em todos os perfis".
    [[nodiscard]] shared_kernel::Result<void> setPersistent(ProfileSection section, bool persistent);

    [[nodiscard]] const ProfileLibrary& library() const noexcept { return library_; }
    [[nodiscard]] std::optional<shared_kernel::DomainError> loadError() const { return loadError_; }

private:
    // Captura as seções de hardware que os donos conhecem; registra no relatório por que cada omitida ficou de fora.
    void captureHardware(HardwareSections& hardware, SaveReport& report);

    // Grava o candidato e, só se o repositório aceitar, troca a biblioteca em memória.
    [[nodiscard]] shared_kernel::Result<void> commit(ProfileLibrary candidate);

    ProfileRepository& repository_;
    CameraSettingsPort& camera_;
    ProfileTransfer* transfer_; // pode ser nulo
    HardwarePorts hardware_;
    ProfileLibrary library_;
    std::optional<shared_kernel::DomainError> loadError_;
};

} // namespace ghub::profiles
