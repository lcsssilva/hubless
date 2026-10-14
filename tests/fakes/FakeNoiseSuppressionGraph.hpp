#pragma once

#include "modules/audio_processing/application/NoiseSuppressionGraph.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Grafo de redução de ruído falso: registra as chamadas em ordem, permite programar falha por operação e guarda
// a preferência de entrada padrão e o estado da fonte filtrada.
class FakeNoiseSuppressionGraph final : public audio_processing::NoiseSuppressionGraph {
public:
    enum class Op { FindSource, Create, Update, Remove, GetDefault, SetDefault };

    struct Call {
        Op op;
        std::optional<std::string> argument; // fonte alvo (Create) ou valor gravado (SetDefault)
        friend bool operator==(const Call&, const Call&) = default;
    };

    [[nodiscard]] shared_kernel::Result<std::string> findHeadsetSource() override
    {
        calls.push_back({Op::FindSource, std::nullopt});
        if (auto e = failure(Op::FindSource)) {
            return std::unexpected(*e);
        }
        return headsetSource;
    }

    [[nodiscard]] shared_kernel::Result<void> createNoiseSuppression(audio_processing::VoiceThreshold threshold,
                                                                     const std::string& source) override
    {
        calls.push_back({Op::Create, source});
        if (auto e = failure(Op::Create)) {
            return std::unexpected(*e);
        }
        suppressionExists = true;
        suppressionThreshold = threshold;
        return {};
    }

    [[nodiscard]] shared_kernel::Result<void>
    updateNoiseSuppression(audio_processing::VoiceThreshold threshold) override
    {
        calls.push_back({Op::Update, std::nullopt});
        if (auto e = failure(Op::Update)) {
            return std::unexpected(*e);
        }
        suppressionThreshold = threshold;
        return {};
    }

    [[nodiscard]] shared_kernel::Result<void> removeNoiseSuppression() override
    {
        calls.push_back({Op::Remove, std::nullopt});
        if (auto e = failure(Op::Remove)) {
            return std::unexpected(*e);
        }
        suppressionExists = false;
        return {};
    }

    [[nodiscard]] std::string noiseSuppressionSourceName() const override { return "ghub.g733.mic"; }

    [[nodiscard]] shared_kernel::Result<std::optional<std::string>> configuredDefaultSource() override
    {
        calls.push_back({Op::GetDefault, std::nullopt});
        if (auto e = failure(Op::GetDefault)) {
            return std::unexpected(*e);
        }
        return configuredDefault;
    }

    [[nodiscard]] shared_kernel::Result<void>
    setConfiguredDefaultSource(const std::optional<std::string>& sourceName) override
    {
        calls.push_back({Op::SetDefault, sourceName});
        if (auto e = failure(Op::SetDefault)) {
            return std::unexpected(*e);
        }
        configuredDefault = sourceName;
        return {};
    }

    void setNoiseSuppressionObserver(audio_processing::NoiseSuppressionObserver* newObserver) override
    {
        observer = newObserver;
    }

    void failOn(Op op, std::string detail = "falha programada")
    {
        failures[op] = shared_kernel::DomainError{.code = shared_kernel::ErrorCode::BackendUnavailable,
                                                  .detail = std::move(detail)};
    }

    [[nodiscard]] std::vector<Op> ops() const
    {
        std::vector<Op> out;
        for (const auto& call : calls) {
            out.push_back(call.op);
        }
        return out;
    }

    std::vector<Call> calls;
    std::string headsetSource = "alsa_input.headset";
    std::optional<std::string> configuredDefault;
    bool suppressionExists = false;
    std::optional<audio_processing::VoiceThreshold> suppressionThreshold;
    audio_processing::NoiseSuppressionObserver* observer = nullptr;

private:
    std::optional<shared_kernel::DomainError> failure(Op op) const
    {
        const auto it = failures.find(op);
        return it == failures.end() ? std::nullopt : std::optional{it->second};
    }

    std::map<Op, shared_kernel::DomainError> failures;
};

} // namespace ghub::fakes
