#pragma once

#include "shared_kernel/Result.hpp"

#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace ghub::lighting::infrastructure {

// Dono exclusivo de um descritor de arquivo (RAII); fecha na destruição.
class ScreenFd {
public:
    ScreenFd() = default;
    explicit ScreenFd(int fd) noexcept : fd_(fd) {}
    ScreenFd(const ScreenFd&) = delete;
    ScreenFd& operator=(const ScreenFd&) = delete;
    ScreenFd(ScreenFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    ScreenFd& operator=(ScreenFd&& other) noexcept
    {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    ~ScreenFd() { reset(); }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    // Entrega o descritor a quem vai fechá-lo (ex.: pw_context_connect_fd).
    [[nodiscard]] int release() noexcept { return std::exchange(fd_, -1); }

private:
    void reset() noexcept
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = -1;
    }

    int fd_ = -1;
};

// INC-021 R4 (ADR-0030): as chamadas do portal org.freedesktop.portal.ScreenCast que o Hubless usa, sem tipos de Qt,
// para testar a sequência com um fake. Cada chamada que espera o sinal Response do portal é BLOQUEANTE e consulta
// `cancelled` (BackendUnavailable "cancelado" quando vira true). Nunca usa persist_mode nem restore_token: o
// consentimento é pedido de novo a cada sessão.
class ScreenCastPortalApi {
public:
    virtual ~ScreenCastPortalApi() = default;

    // CreateSession; devolve o session_handle.
    [[nodiscard]] virtual shared_kernel::Result<std::string> createSession(const std::atomic<bool>& cancelled) = 0;
    // SelectSources: só monitor, um só, cursor oculto.
    [[nodiscard]] virtual shared_kernel::Result<void> selectSources(const std::string& session,
                                                                    const std::atomic<bool>& cancelled) = 0;
    // Start: o sistema mostra o diálogo de permissão; devolve o id do nó PipeWire do primeiro stream.
    [[nodiscard]] virtual shared_kernel::Result<std::uint32_t> start(const std::string& session,
                                                                     const std::atomic<bool>& cancelled) = 0;
    // OpenPipeWireRemote: descritor de uma conexão PipeWire que só enxerga o stream concedido.
    [[nodiscard]] virtual shared_kernel::Result<ScreenFd> openPipeWireRemote(const std::string& session) = 0;
    // Session.Close; melhor esforço, nunca falha.
    virtual void closeSession(const std::string& session) noexcept = 0;

protected:
    ScreenCastPortalApi() = default;
    ScreenCastPortalApi(const ScreenCastPortalApi&) = default;
    ScreenCastPortalApi& operator=(const ScreenCastPortalApi&) = default;
    ScreenCastPortalApi(ScreenCastPortalApi&&) = default;
    ScreenCastPortalApi& operator=(ScreenCastPortalApi&&) = default;
};

// Sessão do portal concedida pelo usuário: fecha a sessão no portal na destruição (RAII). `api` precisa sobreviver.
class ScreenCastGrant {
public:
    ScreenCastGrant(ScreenCastPortalApi& api, std::string session, std::uint32_t nodeId, ScreenFd fd) noexcept
        : api_(&api), session_(std::move(session)), nodeId_(nodeId), fd_(std::move(fd))
    {
    }
    ScreenCastGrant(const ScreenCastGrant&) = delete;
    ScreenCastGrant& operator=(const ScreenCastGrant&) = delete;
    ScreenCastGrant(ScreenCastGrant&& other) noexcept
        : api_(std::exchange(other.api_, nullptr)), session_(std::move(other.session_)), nodeId_(other.nodeId_),
          fd_(std::move(other.fd_))
    {
    }
    ScreenCastGrant& operator=(ScreenCastGrant&&) = delete;
    ~ScreenCastGrant()
    {
        if (api_ != nullptr) {
            api_->closeSession(session_);
        }
    }

    [[nodiscard]] std::uint32_t nodeId() const noexcept { return nodeId_; }
    // Entrega o descritor ao PipeWire (uma vez).
    [[nodiscard]] ScreenFd takeFd() noexcept { return std::move(fd_); }

private:
    ScreenCastPortalApi* api_; // nulo depois de movido
    std::string session_;
    std::uint32_t nodeId_;
    ScreenFd fd_;
};

// CreateSession -> SelectSources -> Start -> OpenPipeWireRemote. Qualquer falha depois do CreateSession fecha a sessão
// do portal antes de devolver o erro; `cancelled` interrompe as esperas.
[[nodiscard]] shared_kernel::Result<ScreenCastGrant> requestScreenCast(ScreenCastPortalApi& api,
                                                                       const std::atomic<bool>& cancelled);

} // namespace ghub::lighting::infrastructure
