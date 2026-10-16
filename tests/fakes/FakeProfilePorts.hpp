#pragma once

#include "modules/profiles/application/ActiveApplicationSource.hpp"
#include "modules/profiles/application/CameraSettingsPort.hpp"
#include "modules/profiles/application/EqualizerSettingsPort.hpp"
#include "modules/profiles/application/ProfileRepository.hpp"
#include "modules/profiles/application/ProfileTransfer.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ghub::test {

// Câmera falsa: devolve o que o teste configurou e registra as chamadas de apply na ordem.
class FakeCameraSettingsPort : public profiles::CameraSettingsPort {
public:
    shared_kernel::Result<std::vector<profiles::CameraControlSetting>> current{
        std::vector<profiles::CameraControlSetting>{}};
    // Resultado por id; id ausente -> devolve o próprio valor pedido (releitura igual).
    std::map<std::uint32_t, shared_kernel::Result<std::int64_t>> applyResults;
    int readCalls = 0;
    std::vector<std::pair<std::uint32_t, std::int64_t>> applied;

    shared_kernel::Result<std::vector<profiles::CameraControlSetting>> readCurrent() override
    {
        ++readCalls;
        return current;
    }

    shared_kernel::Result<std::int64_t> apply(std::uint32_t id, std::int64_t value) override
    {
        applied.emplace_back(id, value);
        if (const auto found = applyResults.find(id); found != applyResults.end()) {
            return found->second;
        }
        return value;
    }
};

// EQ falso: guarda a curva e registra os pedidos.
class FakeEqualizerSettingsPort final : public profiles::EqualizerSettingsPort {
public:
    std::array<int, 10> gains{};
    shared_kernel::Result<void> requestResult{};
    std::vector<std::array<int, 10>> requested;

    std::array<int, 10> currentGains() const override { return gains; }

    shared_kernel::Result<void> requestGains(const std::array<int, 10>& wanted) override
    {
        requested.push_back(wanted);
        return requestResult;
    }
};

// Repositório falso em memória, com falhas configuráveis.
class FakeProfileRepository final : public profiles::ProfileRepository {
public:
    shared_kernel::Result<profiles::ProfileLibrary> loadResult{profiles::ProfileLibrary{}};
    shared_kernel::Result<void> saveResult{};
    int loadCalls = 0;
    int saveCalls = 0;
    profiles::ProfileLibrary lastSaved;

    shared_kernel::Result<profiles::ProfileLibrary> load() override
    {
        ++loadCalls;
        return loadResult;
    }

    shared_kernel::Result<void> save(const profiles::ProfileLibrary& library) override
    {
        ++saveCalls;
        if (saveResult) {
            lastSaved = library;
        }
        return saveResult;
    }
};

// Exportação/importação falsa: guarda o último perfil exportado e devolve o que o teste configurou.
class FakeProfileTransfer final : public profiles::ProfileTransfer {
public:
    shared_kernel::Result<void> exportResult{};
    std::optional<shared_kernel::Result<profiles::ImportedProfile>> importResult;
    std::vector<std::string> exportedPaths;
    std::vector<profiles::Profile> exported;
    std::vector<std::string> importedPaths;

    shared_kernel::Result<void> exportProfile(const profiles::Profile& profile, const std::string& path) override
    {
        exportedPaths.push_back(path);
        exported.push_back(profile);
        return exportResult;
    }

    shared_kernel::Result<profiles::ImportedProfile> importProfile(const std::string& path) override
    {
        importedPaths.push_back(path);
        if (!importResult) {
            return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "sem resultado configurado");
        }
        return *importResult;
    }
};

// Fonte do aplicativo em foco falsa: registra start/stop e guarda os listeners; o teste dispara os avisos a partir
// de outra thread (como o adaptador real pode fazer). `start`/`stop` são chamados na thread dona.
class FakeActiveApplicationSource final : public profiles::ActiveApplicationSource {
public:
    shared_kernel::Result<void> startResult{};
    int startCalls = 0;
    int stopCalls = 0;

    shared_kernel::Result<void> start(ActivationListener onActivated, StatusListener onStatus) override
    {
        ++startCalls;
        if (!startResult) {
            return startResult;
        }
        activated_ = std::move(onActivated);
        status_ = std::move(onStatus);
        running_ = true;
        return {};
    }

    void stop() noexcept override
    {
        ++stopCalls;
        running_ = false;
    }

    [[nodiscard]] bool running() const { return running_; }

    // Entrega o aviso a partir de outra thread e espera ela terminar (os avisos ficam enfileirados no dono).
    void emitActivated(const std::string& application)
    {
        if (!activated_) {
            return; // fonte nunca iniciada (ou parada sem listener): a fonte real também não entregaria nada
        }
        const auto id = profiles::ApplicationId::create(application).value();
        std::thread([listener = activated_, id] { listener(id); }).join();
    }

    void emitStatus(std::optional<shared_kernel::DomainError> status)
    {
        if (!status_) {
            return;
        }
        std::thread([listener = status_, status = std::move(status)] { listener(status); }).join();
    }

private:
    ActivationListener activated_;
    StatusListener status_;
    bool running_ = false;
};

} // namespace ghub::test
