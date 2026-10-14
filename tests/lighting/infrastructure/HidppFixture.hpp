#pragma once

#include "fakes/FakeClock.hpp"
#include "fakes/FakeHidReportIo.hpp"
#include "modules/lighting/infrastructure/LightingHidppPolicy.hpp"
#include "platform/hidpp/HidppChannel.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace hidpp = ghub::platform::hidpp;

namespace ghub::lighting::testing {

using Frame = fakes::FakeHidReportIo::Frame;

constexpr std::uint8_t kSwid = 0x0B;

inline Frame longReport(std::uint8_t device, std::uint8_t index, std::uint8_t functionAndSwid,
                        const std::vector<std::uint8_t>& params = {})
{
    Frame frame(20, 0);
    frame[0] = 0x11;
    frame[1] = device;
    frame[2] = index;
    frame[3] = functionAndSwid;
    std::copy(params.begin(), params.end(), frame.begin() + 4);
    return frame;
}

// Dispositivo HID++ simulado. As features ficam em índices que NÃO coincidem com os fixos do OpenRGB/HeadsetControl;
// Root fica em 0 por especificação. Responde Root GetFeature, FeatureSet fn0/fn1 e ecoa os parâmetros nos demais.
// O fixture verifica no TearDown que nenhum quadro escrito tem par (feature, função) fora da política.
class HidppFixture : public ::testing::Test {
protected:
    static constexpr std::uint8_t device = 0x01;

    HidppFixture()
    {
        auto fake = std::make_unique<fakes::FakeHidReportIo>(&clock);
        io = fake.get();
        io->setResponder([this](const Frame& frame) { return respond(frame); });
        channel.emplace(std::move(fake), clock, policy);
    }

    void TearDown() override
    {
        for (const auto& frame : io->written()) {
            ASSERT_EQ(frame.size(), 20U);
            ASSERT_EQ(frame[0], 0x11);
            const std::uint8_t index = frame[2];
            const std::uint8_t function = frame[3] >> 4;
            std::uint16_t featureId = 0;
            if (index != 0) {
                ASSERT_LE(index, features.size()) << "índice de feature desconhecido do dispositivo simulado";
                featureId = features[index - 1U];
            }
            EXPECT_TRUE(infrastructure::LightingHidppPolicy::isAllowed(featureId, function))
                << "quadro fora da política: feature " << std::hex << featureId << " fn " << std::dec
                << static_cast<int>(function);
        }
    }

    // Índice de feature (1-based) no dispositivo simulado; 0 se ausente.
    [[nodiscard]] std::uint8_t indexOf(std::uint16_t featureId) const
    {
        const auto it = std::ranges::find(features, featureId);
        return it == features.end() ? std::uint8_t{0} : static_cast<std::uint8_t>(it - features.begin() + 1);
    }

    std::vector<Frame> respond(const Frame& frame)
    {
        const std::uint8_t index = frame[2];
        const std::uint8_t function = frame[3] >> 4;
        const std::uint8_t swid = frame[3] & 0x0F;
        const auto header = static_cast<std::uint8_t>((function << 4) | swid);
        if (index == 0 && function == 0) {
            const auto id = static_cast<std::uint16_t>((frame[4] << 8) | frame[5]);
            const auto found = indexOf(id);
            return {longReport(frame[1], 0, header, {found, 0x00, 0x02})};
        }
        if (index != 0 && features[index - 1U] == 0x0001) {
            if (function == 0) {
                return {longReport(frame[1], index, header, {static_cast<std::uint8_t>(features.size())})};
            }
            if (function == 1) {
                const auto wanted = frame[4];
                const auto id = wanted == 0 ? std::uint16_t{0} : features[wanted - 1U];
                return {longReport(frame[1], index, header,
                                   {static_cast<std::uint8_t>(id >> 8), static_cast<std::uint8_t>(id & 0xFF), 0x60})};
            }
        }
        Frame reply(frame);
        reply[3] = header;
        return {reply};
    }

    fakes::FakeClock clock;
    infrastructure::LightingHidppPolicy policy;
    fakes::FakeHidReportIo* io = nullptr;
    std::optional<hidpp::HidppChannel> channel;
    // Ordem do dispositivo: índice = posição + 1. 0x8081 e 0x8070 existem; 0x8071 também.
    std::vector<std::uint16_t> features{0x1004, 0x8081, 0x0001, 0x8071, 0x8070};
};

} // namespace ghub::lighting::testing
