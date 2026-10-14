#pragma once

#include "modules/camera/application/VideoCapture.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace ghub::fakes {

// Item do roteiro de uma sessão falsa: um quadro, um prazo esgotado ou um erro de stream.
struct FakeTimeout {};
using FakeReadStep = std::variant<std::vector<std::byte>, FakeTimeout, shared_kernel::DomainError>;

// Captura falsa: o teste define o resultado do próximo `start` e o roteiro das leituras.
// Registra os modos pedidos e quantas sessões estão vivas. Não dorme nem usa relógio real.
// A captura precisa sobreviver às sessões que criou.
class FakeVideoCapture final : public camera::VideoCapture {
public:
    [[nodiscard]] shared_kernel::Result<std::unique_ptr<camera::VideoCaptureSession>>
    start(const camera::PreviewMode& mode) override
    {
        startedModes_.push_back(mode);
        if (startsWithNull_) {
            return std::unique_ptr<camera::VideoCaptureSession>{};
        }
        if (startError_.has_value()) {
            return std::unexpected(*startError_);
        }
        return std::make_unique<Session>(script_, liveSessions_);
    }

    // Os três abaixo definem o próximo resultado de `start` (o último chamado vence).
    void failStartWith(shared_kernel::DomainError error)
    {
        startError_ = std::move(error);
        startsWithNull_ = false;
    }
    void returnNullSession()
    {
        startError_.reset();
        startsWithNull_ = true;
    }
    void succeedWith(std::vector<FakeReadStep> script)
    {
        startError_.reset();
        startsWithNull_ = false;
        script_ = std::move(script);
    }

    [[nodiscard]] const std::vector<camera::PreviewMode>& startedModes() const noexcept { return startedModes_; }
    [[nodiscard]] int liveSessions() const noexcept { return liveSessions_; }

private:
    class Session final : public camera::VideoCaptureSession {
    public:
        Session(const std::vector<FakeReadStep>& script, int& live)
            : script_(script.begin(), script.end()), live_(&live)
        {
            ++*live_;
        }
        ~Session() override { --*live_; }
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] shared_kernel::Result<bool> readFrame(std::chrono::milliseconds /*timeout*/,
                                                            std::vector<std::byte>& frame) override
        {
            if (script_.empty()) {
                return false;
            }
            FakeReadStep step = std::move(script_.front());
            script_.pop_front();
            if (auto* bytes = std::get_if<std::vector<std::byte>>(&step)) {
                frame = std::move(*bytes);
                return true;
            }
            if (auto* error = std::get_if<shared_kernel::DomainError>(&step)) {
                return std::unexpected(std::move(*error));
            }
            return false;
        }

    private:
        std::deque<FakeReadStep> script_;
        int* live_;
    };

    std::optional<shared_kernel::DomainError> startError_;
    bool startsWithNull_ = false;
    std::vector<FakeReadStep> script_;
    std::vector<camera::PreviewMode> startedModes_;
    int liveSessions_ = 0;
};

} // namespace ghub::fakes
