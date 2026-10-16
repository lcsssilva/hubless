#include "modules/onboard_memory/infrastructure/HidppOnboardMemory.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeHidReportIo.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

using ghub::fakes::FakeClock;
using ghub::fakes::FakeHidReportIo;
using ghub::onboard_memory::infrastructure::HidppOnboardMemory;
using ghub::shared_kernel::ErrorCode;
using Frame = FakeHidReportIo::Frame;

namespace {

// Política de teste no formato da dos módulos donos: Root/FeatureSet fn0-1; 0x8100 fn0/2/4/5/7/8; fn6 só para
// setores 1..profileCount com offset 0.
class ProfileRangePolicy final : public ghub::onboard_memory::OnboardProfilePolicy {
public:
    bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                   std::span<const std::uint8_t> params) const noexcept override
    {
        if (featureId == 0x0000 || featureId == 0x0001) {
            return function == 0 || function == 1;
        }
        if (featureId != 0x8100) {
            return false;
        }
        switch (function) {
        case 0:
        case 2:
        case 4:
        case 5:
        case 7:
        case 8:
            return true;
        case 6: {
            if (params.size() < 6) {
                return false;
            }
            const unsigned sector = (static_cast<unsigned>(params[0]) << 8U) | params[1];
            return sector >= 1 && sector <= profileCount_ && params[2] == 0 && params[3] == 0;
        }
        default:
            return false;
        }
    }
    void setProfileCount(std::uint8_t profileCount) noexcept override { profileCount_ = profileCount; }

private:
    std::uint8_t profileCount_ = 0;
};

constexpr std::uint8_t kDevice = 0x01;
constexpr std::uint8_t kOnboardIndex = 8; // de propósito diferente de qualquer índice "conhecido"
constexpr std::uint8_t kSwidHeader = 0x0B;

Frame longReport(std::uint8_t index, std::uint8_t functionAndSwid, const std::vector<std::uint8_t>& params = {})
{
    Frame frame(20, 0);
    frame[0] = 0x11;
    frame[1] = kDevice;
    frame[2] = index;
    frame[3] = functionAndSwid;
    std::copy(params.begin(), params.end(), frame.begin() + 4);
    return frame;
}

std::uint8_t functionOf(const Frame& frame)
{
    return static_cast<std::uint8_t>(frame[3] >> 4U);
}

// Mouse simulado: resolve a 0x8100 no índice 8 pela Root e responde às funções da feature.
class HidppOnboardMemoryTest : public ::testing::Test {
protected:
    struct Failure {
        std::uint8_t function;
        int occurrence; // 1-based, entre os quadros dessa função
        bool silent;    // true: sem resposta (prazo esgotado); false: erro HID++ 2.0
    };

    HidppOnboardMemoryTest()
    {
        auto fake = std::make_unique<FakeHidReportIo>(&clock);
        io = fake.get();
        io->setResponder([this](const Frame& frame) { return respond(frame); });
        memory = std::make_unique<HidppOnboardMemory>(std::move(fake), clock, kDevice,
                                                      std::make_unique<ProfileRangePolicy>());
        sectorData[1] = pattern(255, 1);
        sectorData[0x0101] = pattern(255, 101);
    }

    static std::vector<std::uint8_t> pattern(std::size_t size, int seed)
    {
        std::vector<std::uint8_t> bytes(size);
        std::iota(bytes.begin(), bytes.end(), static_cast<std::uint8_t>(seed));
        return bytes;
    }

    std::vector<Frame> respond(const Frame& frame)
    {
        const std::uint8_t index = frame[2];
        const std::uint8_t header = frame[3];
        const std::uint8_t function = functionOf(frame);
        if (index == 0 && function == 0) {
            const bool onboard = frame[4] == 0x81 && frame[5] == 0x00;
            return {longReport(0, header, {onboard ? kOnboardIndex : std::uint8_t{0}, 0x00, 0x02})};
        }
        if (index != kOnboardIndex) {
            return {};
        }
        ++counts[function];
        for (const auto& failure : failures) {
            if (failure.function == function && failure.occurrence == counts[function]) {
                if (failure.silent) {
                    return {};
                }
                return {errorFor(frame)};
            }
        }
        switch (function) {
        case 0: // memory_model, profile_format, macro_format, profile_count=5, oob=1, buttons, sectors, size 255
            return {longReport(index, header, {0x01, 0x03, 0x01, 5, 1, 16, 16, 0x00, 0xFF, 0x00, 0x00})};
        case 2:
            return {longReport(index, header, {onboardMode})};
        case 4:
            return {longReport(index, header, {0x00, 0x03})};
        case 5: {
            const auto sector = static_cast<std::uint16_t>((frame[4] << 8U) | frame[5]);
            const auto offset = static_cast<std::size_t>((frame[6] << 8U) | frame[7]);
            std::vector<std::uint8_t> chunk(16, 0);
            const auto& data = sectorData[sector];
            for (std::size_t i = 0; i < 16 && offset + i < data.size(); ++i) {
                chunk[i] = data[offset + i];
            }
            return {longReport(index, header, chunk)};
        }
        default:
            return {longReport(index, header, {})};
        }
    }

    // Erro HID++ 2.0: 11 dev FF idx fnsw code.
    static Frame errorFor(const Frame& frame)
    {
        Frame error(20, 0);
        error[0] = 0x11;
        error[1] = frame[1];
        error[2] = 0xFF;
        error[3] = frame[2];
        error[4] = frame[3];
        error[5] = 0x05;
        return error;
    }

    [[nodiscard]] std::vector<Frame> writesOf(std::uint8_t function) const
    {
        std::vector<Frame> result;
        for (const auto& frame : io->written()) {
            if (frame[2] == kOnboardIndex && functionOf(frame) == function) {
                result.push_back(frame);
            }
        }
        return result;
    }

    [[nodiscard]] bool anyWriteFrame() const
    {
        return std::ranges::any_of(
            io->written(), [](const Frame& frame) { return frame[2] == kOnboardIndex && functionOf(frame) >= 6; });
    }

    FakeClock clock;
    FakeHidReportIo* io = nullptr;
    std::unique_ptr<HidppOnboardMemory> memory;
    std::map<std::uint16_t, std::vector<std::uint8_t>> sectorData;
    std::map<std::uint8_t, int> counts;
    std::vector<Failure> failures;
    std::uint8_t onboardMode = 1;
};

} // namespace

TEST_F(HidppOnboardMemoryTest, DescribeReadsLayoutAfterResolvingTheFeatureThroughRoot)
{
    const auto layout = memory->describe();

    ASSERT_TRUE(layout.has_value()) << layout.error().detail;
    EXPECT_EQ(layout->sectorSize, 255);
    EXPECT_EQ(layout->profileCount, 5);
    ASSERT_EQ(io->written().size(), 2U);
    EXPECT_EQ(io->written()[0], longReport(0x00, 0x0B, {0x81, 0x00}));
    EXPECT_EQ(io->written()[1], longReport(kOnboardIndex, 0x0B));
}

TEST_F(HidppOnboardMemoryTest, ReadsOnboardModeAndCurrentProfileSector)
{
    onboardMode = 2;

    const auto mode = memory->onboardMode();
    const auto sector = memory->currentProfileSector();

    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(*mode, 2);
    ASSERT_TRUE(sector.has_value());
    EXPECT_EQ(*sector, 3);
    EXPECT_EQ(io->written().back(), longReport(kOnboardIndex, 0x4B));
}

TEST_F(HidppOnboardMemoryTest, ReadSectorStepsBySixteenAndRereadsTheTailFromSizeMinusSixteen)
{
    const auto bytes = memory->readSector(1, 255);

    ASSERT_TRUE(bytes.has_value()) << bytes.error().detail;
    EXPECT_EQ(*bytes, sectorData[1]);
    const auto reads = writesOf(5);
    ASSERT_EQ(reads.size(), 16U);
    for (std::size_t i = 0; i < 15; ++i) {
        EXPECT_EQ(reads[i], longReport(kOnboardIndex, 0x5B, {0x00, 0x01, 0x00, static_cast<std::uint8_t>(i * 16)}))
            << "leitura " << i;
    }
    EXPECT_EQ(reads[15], longReport(kOnboardIndex, 0x5B, {0x00, 0x01, 0x00, 239}));
}

TEST_F(HidppOnboardMemoryTest, ReadSectorSmallerThanOneChunkIsAnErrorWithoutAnyReport)
{
    const auto bytes = memory->readSector(1, 8);

    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(io->written().empty());
}

TEST_F(HidppOnboardMemoryTest, WriteSectorSendsAddressThenSixteenBodiesThenEnd)
{
    ASSERT_TRUE(memory->describe().has_value());
    const auto before = io->written().size();
    const auto bytes = pattern(255, 0x10);

    const auto result = memory->writeSector(2, bytes);

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    const std::vector<Frame> sent(io->written().begin() + static_cast<std::ptrdiff_t>(before), io->written().end());
    ASSERT_EQ(sent.size(), 18U);
    EXPECT_EQ(sent[0], longReport(kOnboardIndex, 0x6B, {0x00, 0x02, 0x00, 0x00, 0x00, 0xFF}));
    for (std::size_t block = 0; block < 15; ++block) {
        std::vector<std::uint8_t> chunk(bytes.begin() + static_cast<std::ptrdiff_t>(block * 16),
                                        bytes.begin() + static_cast<std::ptrdiff_t>(block * 16 + 16));
        EXPECT_EQ(sent[1 + block], longReport(kOnboardIndex, 0x7B, chunk)) << "bloco " << block;
    }
    std::vector<std::uint8_t> last(bytes.begin() + 240, bytes.end()); // 15 bytes
    ASSERT_EQ(last.size(), 15U);
    last.push_back(0x00);
    EXPECT_EQ(sent[16], longReport(kOnboardIndex, 0x7B, last));
    EXPECT_EQ(sent[17], longReport(kOnboardIndex, 0x8B));
}

TEST_F(HidppOnboardMemoryTest, WriteSectorBeforeDescribeSendsNothing)
{
    const auto result = memory->writeSector(1, pattern(255, 0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(io->written().empty());
}

TEST_F(HidppOnboardMemoryTest, WriteSectorWithTheWrongSizeSendsNoWriteReport)
{
    ASSERT_TRUE(memory->describe().has_value());

    for (const std::size_t size : {std::size_t{254}, std::size_t{256}, std::size_t{16}}) {
        const auto result = memory->writeSector(1, pattern(size, 0));
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    }
    EXPECT_FALSE(anyWriteFrame());
}

TEST_F(HidppOnboardMemoryTest, PolicyRefusesTheDirectoryRomAndSectorsBeyondTheProfileCount)
{
    ASSERT_TRUE(memory->describe().has_value());

    for (const std::uint16_t sector :
         {std::uint16_t{0}, std::uint16_t{6}, std::uint16_t{0x0100}, std::uint16_t{0x0101}}) {
        const auto result = memory->writeSector(sector, pattern(255, 0));
        ASSERT_FALSE(result.has_value()) << "setor " << sector;
        EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    }
    EXPECT_FALSE(anyWriteFrame());
}

TEST_F(HidppOnboardMemoryTest, DeviceErrorOnTheThirdBodyStopsEverythingAfterIt)
{
    ASSERT_TRUE(memory->describe().has_value());
    failures.push_back({.function = 7, .occurrence = 3, .silent = false});

    const auto result = memory->writeSector(1, pattern(255, 0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(writesOf(6).size(), 1U);
    EXPECT_EQ(writesOf(7).size(), 3U);
    EXPECT_TRUE(writesOf(8).empty());
    EXPECT_EQ(functionOf(io->written().back()), 7);
}

TEST_F(HidppOnboardMemoryTest, TimeoutOnTheEndReportIsAnErrorWithoutRetry)
{
    ASSERT_TRUE(memory->describe().has_value());
    failures.push_back({.function = 8, .occurrence = 1, .silent = true});

    const auto result = memory->writeSector(1, pattern(255, 0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(writesOf(8).size(), 1U);
    EXPECT_EQ(functionOf(io->written().back()), 8);
}

TEST_F(HidppOnboardMemoryTest, DeviceErrorOnTheAddressReportSendsNoBody)
{
    ASSERT_TRUE(memory->describe().has_value());
    failures.push_back({.function = 6, .occurrence = 1, .silent = false});

    const auto result = memory->writeSector(1, pattern(255, 0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(writesOf(7).empty());
    EXPECT_TRUE(writesOf(8).empty());
}

TEST_F(HidppOnboardMemoryTest, FailedDescribeKeepsWritesBlocked)
{
    failures.push_back({.function = 0, .occurrence = 1, .silent = true});

    ASSERT_FALSE(memory->describe().has_value());
    const auto result = memory->writeSector(1, pattern(255, 0));

    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(anyWriteFrame());
}
