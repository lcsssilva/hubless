#pragma once

#include "modules/profiles/application/HardwareSettingsPorts.hpp"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace ghub::test {

// Headset falso (INC-017 parte 2): guarda o "último valor" configurado e registra o que foi aplicado.
class FakeHeadsetSettingsPort final : public profiles::HeadsetSettingsPort {
public:
    std::optional<int> sidetone;
    std::vector<profiles::HeadsetLightSetting> lights;
    // Resultado por chamada; sidetone devolve o valor pedido quando sem erro.
    shared_kernel::Result<void> sidetoneResult{};
    std::vector<shared_kernel::Result<void>> lightResults; // consumidos em ordem; vazio = sucesso
    std::vector<int> appliedSidetone;
    std::vector<profiles::HeadsetLightSetting> appliedLights;

    [[nodiscard]] std::optional<int> lastSidetone() const override { return sidetone; }
    [[nodiscard]] std::vector<profiles::HeadsetLightSetting> lastLights() const override { return lights; }

    [[nodiscard]] shared_kernel::Result<int> applySidetone(int level) override
    {
        appliedSidetone.push_back(level);
        if (!sidetoneResult) {
            return std::unexpected(sidetoneResult.error());
        }
        return level;
    }

    [[nodiscard]] shared_kernel::Result<void> applyLight(const profiles::HeadsetLightSetting& light) override
    {
        const std::size_t index = appliedLights.size();
        appliedLights.push_back(light);
        return index < lightResults.size() ? lightResults[index] : shared_kernel::Result<void>{};
    }
};

class FakeKeyboardEffectPort final : public profiles::KeyboardEffectPort {
public:
    std::optional<profiles::KeyboardEffectSetting> effect;
    shared_kernel::Result<void> applyResult{};
    std::vector<profiles::KeyboardEffectSetting> applied;

    [[nodiscard]] std::optional<profiles::KeyboardEffectSetting> last() const override { return effect; }

    [[nodiscard]] shared_kernel::Result<void> apply(const profiles::KeyboardEffectSetting& wanted) override
    {
        applied.push_back(wanted);
        return applyResult;
    }
};

class FakePointerSettingsPort final : public profiles::PointerSettingsPort {
public:
    shared_kernel::Result<profiles::PointerDpiSetting> current{
        profiles::PointerDpiSetting{.reportRateHz = 1000, .dpiStages = {800, 1600}, .defaultStage = 0,
                                    .dpiShiftStage = 1}};
    shared_kernel::Result<void> applyResult{};
    bool wrote = true;
    int readCalls = 0;
    std::vector<profiles::PointerDpiSetting> applied;

    [[nodiscard]] shared_kernel::Result<profiles::PointerDpiSetting> readCurrent() override
    {
        ++readCalls;
        return current;
    }

    [[nodiscard]] shared_kernel::Result<profiles::PointerDpiApplied> apply(const profiles::PointerDpiSetting& s) override
    {
        applied.push_back(s);
        if (!applyResult) {
            return std::unexpected(applyResult.error());
        }
        return profiles::PointerDpiApplied{.setting = s, .wrote = wrote};
    }
};

} // namespace ghub::test
