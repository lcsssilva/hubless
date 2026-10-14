#pragma once

#include "modules/lighting/application/KeyboardLighting.hpp"
#include "modules/lighting/infrastructure/HidppChannel.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ghub::lighting::infrastructure {

// Sessão HID++ 2.0 com o G915 (INC-008 parte C, ADR-0013): modo ao vivo pelas features 0x8071 (fn5) e 0x8081
// (fn1/fn6/fn7) e, na parte D, envio de efeito de hardware pela 0x8071 fn1 (vale enquanto o host tem o controle; some
// após endLive). `channel` precisa sobreviver à sessão. Não é segura para threads.
class G915HidppSession final : public application::KeyboardLightingSession {
public:
    G915HidppSession(HidppChannel& channel, std::uint8_t deviceIndex) : channel_(&channel), deviceIndex_(deviceIndex) {}
    // O observador instalado no canal guarda `this`: a sessão não se move nem se copia.
    G915HidppSession(const G915HidppSession&) = delete;
    G915HidppSession& operator=(const G915HidppSession&) = delete;
    G915HidppSession(G915HidppSession&&) = delete;
    G915HidppSession& operator=(G915HidppSession&&) = delete;
    ~G915HidppSession() override = default;

    [[nodiscard]] shared_kernel::Result<void> beginLive() override;
    [[nodiscard]] shared_kernel::Result<void> paint(const domain::KeyColorMap& colors) override;
    [[nodiscard]] shared_kernel::Result<void> setEffect(domain::KeyboardZone zone,
                                                        const domain::KeyboardEffect& effect) override;
    [[nodiscard]] shared_kernel::Result<void> endLive() override;
    [[nodiscard]] shared_kernel::Result<bool> controlLost() override;

private:
    // GetInfo (0x8071 fn0) de um cluster: IDs dos efeitos na ordem dos índices. Lido uma vez por sessão.
    struct ClusterEffects {
        std::vector<std::uint16_t> ids;
    };

    [[nodiscard]] shared_kernel::Result<std::uint8_t> effectIndex(std::uint8_t cluster, std::uint16_t location,
                                                                  std::uint16_t effectId);
    [[nodiscard]] shared_kernel::Result<ClusterEffects> readCluster(std::uint8_t cluster, std::uint16_t location);

    [[nodiscard]] shared_kernel::Result<void> send(std::uint16_t featureId, std::uint8_t function,
                                                   std::span<const std::uint8_t> params);

    // Observador dos relatórios que não são respostas (R15): marca a perda do controle.
    void onNotification(std::span<const std::uint8_t> report) noexcept;

    HidppChannel* channel_;                 // não nulo
    std::optional<std::uint8_t> restIndex_; // índice da 0x1D4B (evento 0 = reconexão); ausente = critério (b) off
    bool controlLost_ = false;
    std::uint8_t deviceIndex_;
    std::array<std::optional<ClusterEffects>, 2> clusters_; // índice = cluster (0 logo, 1 teclado)
};

} // namespace ghub::lighting::infrastructure
