#include "modules/lighting/infrastructure/G915HidppControlSession.hpp"

#include "HidppFixture.hpp"
#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::GameModeKeys;
using ghub::lighting::domain::KeyId;
using ghub::lighting::domain::OnboardSlot;
using ghub::lighting::infrastructure::G915HidppControlSession;
using ghub::lighting::testing::Frame;
using ghub::lighting::testing::longReport;
using ghub::shared_kernel::ErrorCode;

constexpr std::uint8_t fn(std::uint8_t function)
{
    return static_cast<std::uint8_t>((function << 4) | ghub::lighting::testing::kSwid);
}

OnboardSlot slot(std::uint8_t number)
{
    return *OnboardSlot::create(number); // NOLINT(bugprone-unchecked-optional-access)
}

class G915HidppControlSessionTest : public ghub::lighting::testing::HidppFixture {
protected:
    G915HidppControlSessionTest()
    {
        features = {0x1004, 0x4522, 0x0001, 0x8100};
        installDevice();
    }

    // Diretório do setor 0 e perfil atual, como na evidência de 2026-10-14 (setores 1..3 habilitados, ativo 1).
    void installDevice()
    {
        io->setResponder([this](const Frame& frame) -> std::vector<Frame> {
            if (frame[2] != indexOf(0x8100) || indexOf(0x8100) == 0) {
                return respond(frame);
            }
            const std::uint8_t function = frame[3] >> 4;
            const auto header = static_cast<std::uint8_t>((function << 4) | (frame[3] & 0x0F));
            if (function == 5) {
                std::vector<std::uint8_t> chunk(16, 0xFF);
                if (frame[4] == 0 && frame[5] == 0 && frame[6] == 0 && frame[7] == 0) {
                    chunk = directory;
                }
                return {longReport(frame[1], frame[2], header, chunk)};
            }
            if (function == 4) {
                return {longReport(frame[1], frame[2], header, {0x00, activeSector})};
            }
            return respond(frame);
        });
    }

    [[nodiscard]] std::vector<Frame> featureFrames() const
    {
        std::vector<Frame> frames;
        for (const auto& frame : io->written()) {
            if (frame[2] != 0) {
                frames.push_back(frame);
            }
        }
        return frames;
    }

    std::vector<std::uint8_t> directory{0x00, 0x01, 0x01, 0xFF, 0x00, 0x02, 0x01, 0xFF,
                                        0x00, 0x03, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    std::uint8_t activeSector = 0x01;
    G915HidppControlSession session{*channel, device};
};

} // namespace

TEST_F(G915HidppControlSessionTest, GameModeWithF1ClearsThenSetsTheCodeAsInCapture)
{
    const auto f1 = *G915KeyCatalog::findByName("Key: F1"); // NOLINT(bugprone-unchecked-optional-access)
    const auto keys = GameModeKeys::create(std::vector<KeyId>{f1});
    ASSERT_TRUE(keys.has_value());

    const auto done = session.setGameMode(*keys);

    ASSERT_TRUE(done.has_value()) << done.error().detail;
    // C-04, 2026-10-15 18:24:41: 4522 fn3 sem parâmetros e depois fn1 `3a`.
    EXPECT_EQ(featureFrames(), (std::vector<Frame>{longReport(device, indexOf(0x4522), fn(3)),
                                                   longReport(device, indexOf(0x4522), fn(1), {0x3A})}));
}

TEST_F(G915HidppControlSessionTest, EmptyGameModeClearsThenSendsAnEmptyList)
{
    const auto keys = GameModeKeys::create({});

    ASSERT_TRUE(session.setGameMode(*keys).has_value());

    // C-04, 2026-10-15 18:24:45: fn3 e fn1 vazio reativam todas as teclas.
    EXPECT_EQ(featureFrames(), (std::vector<Frame>{longReport(device, indexOf(0x4522), fn(3)),
                                                   longReport(device, indexOf(0x4522), fn(1))}));
}

TEST_F(G915HidppControlSessionTest, SuggestedGameModeSendsTheWindowsKeysAndMenu)
{
    ASSERT_TRUE(session.setGameMode(GameModeKeys::suggested()).has_value());

    EXPECT_EQ(featureFrames().back(), longReport(device, indexOf(0x4522), fn(1), {0xE3, 0xE7, 0x65}));
}

TEST_F(G915HidppControlSessionTest, FailureOfTheClearDoesNotSendTheKeys)
{
    io->setResponder([this](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == indexOf(0x4522)) {
            return {longReport(frame[1], 0xFF, frame[2], {frame[3], 0x02})};
        }
        return respond(frame);
    });

    const auto done = session.setGameMode(GameModeKeys::suggested());

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(featureFrames().size(), 1U);
}

TEST_F(G915HidppControlSessionTest, MissingGameModeFeatureIsUnavailableWithoutWriting)
{
    features = {0x1004, 0x0001, 0x8100};

    const auto done = session.setGameMode(GameModeKeys::suggested());

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(featureFrames().empty());
}

TEST_F(G915HidppControlSessionTest, SelectSlotSendsSectorBigEndianAsInCapture)
{
    ASSERT_TRUE(session.selectSlot(slot(2)).has_value());

    // C-04, 2026-10-15 18:30:17: 8100 fn3 `00 02`.
    EXPECT_EQ(featureFrames(), (std::vector<Frame>{longReport(device, indexOf(0x8100), fn(3), {0x00, 0x02})}));
}

TEST_F(G915HidppControlSessionTest, ReadSlotsListsTheEnabledEntriesAndTheActiveOne)
{
    activeSector = 0x02;

    const auto slots = session.readSlots();

    ASSERT_TRUE(slots.has_value()) << slots.error().detail;
    ASSERT_EQ(slots->enabled.size(), 3U);
    EXPECT_EQ(slots->enabled[0], slot(1));
    EXPECT_EQ(slots->enabled[2], slot(3));
    ASSERT_TRUE(slots->active.has_value());
    EXPECT_EQ(*slots->active, slot(2));
}

TEST_F(G915HidppControlSessionTest, ReadSlotsOmitsDisabledEntries)
{
    directory = {0x00, 0x01, 0x01, 0xFF, 0x00, 0x02, 0x00, 0xFF, 0x00, 0x03, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    const auto slots = session.readSlots();

    ASSERT_TRUE(slots.has_value());
    ASSERT_EQ(slots->enabled.size(), 2U);
    EXPECT_EQ(slots->enabled[0], slot(1));
    EXPECT_EQ(slots->enabled[1], slot(3));
}

TEST_F(G915HidppControlSessionTest, ReadSlotsOnlyReads)
{
    ASSERT_TRUE(session.readSlots().has_value());

    for (const auto& frame : featureFrames()) {
        const std::uint8_t function = frame[3] >> 4;
        EXPECT_TRUE(function == 4 || function == 5) << int{function};
    }
}

TEST_F(G915HidppControlSessionTest, ActiveSectorOutsideTheSlotsIsLeftUnknown)
{
    activeSector = 0x09;

    const auto slots = session.readSlots();

    ASSERT_TRUE(slots.has_value());
    EXPECT_FALSE(slots->active.has_value());
}

TEST_F(G915HidppControlSessionTest, EntriesWithSectorsOutsideOneToThreeAreIgnored)
{
    directory = {0x00, 0x01, 0x01, 0xFF, 0x00, 0x07, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    const auto slots = session.readSlots();

    ASSERT_TRUE(slots.has_value());
    ASSERT_EQ(slots->enabled.size(), 1U);
    EXPECT_EQ(slots->enabled[0], slot(1));
}
