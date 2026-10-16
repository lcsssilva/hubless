#pragma once

#include "modules/lighting/application/KeyboardControl.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Controle do teclado falso (modo de jogo e slots): registra as chamadas ("open", "game:<códigos hex>", "slots",
// "select:<n>"), devolve os slots configurados e falha quando configurado. `setOnCall` roda a cada chamada de sessão
// (para conferir o lease no meio da operação).
class FakeKeyboardControl final : public lighting::application::KeyboardControl {
public:
    [[nodiscard]] shared_kernel::Result<std::unique_ptr<lighting::application::KeyboardControlSession>> open() override
    {
        calls_.emplace_back("open");
        if (openFailure_.has_value()) {
            return std::unexpected(*openFailure_);
        }
        return std::make_unique<Session>(*this);
    }

    void failOpen(shared_kernel::DomainError error) { openFailure_ = std::move(error); }
    void failGameMode(shared_kernel::DomainError error) { gameModeFailure_ = std::move(error); }
    void failSelect(shared_kernel::DomainError error) { selectFailure_ = std::move(error); }
    void setSlots(lighting::domain::OnboardSlots value) { slots_ = std::move(value); }
    void setOnCall(std::function<void()> hook) { onCall_ = std::move(hook); }

    [[nodiscard]] const std::vector<std::string>& calls() const noexcept { return calls_; }

private:
    class Session final : public lighting::application::KeyboardControlSession {
    public:
        explicit Session(FakeKeyboardControl& owner) : owner_(&owner) {}

        shared_kernel::Result<void> setGameMode(const lighting::domain::GameModeKeys& keys) override
        {
            static constexpr const char* digits = "0123456789abcdef";
            std::string text = "game:";
            for (const auto code : keys.hidCodes()) {
                text += digits[code >> 4U];
                text += digits[code & 0x0FU];
            }
            return owner_->record(text, owner_->gameModeFailure_);
        }

        shared_kernel::Result<lighting::domain::OnboardSlots> readSlots() override
        {
            owner_->calls_.emplace_back("slots");
            owner_->hook();
            return owner_->slots_;
        }

        shared_kernel::Result<void> selectSlot(lighting::domain::OnboardSlot slot) override
        {
            return owner_->record("select:" + std::to_string(slot.number()), owner_->selectFailure_);
        }

    private:
        FakeKeyboardControl* owner_;
    };

    void hook() const
    {
        if (onCall_) {
            onCall_();
        }
    }

    shared_kernel::Result<void> record(const std::string& call,
                                       const std::optional<shared_kernel::DomainError>& failure)
    {
        calls_.push_back(call);
        hook();
        if (failure.has_value()) {
            return std::unexpected(*failure);
        }
        return {};
    }

    std::vector<std::string> calls_;
    lighting::domain::OnboardSlots slots_;
    std::optional<shared_kernel::DomainError> openFailure_;
    std::optional<shared_kernel::DomainError> gameModeFailure_;
    std::optional<shared_kernel::DomainError> selectFailure_;
    std::function<void()> onCall_;
};

} // namespace ghub::fakes
