#pragma once

#include "fakes/FakeClock.hpp"
#include "fakes/FakeHidReportIo.hpp"
#include "platform/hidpp/HidReportIo.hpp"
#include "shared_kernel/Result.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::fakes {

using StatusFrame = FakeHidReportIo::Frame;

inline StatusFrame statusReport(std::uint8_t device, std::uint8_t index, std::uint8_t functionAndSwid,
                                const std::vector<std::uint8_t>& params = {})
{
    StatusFrame frame(20, 0);
    frame[0] = 0x11;
    frame[1] = device;
    frame[2] = index;
    frame[3] = functionAndSwid;
    std::copy(params.begin(), params.end(), frame.begin() + 4);
    return frame;
}

// Dispositivo HID++ 2.0 simulado para as leituras de estado (INC-015 parte 2): Root no índice 0; a feature i de
// `features` fica no índice i + 1; `responses` dá o payload de cada (feature, função).
struct SimStatusDevice {
    std::vector<std::uint16_t> features;
    std::map<std::pair<std::uint16_t, std::uint8_t>, std::vector<std::uint8_t>> responses;
    std::optional<std::uint8_t> hidpp20Error; // responde erro 0xFF com este código a qualquer pedido
    bool silent = false;                      // nunca responde

    [[nodiscard]] std::uint8_t indexOf(std::uint16_t featureId) const
    {
        const auto it = std::ranges::find(features, featureId);
        return it == features.end() ? std::uint8_t{0} : static_cast<std::uint8_t>(it - features.begin() + 1);
    }

    [[nodiscard]] std::vector<StatusFrame> respond(const StatusFrame& frame) const
    {
        if (silent) {
            return {};
        }
        const std::uint8_t device = frame[1];
        const std::uint8_t index = frame[2];
        const std::uint8_t function = frame[3] >> 4;
        const auto header = static_cast<std::uint8_t>((function << 4) | (frame[3] & 0x0F));
        if (hidpp20Error) {
            return {statusReport(device, 0xFF, index, {header, *hidpp20Error})};
        }
        if (index == 0) {
            const auto id = static_cast<std::uint16_t>((frame[4] << 8) | frame[5]);
            return {statusReport(device, 0, header, {indexOf(id), 0x00, 0x02})};
        }
        const std::uint16_t feature = features.at(index - 1U);
        const auto it = responses.find({feature, function});
        return {statusReport(device, index, header, it == responses.end() ? std::vector<std::uint8_t>{} : it->second)};
    }
};

// Base dos testes dos adaptadores de leitura: raiz sysfs temporária, hidraw falso que conta aberturas, relógio falso e
// registro dos quadros escritos. A derivada informa, em `frameAllowed`, a política do módulo (conferida no TearDown).
class StatusReadFixture : public ::testing::Test {
protected:
    using IoPointer = std::unique_ptr<platform::hidpp::HidReportIo>;

    class ForwardingIo final : public platform::hidpp::HidReportIo {
    public:
        ForwardingIo(FakeHidReportIo& target, int& openCount) : target_(target), openCount_(openCount) { ++openCount_; }
        ~ForwardingIo() override { --openCount_; }
        ForwardingIo(const ForwardingIo&) = delete;
        ForwardingIo& operator=(const ForwardingIo&) = delete;
        ForwardingIo(ForwardingIo&&) = delete;
        ForwardingIo& operator=(ForwardingIo&&) = delete;

        std::expected<std::size_t, platform::hidpp::HidIoError> write(std::span<const std::uint8_t> report) override
        {
            return target_.write(report);
        }
        std::expected<std::size_t, platform::hidpp::HidIoError> read(std::span<std::uint8_t> buffer,
                                                                     std::chrono::milliseconds timeout) override
        {
            return target_.read(buffer, timeout);
        }

    private:
        FakeHidReportIo& target_;
        int& openCount_;
    };

    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() /
               ("ghub_status_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "class" / "hidraw");
        addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2"); // receptor do G915
        addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:1");
        addNode("hidraw12", "0003:0000046D:0000C539", "usb-0000:11:00.4-1.2/input2"); // receptor do G703
        addNode("hidraw13", "0003:0000046D:00004086", "usb-0000:11:00.4-1.2/input2:2");
        addNode("hidraw3", "0003:0000046D:00000AFE", "usb-0000:0e:00.0-2/input3"); // G733
        io = std::make_unique<FakeHidReportIo>(&clock);
        io->setResponder([this](const StatusFrame& frame) {
            // O hidraw só pode estar aberto enquanto há pedidos em curso.
            EXPECT_EQ(openHandles, 1);
            return device.respond(frame);
        });
    }

    void TearDown() override
    {
        std::filesystem::remove_all(root);
        EXPECT_EQ(openHandles, 0) << "o hidraw ficou aberto depois da leitura";
        for (const auto& frame : io->written()) {
            ASSERT_EQ(frame.size(), 20U);
            ASSERT_EQ(frame[0], 0x11);
            ASSERT_EQ(frame[3] & 0x0F, 0x0B);
            const std::uint16_t featureId = frame[2] == 0 ? std::uint16_t{0} : device.features.at(frame[2] - 1U);
            std::vector<std::uint8_t> params(frame.begin() + 4, frame.end());
            if (featureId == 0) {
                params.resize(2); // Root GetFeature: só o ID da feature consultada
            } else {
                EXPECT_TRUE(std::ranges::all_of(params, [](std::uint8_t b) { return b == 0; }))
                    << "parâmetros não nulos enviados a " << std::hex << featureId;
                params.resize(paramCount(featureId)); // o que a política vê: os parâmetros do chamador
            }
            EXPECT_TRUE(frameAllowed(featureId, frame[3] >> 4, params))
                << "quadro fora da política: feature " << std::hex << featureId << " fn " << std::dec
                << (frame[3] >> 4);
        }
    }

    // Quantidade de bytes de parâmetro que o adaptador passa ao canal para a feature (o canal completa com zeros).
    [[nodiscard]] virtual std::size_t paramCount(std::uint16_t /*featureId*/) const { return 0; }

    virtual bool frameAllowed(std::uint16_t featureId, std::uint8_t function,
                              const std::vector<std::uint8_t>& params) const = 0;

    void addNode(const std::string& name, const std::string& hidId, const std::string& phys)
    {
        const auto dir = root / "class" / "hidraw" / name / "device";
        std::filesystem::create_directories(dir);
        std::ofstream{dir / "uevent"} << "HID_ID=" << hidId << "\nHID_PHYS=" << phys << "\n";
    }

    // Opener injetável: registra o caminho, conta o handle aberto e entrega o hidraw falso.
    [[nodiscard]] std::function<shared_kernel::Result<IoPointer>(const std::filesystem::path&)> opener()
    {
        return [this](const std::filesystem::path& path) -> shared_kernel::Result<IoPointer> {
            openedPaths.push_back(path);
            if (openFailure) {
                return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, *openFailure);
            }
            return IoPointer{std::make_unique<ForwardingIo>(*io, openHandles)};
        };
    }

    // "root 8040" para a consulta Root; "8040 fn1" para as demais.
    [[nodiscard]] std::vector<std::string> sequence() const
    {
        std::vector<std::string> lines;
        const auto hex4 = [](unsigned value) {
            std::string text(4, '0');
            for (int i = 3; i >= 0; --i, value >>= 4U) {
                text[static_cast<std::size_t>(i)] = "0123456789abcdef"[value & 0xFU];
            }
            return text;
        };
        for (const auto& frame : io->written()) {
            if (frame[2] == 0) {
                lines.push_back("root " + hex4(static_cast<unsigned>((frame[4] << 8) | frame[5])));
                continue;
            }
            lines.push_back(hex4(device.features.at(frame[2] - 1U)) + " fn" + std::to_string(frame[3] >> 4));
        }
        return lines;
    }

    std::filesystem::path root;
    FakeClock clock;
    SimStatusDevice device;
    std::unique_ptr<FakeHidReportIo> io;
    std::vector<std::filesystem::path> openedPaths;
    std::optional<std::string> openFailure;
    int openHandles = 0;
};

} // namespace ghub::fakes
