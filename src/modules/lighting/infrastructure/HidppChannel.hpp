#pragma once

#include "modules/lighting/infrastructure/HidReportIo.hpp"
#include "modules/lighting/infrastructure/HidppError.hpp"
#include "modules/lighting/infrastructure/HidppFeatureMap.hpp"
#include "modules/lighting/infrastructure/HidppTrace.hpp"
#include "shared_kernel/Clock.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <utility>

namespace ghub::lighting::infrastructure {

// Recebe os relatórios lidos que não são a resposta esperada (notificações, outros dispositivos); só leitura.
using HidppNotificationHandler = std::function<void(std::span<const std::uint8_t> report)>;

// Canal HID++ 2.0 de uma sessão sobre um nó hidraw (INC-008 R2/R3/R4). Dono do HidReportIo e do mapa de features.
// Não é seguro para threads: um chamador por vez (o DeviceLease garante o escritor único).
class HidppChannel {
public:
    static constexpr std::uint8_t softwareId = 0x0B;
    static constexpr std::size_t maxParams = 16;
    static constexpr std::size_t reportSize = 20;
    static constexpr std::chrono::milliseconds defaultTimeout{300};

    using Response = std::array<std::uint8_t, maxParams>; // bytes 4..19 do relatório 0x11 de resposta

    HidppChannel(std::unique_ptr<HidReportIo> io, const shared_kernel::Clock& clock,
                 std::chrono::milliseconds timeout = defaultTimeout, HidppTraceSink trace = {});

    // Cada chamada informa ao `trace` (se houver) exatamente um resumo com o resultado final, sem os parâmetros.
    // Ordem: política -> índice da feature (consulta Root se faltar) -> escrita -> leitura até casar a resposta.
    // Não reenvia nada. `timeout` é o prazo total da chamada, incluindo a consulta Root.
    [[nodiscard]] std::expected<Response, HidppError> call(std::uint8_t deviceIndex, std::uint16_t featureId,
                                                           std::uint8_t function,
                                                           std::span<const std::uint8_t> params = {});

    // Observador dos relatórios que não casam com a resposta esperada (durante call() ou pollNotifications()).
    void setNotificationHandler(HidppNotificationHandler handler) { handler_ = std::move(handler); }
    // Lê sem esperar (prazo zero) até não haver relatório, entregando cada um ao handler. Não escreve nada.
    [[nodiscard]] std::expected<void, HidppError> pollNotifications();
    // Índice da feature no dispositivo (consulta Root fn0 se faltar). Ausente -> FeatureNotPresent.
    [[nodiscard]] std::expected<std::uint8_t, HidppError> featureIndex(std::uint8_t deviceIndex,
                                                                       std::uint16_t featureId);

private:
    void notify(std::span<const std::uint8_t> report) const;
    [[nodiscard]] std::expected<Response, HidppError> perform(std::uint8_t deviceIndex, std::uint16_t featureId,
                                                              std::uint8_t function,
                                                              std::span<const std::uint8_t> params);
    [[nodiscard]] std::expected<Response, HidppError> exchange(std::uint8_t deviceIndex, std::uint8_t featureIndex,
                                                               std::uint8_t function,
                                                               std::span<const std::uint8_t> params,
                                                               shared_kernel::Instant deadline);
    [[nodiscard]] std::expected<std::uint8_t, HidppError> resolve(std::uint8_t deviceIndex, std::uint16_t featureId,
                                                                  shared_kernel::Instant deadline);

    std::unique_ptr<HidReportIo> io_;
    const shared_kernel::Clock& clock_;
    std::chrono::milliseconds timeout_;
    HidppFeatureMap features_;
    HidppTraceSink trace_;
    HidppNotificationHandler handler_;
};

} // namespace ghub::lighting::infrastructure
