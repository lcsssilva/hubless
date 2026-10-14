#pragma once

#include "modules/camera/application/CameraControlAccess.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Acesso falso aos controles de uma câmera: o "dispositivo" é a lista `controls`; escrever troca o valor atual.
// Registra aberturas, leituras, escritas e sessões vivas. Não dorme nem usa relógio real.
// O acesso precisa sobreviver às sessões que criou.
class FakeCameraControlAccess final : public camera::CameraControlAccess {
public:
    struct Write {
        std::uint32_t id;
        std::int64_t value;
    };

    // --- configuração ---
    std::vector<camera::CameraControlSpec> controls;
    std::optional<shared_kernel::DomainError> openError;
    std::map<std::size_t, shared_kernel::DomainError> readErrors; // n-ésima leitura (de 0) -> erro
    std::optional<shared_kernel::DomainError> writeError;
    std::function<void()> onRead; // executado no início de cada leitura (ex.: avançar o relógio do teste)
    // Executado no início de cada escrita, antes de registrá-la (ex.: bloquear a thread de trabalho num latch).
    std::function<void()> onWrite;

    // --- registro ---
    std::vector<Write> writes;
    std::size_t opens = 0;
    std::size_t reads = 0;
    int liveSessions = 0;

    [[nodiscard]] shared_kernel::Result<std::unique_ptr<camera::CameraControlSession>> open() override
    {
        ++opens;
        if (openError.has_value()) {
            return std::unexpected(*openError);
        }
        return std::make_unique<Session>(*this);
    }

private:
    class Session final : public camera::CameraControlSession {
    public:
        explicit Session(FakeCameraControlAccess& owner) : owner_(&owner) { ++owner_->liveSessions; }
        ~Session() override { --owner_->liveSessions; }
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] shared_kernel::Result<camera::CameraControl> readControl(std::uint32_t id) override
        {
            const std::size_t call = owner_->reads++;
            if (owner_->onRead) {
                owner_->onRead();
            }
            if (const auto error = owner_->readErrors.find(call); error != owner_->readErrors.end()) {
                return std::unexpected(error->second);
            }
            const auto* spec = owner_->find(id);
            if (spec == nullptr) {
                return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "controle inexistente");
            }
            return camera::CameraControl::create(*spec);
        }

        [[nodiscard]] shared_kernel::Result<void> writeControl(std::uint32_t id, std::int64_t value) override
        {
            if (owner_->onWrite) {
                owner_->onWrite();
            }
            if (owner_->writeError.has_value()) {
                return std::unexpected(*owner_->writeError);
            }
            auto* spec = owner_->find(id);
            if (spec == nullptr) {
                return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "controle inexistente");
            }
            owner_->writes.push_back({.id = id, .value = value});
            spec->currentValue = value;
            return {};
        }

    private:
        FakeCameraControlAccess* owner_;
    };

    camera::CameraControlSpec* find(std::uint32_t id)
    {
        for (auto& spec : controls) {
            if (spec.id == id) {
                return &spec;
            }
        }
        return nullptr;
    }
};

} // namespace ghub::fakes
