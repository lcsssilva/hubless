#pragma once

#include "modules/audio_processing/application/AudioGraph.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Grafo de áudio falso: registra as chamadas em ordem, permite programar falha por operação e guarda a
// preferência de saída padrão e o estado do EQ.
class FakeAudioGraph final : public audio_processing::AudioGraph {
public:
    enum class Op { FindSink, Create, Update, Remove, GetDefault, SetDefault };

    struct Call {
        Op op;
        std::optional<std::string> argument; // sink alvo (Create) ou valor gravado (SetDefault)
        friend bool operator==(const Call&, const Call&) = default;
    };

    [[nodiscard]] shared_kernel::Result<std::string> findHeadsetSink() override
    {
        calls.push_back({Op::FindSink, std::nullopt});
        if (auto e = failure(Op::FindSink)) {
            return std::unexpected(*e);
        }
        return headsetSink;
    }

    [[nodiscard]] shared_kernel::Result<void> createEqualizer(const audio_processing::EqualizerCurve& curve,
                                                              const std::string& targetSink) override
    {
        calls.push_back({Op::Create, targetSink});
        if (auto e = failure(Op::Create)) {
            return std::unexpected(*e);
        }
        equalizerExists = true;
        equalizerCurve = curve;
        return {};
    }

    [[nodiscard]] shared_kernel::Result<void> updateEqualizer(const audio_processing::EqualizerCurve& curve) override
    {
        calls.push_back({Op::Update, std::nullopt});
        if (auto e = failure(Op::Update)) {
            return std::unexpected(*e);
        }
        equalizerCurve = curve;
        return {};
    }

    [[nodiscard]] shared_kernel::Result<void> removeEqualizer() override
    {
        calls.push_back({Op::Remove, std::nullopt});
        if (auto e = failure(Op::Remove)) {
            return std::unexpected(*e);
        }
        equalizerExists = false;
        return {};
    }

    [[nodiscard]] std::string equalizerSinkName() const override { return "ghub.g733.eq"; }

    [[nodiscard]] shared_kernel::Result<std::optional<std::string>> configuredDefaultSink() override
    {
        calls.push_back({Op::GetDefault, std::nullopt});
        if (auto e = failure(Op::GetDefault)) {
            return std::unexpected(*e);
        }
        return configuredDefault;
    }

    [[nodiscard]] shared_kernel::Result<void>
    setConfiguredDefaultSink(const std::optional<std::string>& sinkName) override
    {
        calls.push_back({Op::SetDefault, sinkName});
        if (auto e = failure(Op::SetDefault)) {
            return std::unexpected(*e);
        }
        configuredDefault = sinkName;
        return {};
    }

    void setObserver(audio_processing::AudioGraphObserver* newObserver) override { observer = newObserver; }

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
    std::string headsetSink = "alsa_output.headset";
    std::optional<std::string> configuredDefault;
    bool equalizerExists = false;
    std::optional<audio_processing::EqualizerCurve> equalizerCurve;
    audio_processing::AudioGraphObserver* observer = nullptr;

private:
    std::optional<shared_kernel::DomainError> failure(Op op) const
    {
        const auto it = failures.find(op);
        return it == failures.end() ? std::nullopt : std::optional{it->second};
    }

    std::map<Op, shared_kernel::DomainError> failures;
};

} // namespace ghub::fakes
