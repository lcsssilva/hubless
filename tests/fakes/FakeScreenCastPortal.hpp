#pragma once

#include "modules/lighting/infrastructure/ScreenCastPortal.hpp"

#include <fcntl.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Portal falso: registra a ordem das chamadas, falha no passo pedido e pode ficar "esperando o usuário" no Start até
// ser cancelado. O descritor devolvido é um /dev/null aberto de verdade (para provar fechamento sem capturar nada).
class FakeScreenCastPortal final : public lighting::infrastructure::ScreenCastPortalApi {
public:
    [[nodiscard]] shared_kernel::Result<std::string> createSession(const std::atomic<bool>&) override
    {
        record("CreateSession");
        if (failing("CreateSession")) {
            return failure();
        }
        return std::string{"/session/fake"};
    }
    [[nodiscard]] shared_kernel::Result<void> selectSources(const std::string& session,
                                                            const std::atomic<bool>&) override
    {
        record("SelectSources:" + session);
        if (failing("SelectSources")) {
            return failure();
        }
        return {};
    }
    [[nodiscard]] shared_kernel::Result<std::uint32_t> start(const std::string& session,
                                                             const std::atomic<bool>& cancelled) override
    {
        record("Start:" + session);
        if (blockStart_) {
            insideStart_ = true;
            while (!cancelled) {
                std::this_thread::yield(); // espera por condição (cancelamento), sem dormir
            }
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "cancelado");
        }
        if (failing("Start")) {
            return failure();
        }
        return nodeId_;
    }
    [[nodiscard]] shared_kernel::Result<lighting::infrastructure::ScreenFd>
    openPipeWireRemote(const std::string& session) override
    {
        record("OpenPipeWireRemote:" + session);
        if (failing("OpenPipeWireRemote")) {
            return failure();
        }
        return lighting::infrastructure::ScreenFd{::open("/dev/null", O_RDONLY | O_CLOEXEC)};
    }
    void closeSession(const std::string& session) noexcept override { record("Close:" + session); }

    void failAt(std::string step) { failStep_ = std::move(step); }
    void blockInStart() { blockStart_ = true; }
    void setNodeId(std::uint32_t id) { nodeId_ = id; }
    [[nodiscard]] bool insideStart() const noexcept { return insideStart_; }
    [[nodiscard]] std::vector<std::string> calls() const
    {
        const std::lock_guard lock{mutex_};
        return calls_;
    }

private:
    void record(std::string call)
    {
        const std::lock_guard lock{mutex_};
        calls_.push_back(std::move(call));
    }
    [[nodiscard]] bool failing(const std::string& step) const { return failStep_ == step; }
    static std::unexpected<shared_kernel::DomainError> failure()
    {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "negado pelo usuário");
    }

    mutable std::mutex mutex_;
    std::vector<std::string> calls_;
    std::optional<std::string> failStep_;
    std::atomic<bool> blockStart_{false};
    std::atomic<bool> insideStart_{false};
    std::uint32_t nodeId_ = 42;
};

} // namespace ghub::fakes
