#pragma once

#include "modules/lighting/application/KeyboardLighting.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Teclado falso: registra cada chamada em ordem ("open", "begin", "paint", "save:<zona>:<tipo>", "end") e falha quando
// configurado.
class FakeKeyboardLighting final : public lighting::application::KeyboardLighting {
public:
    [[nodiscard]] shared_kernel::Result<std::unique_ptr<lighting::application::KeyboardLightingSession>> open() override
    {
        calls_.emplace_back("open");
        if (openFailure_.has_value()) {
            return std::unexpected(*openFailure_);
        }
        return std::make_unique<Session>(*this);
    }

    void failOpen(shared_kernel::DomainError error) { openFailure_ = std::move(error); }
    void failBegin(shared_kernel::DomainError error) { beginFailure_ = std::move(error); }
    void failPaint(shared_kernel::DomainError error) { paintFailure_ = std::move(error); }
    void failSave(shared_kernel::DomainError error) { saveFailure_ = std::move(error); }
    void failEnd(shared_kernel::DomainError error) { endFailure_ = std::move(error); }

    // Consulta de perda do controle (R15): não entra em calls(); pode ser chamada da thread de trabalho.
    void setControlLost(bool lost) { controlLost_.store(lost); }
    void failControlCheck(bool fail) { controlCheckFails_.store(fail); }
    [[nodiscard]] int controlChecks() const noexcept { return controlChecks_.load(); }

    [[nodiscard]] const std::vector<std::string>& calls() const noexcept { return calls_; }
    [[nodiscard]] const std::vector<lighting::domain::KeyboardEffect>& saved() const noexcept { return saved_; }
    // Zona de cada efeito de saved(), na mesma ordem.
    [[nodiscard]] const std::vector<lighting::domain::KeyboardZone>& savedZones() const noexcept { return savedZones_; }
    [[nodiscard]] const std::vector<lighting::domain::KeyColorMap>& painted() const noexcept { return painted_; }

private:
    class Session final : public lighting::application::KeyboardLightingSession {
    public:
        explicit Session(FakeKeyboardLighting& owner) : owner_(&owner) {}

        shared_kernel::Result<void> beginLive() override { return owner_->record("begin", owner_->beginFailure_); }

        shared_kernel::Result<void> paint(const lighting::domain::KeyColorMap& colors) override
        {
            owner_->painted_.push_back(colors);
            return owner_->record("paint", owner_->paintFailure_);
        }

        shared_kernel::Result<void> setEffect(lighting::domain::KeyboardZone zone,
                                              const lighting::domain::KeyboardEffect& effect) override
        {
            owner_->saved_.push_back(effect);
            owner_->savedZones_.push_back(zone);
            const std::string name = std::string{"save:"} +
                                     (zone == lighting::domain::KeyboardZone::Keys ? "keys" : "logo") + ":" +
                                     std::to_string(static_cast<int>(effect.kind()));
            return owner_->record(name.c_str(), owner_->saveFailure_);
        }

        shared_kernel::Result<void> endLive() override { return owner_->record("end", owner_->endFailure_); }

        shared_kernel::Result<bool> controlLost() override
        {
            owner_->controlChecks_.fetch_add(1);
            if (owner_->controlCheckFails_.load()) {
                return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "leitura falhou");
            }
            return owner_->controlLost_.load();
        }

    private:
        FakeKeyboardLighting* owner_;
    };

    shared_kernel::Result<void> record(const char* call, const std::optional<shared_kernel::DomainError>& failure)
    {
        calls_.emplace_back(call);
        if (failure.has_value()) {
            return std::unexpected(*failure);
        }
        return {};
    }

    std::atomic<bool> controlLost_{false};
    std::atomic<bool> controlCheckFails_{false};
    std::atomic<int> controlChecks_{0};
    std::vector<std::string> calls_;
    std::vector<lighting::domain::KeyColorMap> painted_;
    std::vector<lighting::domain::KeyboardEffect> saved_;
    std::vector<lighting::domain::KeyboardZone> savedZones_;
    std::optional<shared_kernel::DomainError> saveFailure_;
    std::optional<shared_kernel::DomainError> openFailure_;
    std::optional<shared_kernel::DomainError> beginFailure_;
    std::optional<shared_kernel::DomainError> paintFailure_;
    std::optional<shared_kernel::DomainError> endFailure_;
};

} // namespace ghub::fakes
