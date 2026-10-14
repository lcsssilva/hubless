#pragma once

#include "modules/headset/application/HeadsetSettingsWriter.hpp"

#include <QString>

#include <chrono>

namespace ghub::headset {

// Adaptador do port HeadsetSettingsWriter sobre a CLI do HeadsetControl (ADR-0015 decisão 3).
// Cada chamada executa, de forma SÍNCRONA (use numa thread de trabalho), uma única vez e sem shell:
//   headsetcontrol -d 0x046d:0x0afe -s <n> -o json        (sidetone)
//   headsetcontrol -d 0x046d:0x0afe -l <0|1> -o json      (luzes)
// Ambiente só com LC_ALL=C, stderr descartado, timeout com kill, saída limitada a 64 KiB. A CLI sai com 0
// mesmo quando a ação falha: o sucesso exige a ação da capacidade com status "success" no JSON.
// Nunca repete a execução.
class HeadsetControlSettingsWriter final : public HeadsetSettingsWriter {
public:
    HeadsetControlSettingsWriter(QString executable, std::chrono::milliseconds timeout);

    [[nodiscard]] shared_kernel::Result<void> applySidetone(SidetoneLevel level) override;
    [[nodiscard]] shared_kernel::Result<void> applyLights(HeadsetLights lights) override;

private:
    [[nodiscard]] shared_kernel::Result<void> run(const QStringList& option, const char* capability) const;

    QString executable_;
    std::chrono::milliseconds timeout_;
};

} // namespace ghub::headset
