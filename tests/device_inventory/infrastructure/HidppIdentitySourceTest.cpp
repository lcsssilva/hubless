#include "modules/device_inventory/infrastructure/HidppIdentitySource.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeHidReportIo.hpp"
#include "modules/device_inventory/infrastructure/InventoryIdentityPolicy.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ghub::device_inventory {
namespace {

using fakes::FakeHidReportIo;
using Frame = FakeHidReportIo::Frame;
using platform::hidpp::HidppNodeLocator;
using platform::hidpp::HidReportIo;
using shared_kernel::ErrorCode;

constexpr std::uint8_t kSwid = 0x0B;

Frame longReport(std::uint8_t device, std::uint8_t index, std::uint8_t functionAndSwid,
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

struct SimFirmware {
    std::uint8_t kind;
    std::array<std::uint8_t, 3> prefix;
    std::uint8_t number;
    std::uint8_t revision;
    std::uint16_t build;
    bool active;
    std::uint16_t pid;
};

// Dispositivo HID++ 2.0 simulado: Root no índice 0; a feature i de `features` fica no índice i + 1.
struct SimDevice {
    std::vector<std::uint16_t> features;
    std::vector<std::uint8_t> nameBytes;
    std::uint8_t type = 0;
    std::vector<SimFirmware> firmwares;
    std::optional<std::uint8_t> entityCount; // sobrescreve firmwares.size() (dispositivo com mais entidades)
    std::array<std::uint8_t, 4> unitId{0xA1, 0xB2, 0xC3, 0xD4};
    std::optional<std::uint8_t> hidpp20Error; // responde erro 0xFF com este código a qualquer pedido
    bool silent = false;                      // nunca responde

    [[nodiscard]] std::uint8_t indexOf(std::uint16_t featureId) const
    {
        const auto it = std::ranges::find(features, featureId);
        return it == features.end() ? std::uint8_t{0} : static_cast<std::uint8_t>(it - features.begin() + 1);
    }

    std::vector<Frame> respond(const Frame& frame) const
    {
        if (silent) {
            return {};
        }
        const std::uint8_t device = frame[1];
        const std::uint8_t index = frame[2];
        const std::uint8_t function = frame[3] >> 4;
        const auto header = static_cast<std::uint8_t>((function << 4) | (frame[3] & 0x0F));
        if (hidpp20Error) {
            return {longReport(device, 0xFF, static_cast<std::uint8_t>(index), {header, *hidpp20Error})};
        }
        if (index == 0) {
            const auto id = static_cast<std::uint16_t>((frame[4] << 8) | frame[5]);
            return {longReport(device, 0, header, {indexOf(id), 0x00, 0x02})};
        }
        const std::uint16_t feature = features.at(index - 1U);
        const std::uint8_t arg = frame[4];
        switch (feature) {
        case 0x0001:
            if (function == 0) {
                return {longReport(device, index, header, {static_cast<std::uint8_t>(features.size())})};
            }
            return {longReport(device, index, header,
                               {static_cast<std::uint8_t>(features.at(arg - 1U) >> 8),
                                static_cast<std::uint8_t>(features.at(arg - 1U) & 0xFF), 0x60})};
        case 0x0005:
            if (function == 0) {
                return {longReport(device, index, header, {static_cast<std::uint8_t>(nameBytes.size())})};
            }
            if (function == 1) {
                std::vector<std::uint8_t> chunk;
                for (std::size_t i = arg; i < arg + 16U && i < nameBytes.size(); ++i) {
                    chunk.push_back(nameBytes[i]);
                }
                return {longReport(device, index, header, chunk)};
            }
            return {longReport(device, index, header, {type})};
        case 0x0003:
            if (function == 0) {
                const auto count = entityCount.value_or(static_cast<std::uint8_t>(firmwares.size()));
                return {longReport(device, index, header,
                                   {count, unitId[0], unitId[1], unitId[2], unitId[3], 0x00, 0x04, 0x40, 0x7C})};
            }
            if (function == 1) {
                const auto& fw = firmwares.at(arg);
                return {
                    longReport(device, index, header,
                               {fw.kind, fw.prefix[0], fw.prefix[1], fw.prefix[2], fw.number, fw.revision,
                                static_cast<std::uint8_t>(fw.build >> 8), static_cast<std::uint8_t>(fw.build & 0xFF),
                                static_cast<std::uint8_t>(fw.active ? 1 : 0), static_cast<std::uint8_t>(fw.pid >> 8),
                                static_cast<std::uint8_t>(fw.pid & 0xFF)})};
            }
            break;
        default:
            break;
        }
        return {longReport(device, index, header)};
    }
};

std::vector<std::uint8_t> bytesOf(const std::string& text)
{
    return {text.begin(), text.end()};
}

SimDevice makeDevice(std::vector<std::uint16_t> features, const std::string& name, std::uint8_t type,
                     std::vector<SimFirmware> firmwares)
{
    SimDevice sim;
    sim.features = std::move(features);
    sim.nameBytes = bytesOf(name);
    sim.type = type;
    sim.firmwares = std::move(firmwares);
    return sim;
}

// Bytes da captura C-00 (INC-015): 0x0003 fn1 da entidade de aplicação e 0x0005 fn2; unit ID redigido. As demais
// entidades, os nomes e as listas de features são inventados para o teste.
SimDevice g915()
{
    return makeDevice({0x0001, 0x0003, 0x0005, 0x1004, 0x8071, 0x8081, 0x8100},
                      "G915 X LIGHTSPEED", // 17 caracteres: dois blocos
                      0,
                      {{1, {'B', 'L', 'K'}, 0x01, 0x00, 0x0001, false, 0x407C},
                       {2, {'H', 'W', ' '}, 0x03, 0x01, 0x0002, false, 0x407C},
                       {0, {'M', 'P', 'K'}, 0x09, 0x04, 0x0042, true, 0x407C}});
}

SimDevice g703()
{
    return makeDevice({0x0001, 0x0003, 0x0005, 0x1004, 0x8100}, "G703 LIGHTSPEED", 3,
                      {{0, {'M', 'P', 'M'}, 0x22, 0x02, 0x0015, true, 0x4086}});
}

SimDevice g733()
{
    return makeDevice({0x0001, 0x0003, 0x0005, 0x1000}, "G733 Gaming Headset", 8,
                      {{0, {'U', '2', ' '}, 0x00, 0x06, 0x0000, true, 0x0AFE}});
}

class HidppIdentitySourceTest : public ::testing::Test {
protected:
    // Fecha o ciclo de vida do fake: o canal é dono do HidReportIo, o teste guarda o registro dos quadros.
    class ForwardingIo final : public HidReportIo {
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
               ("ghub_identity_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "class" / "hidraw");
        addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2"); // receptor do G915
        addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:1");
        addNode("hidraw12", "0003:0000046D:0000C539", "usb-0000:11:00.4-1.2/input2"); // receptor do G703
        addNode("hidraw13", "0003:0000046D:00004086", "usb-0000:11:00.4-1.2/input2:2");
        addNode("hidraw3", "0003:0000046D:00000AFE", "usb-0000:0e:00.0-2/input3"); // G733
        io = std::make_unique<FakeHidReportIo>(&clock);
        io->setResponder([this](const Frame& frame) { return device.respond(frame); });
    }

    void TearDown() override
    {
        std::filesystem::remove_all(root);
        // Nenhum quadro enviado pode estar fora da lista fechada de INC-015 R2.
        const InventoryIdentityPolicy policy;
        for (const auto& frame : io->written()) {
            ASSERT_EQ(frame.size(), 20U);
            ASSERT_EQ(frame[0], 0x11);
            ASSERT_EQ(frame[3] & 0x0F, kSwid);
            const std::uint16_t featureId = frame[2] == 0 ? std::uint16_t{0} : device.features.at(frame[2] - 1U);
            EXPECT_TRUE(policy.isAllowed(featureId, frame[3] >> 4, {}))
                << "quadro fora da política: feature " << std::hex << featureId << " fn " << std::dec
                << (frame[3] >> 4);
        }
    }

    void addNode(const std::string& name, const std::string& hidId, const std::string& phys)
    {
        const auto dir = root / "class" / "hidraw" / name / "device";
        std::filesystem::create_directories(dir);
        std::ofstream{dir / "uevent"} << "HID_ID=" << hidId << "\nHID_PHYS=" << phys << "\n";
    }

    [[nodiscard]] HidppIdentitySource source()
    {
        return HidppIdentitySource{HidppNodeLocator{root, "/dev"}, clock, [this](const std::filesystem::path& path) {
                                       openedPaths.push_back(path);
                                       if (openFailure) {
                                           return shared_kernel::Result<std::unique_ptr<HidReportIo>>{
                                               shared_kernel::fail(ErrorCode::BackendUnavailable, *openFailure)};
                                       }
                                       return shared_kernel::Result<std::unique_ptr<HidReportIo>>{
                                           std::make_unique<ForwardingIo>(*io, openHandles)};
                                   }};
    }

    // "root 0005" para a consulta Root; "0005 fn1 16" para as demais (primeiro parâmetro só na fn1).
    [[nodiscard]] std::vector<std::string> sequence() const
    {
        std::vector<std::string> lines;
        for (const auto& frame : io->written()) {
            const auto hex4 = [](unsigned value) {
                std::string text(4, '0');
                for (int i = 3; i >= 0; --i, value >>= 4U) {
                    text[static_cast<std::size_t>(i)] = "0123456789abcdef"[value & 0xFU];
                }
                return text;
            };
            if (frame[2] == 0) {
                lines.push_back("root " + hex4(static_cast<unsigned>((frame[4] << 8) | frame[5])));
                continue;
            }
            const unsigned function = frame[3] >> 4;
            std::string line = hex4(device.features.at(frame[2] - 1U)) + " fn" + std::to_string(function);
            if (function == 1) {
                line += " " + std::to_string(frame[4]);
            }
            lines.push_back(line);
        }
        return lines;
    }

    std::filesystem::path root;
    fakes::FakeClock clock;
    SimDevice device = g915();
    std::unique_ptr<FakeHidReportIo> io;
    std::vector<std::filesystem::path> openedPaths;
    std::optional<std::string> openFailure;
    int openHandles = 0;
};

TEST_F(HidppIdentitySourceTest, ReadsTheG915ThroughItsReceiverWithTheExactRequestSequence)
{
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value()) << identity.error().detail;
    EXPECT_EQ(identity->name(), "G915 X LIGHTSPEED");
    EXPECT_EQ(identity->deviceType(), 0);
    ASSERT_EQ(identity->firmwares().size(), 3U);
    EXPECT_EQ(identity->firmwares()[2].describe(), "MPK 09.04 b0042 — aplicação (ativo)");
    EXPECT_EQ(identity->firmwares()[0].describe(), "BLK 01.00 b0001 — bootloader");
    EXPECT_EQ(identity->featureIds(), device.features);
    EXPECT_FALSE(identity->firmwareTruncated());
    EXPECT_FALSE(identity->featuresTruncated());
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw7"}));
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 0005", "0005 fn0", "0005 fn1 0", "0005 fn1 16", "0005 fn2",
                                                    "root 0003", "0003 fn0", "0003 fn1 0", "0003 fn1 1", "0003 fn1 2",
                                                    "root 0001", "0001 fn0", "0001 fn1 1", "0001 fn1 2", "0001 fn1 3",
                                                    "0001 fn1 4", "0001 fn1 5", "0001 fn1 6", "0001 fn1 7"}));
    for (const auto& frame : io->written()) {
        EXPECT_EQ(frame[1], 1) << "índice do dispositivo atrás do receptor";
    }
}

TEST_F(HidppIdentitySourceTest, ReadsTheG703UsingItsDeviceIndexAndMouseType)
{
    device = g703();
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G703);

    ASSERT_TRUE(identity.has_value()) << identity.error().detail;
    EXPECT_EQ(identity->name(), "G703 LIGHTSPEED");
    EXPECT_EQ(identity->deviceType(), 3);
    ASSERT_EQ(identity->firmwares().size(), 1U);
    EXPECT_EQ(identity->firmwares()[0].versionText(), "MPM 22.02 b0015");
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw12"}));
    for (const auto& frame : io->written()) {
        EXPECT_EQ(frame[1], 2);
    }
}

TEST_F(HidppIdentitySourceTest, ReadsTheG733WithTheBroadcastIndex)
{
    device = g733();
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G733);

    ASSERT_TRUE(identity.has_value()) << identity.error().detail;
    EXPECT_EQ(identity->deviceType(), 8);
    ASSERT_EQ(identity->firmwares().size(), 1U);
    EXPECT_EQ(identity->firmwares()[0].versionText(), "U2 00.06 b0000");
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw3"}));
    for (const auto& frame : io->written()) {
        EXPECT_EQ(frame[1], 0xFF);
    }
}

TEST_F(HidppIdentitySourceTest, AsksForEachNameBlockOnlyAsLongAsTheLengthRequires)
{
    for (const auto& [length, blocks] :
         {std::pair{1U, 1U}, std::pair{16U, 1U}, std::pair{17U, 2U}, std::pair{64U, 4U}}) {
        SetUp();
        device.nameBytes.assign(length, 'n');
        auto reader = source();

        const auto identity = reader.read(IdentityTarget::G915);

        ASSERT_TRUE(identity.has_value()) << length;
        EXPECT_EQ(identity->name(), std::string(length, 'n')) << length;
        const auto lines = sequence();
        EXPECT_EQ(std::ranges::count_if(lines, [](const std::string& line) { return line.starts_with("0005 fn1"); }),
                  static_cast<std::ptrdiff_t>(blocks))
            << length;
        TearDown();
    }
}

TEST_F(HidppIdentitySourceTest, ReplacesNonPrintableNameBytes)
{
    device.nameBytes = {'G', 0x00, '9', 0xC3, 0x7F};
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(identity->name(), "G?9??");
}

TEST_F(HidppIdentitySourceTest, NameLengthZeroOrAboveSixtyFourIsNotInformedAndNoBlockIsRequested)
{
    for (const std::size_t length : {std::size_t{0}, std::size_t{65}, std::size_t{200}}) {
        SetUp();
        device.nameBytes.assign(length, 'x');
        auto reader = source();

        const auto identity = reader.read(IdentityTarget::G915);

        ASSERT_TRUE(identity.has_value()) << length;
        EXPECT_FALSE(identity->name().has_value()) << length;
        EXPECT_EQ(identity->deviceType(), 0) << "o tipo continua sendo lido";
        const auto lines = sequence();
        EXPECT_EQ(std::ranges::count_if(lines, [](const std::string& line) { return line.starts_with("0005 fn1"); }), 0)
            << length;
        TearDown();
    }
}

TEST_F(HidppIdentitySourceTest, AbsentNameFeatureLeavesNameAndTypeUnknownWithoutError)
{
    device.features = {0x0001, 0x0003, 0x1004};
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value()) << identity.error().detail;
    EXPECT_FALSE(identity->name().has_value());
    EXPECT_FALSE(identity->deviceType().has_value());
    EXPECT_EQ(identity->firmwares().size(), 3U);
    EXPECT_EQ(identity->featureIds(), device.features);
}

TEST_F(HidppIdentitySourceTest, AbsentDeviceInfoFeatureLeavesFirmwareEmptyWithoutError)
{
    device.features = {0x0001, 0x0005, 0x1004};
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value()) << identity.error().detail;
    EXPECT_TRUE(identity->firmwares().empty());
    EXPECT_EQ(identity->name(), "G915 X LIGHTSPEED");
    EXPECT_EQ(identity->featureIds(), device.features);
    EXPECT_FALSE(std::ranges::any_of(sequence(), [](const std::string& line) { return line.starts_with("0003 fn"); }));
}

TEST_F(HidppIdentitySourceTest, ZeroFirmwareEntitiesSendNoGetFwInfo)
{
    device.firmwares.clear();
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value());
    EXPECT_TRUE(identity->firmwares().empty());
    EXPECT_FALSE(std::ranges::any_of(sequence(), [](const std::string& line) { return line.starts_with("0003 fn1"); }));
}

TEST_F(HidppIdentitySourceTest, ReadsAtMostEightFirmwareEntitiesAndSaysSo)
{
    device.firmwares.assign(9, SimFirmware{0, {'A', 'B', 'C'}, 0x01, 0x02, 0x0003, false, 0x407C});
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(identity->firmwares().size(), 8U);
    EXPECT_TRUE(identity->firmwareTruncated());
    EXPECT_FALSE(identity->featuresTruncated());
    const auto lines = sequence();
    EXPECT_EQ(std::ranges::count_if(lines, [](const std::string& line) { return line.starts_with("0003 fn1"); }), 8);
}

TEST_F(HidppIdentitySourceTest, ReadsAtMostSixtyFourFeaturesAndSaysSo)
{
    device.features = {0x0001, 0x0003, 0x0005};
    for (std::uint16_t id = 0x1000; device.features.size() < 70; ++id) {
        device.features.push_back(id);
    }
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(identity->featureIds().size(), 64U);
    EXPECT_EQ(identity->featureIds().back(), device.features[63]);
    EXPECT_TRUE(identity->featuresTruncated());
    const auto lines = sequence();
    EXPECT_EQ(std::ranges::count_if(lines, [](const std::string& line) { return line.starts_with("0001 fn1"); }), 64);
}

TEST_F(HidppIdentitySourceTest, ADeviceReportingTwoHundredFiftyFiveFeaturesStillStopsAtSixtyFour)
{
    device.features = {0x0001, 0x0003, 0x0005};
    for (std::uint16_t id = 0x1000; device.features.size() < 255; ++id) {
        device.features.push_back(id);
    }
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(identity->featureIds().size(), 64U);
    EXPECT_TRUE(identity->featuresTruncated());
}

TEST_F(HidppIdentitySourceTest, TheUnitIdNeverReachesTheIdentity)
{
    auto first = source().read(IdentityTarget::G915);
    device.unitId = {0x01, 0x02, 0x03, 0x04};
    SetUp();
    auto second = source().read(IdentityTarget::G915);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, *second) << "dois dispositivos que só diferem no unit ID têm a mesma identidade";
}

TEST_F(HidppIdentitySourceTest, ErrorFiveOnTheHeadsetMeansPoweredOffOrOutOfRange)
{
    device = g733();
    device.hidpp20Error = 0x05;
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G733);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(identity.error().detail, "headset desligado ou fora de alcance");
    EXPECT_EQ(io->written().size(), 1U) << "sem novas tentativas";
}

TEST_F(HidppIdentitySourceTest, ErrorFiveOnTheOtherDevicesIsAPlainDeviceError)
{
    device.hidpp20Error = 0x05;
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(identity.error().detail, "headset desligado ou fora de alcance");
    EXPECT_NE(identity.error().detail.find("0x05"), std::string::npos) << identity.error().detail;
}

TEST_F(HidppIdentitySourceTest, ErrorAfterTheFirstExchangesAbortsWithoutRetryingOrContinuing)
{
    device.hidpp20Error.reset();
    int writes = 0;
    io->setResponder([this, &writes](const Frame& frame) -> std::vector<Frame> {
        if (++writes > 5) {
            return {longReport(frame[1], 0xFF, frame[2], {frame[3], 0x02})};
        }
        return device.respond(frame);
    });
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(io->written().size(), 6U);
}

// Regressão (teste físico de 2026-10-15): o G915 e o G703 dormindo levaram ~1,1 s para a primeira resposta, e o prazo
// padrão de 300 ms do canal dava "tempo esgotado" com o dispositivo conectado.
TEST_F(HidppIdentitySourceTest, WaitsForAWirelessDeviceWakingUpOnTheFirstRequest)
{
    auto reader = source();
    bool asleep = true;
    io->setResponder([this, &asleep](const FakeHidReportIo::Frame& frame) {
        if (asleep) {
            asleep = false;
            clock.advance(std::chrono::milliseconds{1200});
        }
        return device.respond(frame);
    });

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_TRUE(identity.has_value()) << identity.error().detail;
    EXPECT_EQ(identity->name(), "G915 X LIGHTSPEED");
    EXPECT_EQ(io->written().size(), 19U) << "nenhum pedido reenviado";
}

TEST_F(HidppIdentitySourceTest, TimeoutIsAnErrorAndTheRequestIsNotResent)
{
    device.silent = true;
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(identity.error().detail.find("tempo esgotado"), std::string::npos) << identity.error().detail;
    EXPECT_EQ(io->written().size(), 1U);
}

TEST_F(HidppIdentitySourceTest, WriteFailureIsAnIoError)
{
    io->failWrites(platform::hidpp::HidIoError{.failure = platform::hidpp::HidIoFailure::Io, .systemError = 5});
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(identity.error().detail.find("E/S"), std::string::npos) << identity.error().detail;
}

TEST_F(HidppIdentitySourceTest, MissingDeviceIsReportedWithoutOpeningAnything)
{
    std::filesystem::remove_all(root / "class" / "hidraw" / "hidraw3");
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G733);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(openedPaths.empty());
}

TEST_F(HidppIdentitySourceTest, OpenFailureIsPropagatedAndNothingIsWritten)
{
    openFailure = "sem permissão em /dev/hidraw7";
    auto reader = source();

    const auto identity = reader.read(IdentityTarget::G915);

    ASSERT_FALSE(identity.has_value());
    EXPECT_EQ(identity.error().detail, "sem permissão em /dev/hidraw7");
    EXPECT_TRUE(io->written().empty());
}

TEST_F(HidppIdentitySourceTest, TheHidrawIsOnlyOpenDuringTheRead)
{
    auto reader = source();
    EXPECT_EQ(openHandles, 0) << "construir não abre nada";
    EXPECT_TRUE(openedPaths.empty());

    ASSERT_TRUE(reader.read(IdentityTarget::G915).has_value());
    EXPECT_EQ(openHandles, 0) << "o nó é fechado ao fim da leitura";

    device.silent = true;
    ASSERT_FALSE(reader.read(IdentityTarget::G915).has_value());
    EXPECT_EQ(openHandles, 0) << "inclusive quando a leitura falha";
    EXPECT_EQ(openedPaths.size(), 2U) << "cada leitura abre o nó de novo";
}

} // namespace
} // namespace ghub::device_inventory
