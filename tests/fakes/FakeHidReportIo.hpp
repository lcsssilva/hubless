#pragma once

#include "fakes/FakeClock.hpp"
#include "modules/lighting/infrastructure/HidReportIo.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace ghub::fakes {

// hidraw falso: registra todo quadro escrito, responde por roteiro (fila de leituras e/ou um "dispositivo" que
// reage a cada escrita) e, se houver relógio, avança o tempo a cada leitura (prazo sem sleep).
class FakeHidReportIo final : public lighting::infrastructure::HidReportIo {
public:
    using Frame = std::vector<std::uint8_t>;
    using Responder = std::function<std::vector<Frame>(const Frame&)>;

    explicit FakeHidReportIo(FakeClock* clock = nullptr) : clock_(clock) {}

    [[nodiscard]] std::expected<std::size_t, lighting::infrastructure::HidIoError>
    write(std::span<const std::uint8_t> report) override
    {
        written_.emplace_back(report.begin(), report.end());
        if (writeFailure_.has_value()) {
            return std::unexpected(*writeFailure_);
        }
        if (responder_) {
            for (auto& frame : responder_(written_.back())) {
                reads_.push_back(std::move(frame));
            }
        }
        return report.size();
    }

    [[nodiscard]] std::expected<std::size_t, lighting::infrastructure::HidIoError>
    read(std::span<std::uint8_t> buffer, std::chrono::milliseconds timeout) override
    {
        readTimeouts_.push_back(timeout);
        if (reads_.empty()) {
            if (clock_ != nullptr) {
                clock_->advance(timeout);
            }
            return std::unexpected(
                lighting::infrastructure::HidIoError{.failure = lighting::infrastructure::HidIoFailure::TimedOut});
        }
        if (clock_ != nullptr) {
            clock_->advance(readCost_);
        }
        const Frame frame = std::move(reads_.front());
        reads_.pop_front();
        const auto size = std::min(frame.size(), buffer.size());
        std::copy_n(frame.begin(), static_cast<std::ptrdiff_t>(size), buffer.begin());
        return size;
    }

    void setResponder(Responder responder) { responder_ = std::move(responder); }
    void queueRead(Frame frame) { reads_.push_back(std::move(frame)); }
    void setReadCost(std::chrono::milliseconds cost) { readCost_ = cost; }
    void failWrites(lighting::infrastructure::HidIoError error) { writeFailure_ = error; }

    [[nodiscard]] const std::vector<Frame>& written() const noexcept { return written_; }
    [[nodiscard]] const std::vector<std::chrono::milliseconds>& readTimeouts() const noexcept { return readTimeouts_; }

private:
    FakeClock* clock_;
    Responder responder_;
    std::deque<Frame> reads_;
    std::vector<Frame> written_;
    std::vector<std::chrono::milliseconds> readTimeouts_;
    std::chrono::milliseconds readCost_{0};
    std::optional<lighting::infrastructure::HidIoError> writeFailure_;
};

} // namespace ghub::fakes
