#include "modules/lighting/infrastructure/G915HidppSession.hpp"

#include "HidppFixture.hpp"
#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace {

using ghub::lighting::domain::EffectBrightness;
using ghub::lighting::domain::EffectPeriod;
using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::KeyboardEffect;
using ghub::lighting::domain::KeyboardZone;
using ghub::lighting::domain::KeyColorMap;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::infrastructure::G915HidppSession;
using ghub::lighting::testing::Frame;
using ghub::lighting::testing::longReport;
using ghub::shared_kernel::ErrorCode;

constexpr std::uint8_t fn(std::uint8_t function)
{
    return static_cast<std::uint8_t>((function << 4) | ghub::lighting::testing::kSwid);
}

KeyColorMap colorsFor(std::initializer_list<const char*> names, RgbColor color)
{
    KeyColorMap colors;
    for (const auto* name : names) {
        colors = colors.with(*G915KeyCatalog::findByName(name), color); // NOLINT(bugprone-unchecked-optional-access)
    }
    return colors;
}

class G915HidppSessionTest : public ghub::lighting::testing::HidppFixture {
protected:
    // Quadros escritos, sem as consultas Root GetFeature (índice 0).
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

    G915HidppSession session{*channel, device};
};

TEST_F(G915HidppSessionTest, BeginLiveHandsEffectAndNvConfigToTheHost)
{
    ASSERT_TRUE(session.beginLive().has_value());

    EXPECT_EQ(featureFrames(), (std::vector<Frame>{longReport(device, indexOf(0x8071), fn(5), {0x01, 0x03, 0x05})}));
}

class G915HidppSessionLostControlTest : public G915HidppSessionTest {
protected:
    G915HidppSessionLostControlTest()
    {
        // 0x1D4B no índice 4, como na evidência de 2026-10-14.
        features = {0x1004, 0x8081, 0x0001, 0x1D4B, 0x8071, 0x8070};
    }

    // Abre o modo ao vivo e esquece o que foi escrito até aqui.
    void beginLive()
    {
        ASSERT_TRUE(session.beginLive().has_value());
        written = io->written().size();
    }

    [[nodiscard]] bool lostAfter(const Frame& report)
    {
        io->queueRead(report);
        const auto lost = session.controlLost();
        EXPECT_TRUE(lost.has_value());
        EXPECT_EQ(io->written().size(), written) << "controlLost() não pode escrever";
        return lost.value_or(false);
    }

    std::size_t written = 0;
};

TEST_F(G915HidppSessionLostControlTest, NothingPendingMeansControlKept)
{
    beginLive();

    const auto lost = session.controlLost();

    ASSERT_TRUE(lost.has_value());
    EXPECT_FALSE(*lost);
    EXPECT_EQ(io->written().size(), written);
}

TEST_F(G915HidppSessionLostControlTest, ReceiverLinkNoticeFromTheKeyboardMeansLost)
{
    beginLive();

    EXPECT_TRUE(lostAfter(Frame{0x10, 0x01, 0x41, 0x11, 0x71, 0x7c, 0x40})); // repouso (link perdido)
}

TEST_F(G915HidppSessionLostControlTest, ReceiverLinkNoticeWithOtherFlagsAlsoMeansLost)
{
    beginLive();

    EXPECT_TRUE(lostAfter(Frame{0x10, 0x01, 0x41, 0x11, 0xb1, 0x7c, 0x40})); // despertar (link restabelecido)
}

TEST_F(G915HidppSessionLostControlTest, ReconnectEventOfFeature1D4BMeansLost)
{
    beginLive();

    EXPECT_TRUE(lostAfter(longReport(0x01, 0x04, 0x00, {0x01, 0x01, 0x01})));
}

TEST_F(G915HidppSessionLostControlTest, ReceiverNoticeFromAnotherDeviceIsIgnored)
{
    beginLive();

    EXPECT_FALSE(lostAfter(Frame{0x10, 0x02, 0x41, 0x11, 0x71, 0x7c, 0x40}));
}

TEST_F(G915HidppSessionLostControlTest, BatteryEventIsIgnored)
{
    beginLive();

    EXPECT_FALSE(lostAfter(longReport(0x01, 0x07, 0x00, {0x10, 0x2e, 0x00})));
}

TEST_F(G915HidppSessionLostControlTest, OtherFunctionOfFeature1D4BIsIgnored)
{
    beginLive();

    EXPECT_FALSE(lostAfter(longReport(0x01, 0x04, 0x1B, {0x01, 0x01, 0x01})));
}

TEST_F(G915HidppSessionLostControlTest, WithoutFeature1D4BOnlyTheReceiverNoticeCounts)
{
    features = {0x1004, 0x8081, 0x0001, 0x8071, 0x8070};
    beginLive();

    EXPECT_FALSE(lostAfter(longReport(0x01, 0x04, 0x00, {0x01, 0x01, 0x01})));
    EXPECT_TRUE(lostAfter(Frame{0x10, 0x01, 0x41, 0x11, 0x71, 0x7c, 0x40}));
}

TEST_F(G915HidppSessionLostControlTest, NoticeDuringACallAlsoMarksTheLoss)
{
    beginLive();
    io->queueRead(Frame{0x10, 0x01, 0x41, 0x11, 0x71, 0x7c, 0x40});
    ASSERT_TRUE(session.paint(colorsFor({"Key: A"}, RgbColor{1, 2, 3})).has_value());
    written = io->written().size();

    const auto lost = session.controlLost();

    ASSERT_TRUE(lost.has_value());
    EXPECT_TRUE(*lost);
}

TEST_F(G915HidppSessionTest, EndLiveGivesControlBackToTheFirmware)
{
    ASSERT_TRUE(session.endLive().has_value());

    EXPECT_EQ(featureFrames(), (std::vector<Frame>{longReport(device, indexOf(0x8071), fn(5), {0x01, 0x00, 0x00})}));
}

TEST_F(G915HidppSessionTest, PaintSendsTheFramesThenCommits)
{
    const auto painted = session.paint(colorsFor({"Key: A"}, RgbColor{0x10, 0x20, 0x30}));

    ASSERT_TRUE(painted.has_value()) << painted.error().detail;
    const auto perKey = indexOf(0x8081);
    EXPECT_EQ(featureFrames(), (std::vector<Frame>{longReport(device, perKey, fn(1), {0x01, 0x10, 0x20, 0x30, 0xFF}),
                                                   longReport(device, perKey, fn(7))}));
}

TEST_F(G915HidppSessionTest, EmptyMapSendsNothing)
{
    ASSERT_TRUE(session.paint(KeyColorMap{}).has_value());

    EXPECT_TRUE(io->written().empty());
}

TEST_F(G915HidppSessionTest, DeviceErrorStopsThePaintWithoutSendingTheRest)
{
    // Seis cores distintas -> dois quadros pequenos + commit. O primeiro quadro recebe erro HID++ 2.0.
    KeyColorMap colors;
    std::uint8_t shade = 1;
    for (const auto* name : {"Key: A", "Key: B", "Key: C", "Key: D", "Key: E", "Key: F"}) {
        colors = colors.with(*G915KeyCatalog::findByName(name), RgbColor{shade++, 0, 0}); // NOLINT
    }
    io->setResponder([this](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == indexOf(0x8081)) {
            return {longReport(frame[1], 0xFF, frame[2], {frame[3], 0x02})};
        }
        return respond(frame);
    });

    const auto painted = session.paint(colors);

    ASSERT_FALSE(painted.has_value());
    EXPECT_EQ(painted.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(featureFrames().size(), 1U);
}

TEST_F(G915HidppSessionTest, MissingResponseIsAnErrorAndNothingIsResent)
{
    io->setResponder([this](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == indexOf(0x8071)) {
            return {};
        }
        return respond(frame);
    });

    const auto begun = session.beginLive();

    ASSERT_FALSE(begun.has_value());
    EXPECT_EQ(begun.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(featureFrames().size(), 1U);
}

TEST_F(G915HidppSessionTest, KeyboardWithoutPerKeyFeatureIsUnavailableWithoutWriting)
{
    features = {0x0001, 0x8071};

    const auto painted = session.paint(colorsFor({"Key: A"}, RgbColor{1, 2, 3}));

    ASSERT_FALSE(painted.has_value());
    EXPECT_EQ(painted.error().code, ErrorCode::BackendUnavailable);
    EXPECT_TRUE(featureFrames().empty());
}

// Efeitos de hardware (INC-008 parte D): GetInfo simulado igual ao da unidade real (evidência de 2026-10-14).
class G915HidppSaveEffectTest : public G915HidppSessionTest {
protected:
    G915HidppSaveEffectTest()
    {
        locations = {{0, 0x0002}, {1, 0x0001}};
        ids = {{0, {0x0000, 0x0001, 0x0003, 0x000A}}, {1, {0x0000, 0x0001, 0x000A, 0x0003, 0x0004, 0x000B, 0x000C}}};
        install();
    }

    void install()
    {
        io->setResponder([this](const Frame& frame) -> std::vector<Frame> {
            if (indexOf(0x8071) == 0 || frame[2] != indexOf(0x8071)) {
                return respond(frame);
            }
            const std::uint8_t function = frame[3] >> 4;
            const auto header = static_cast<std::uint8_t>((function << 4) | (frame[3] & 0x0F));
            if (function == 0) {
                const std::uint8_t cluster = frame[4];
                const std::uint8_t which = frame[5];
                const auto& list = ids.at(cluster);
                if (which == 0xFF) {
                    const auto loc = locations.at(cluster);
                    return {
                        longReport(frame[1], frame[2], header,
                                   {cluster, 0x00, static_cast<std::uint8_t>(loc >> 8),
                                    static_cast<std::uint8_t>(loc & 0xFF), static_cast<std::uint8_t>(list.size())})};
                }
                const auto id = list.at(which);
                return {longReport(
                    frame[1], frame[2], header,
                    {cluster, which, static_cast<std::uint8_t>(id >> 8), static_cast<std::uint8_t>(id & 0xFF)})};
            }
            return fn1Responder ? fn1Responder(frame) : respond(frame);
        });
    }

    [[nodiscard]] std::vector<Frame> framesOfFunction(std::uint8_t function) const
    {
        std::vector<Frame> frames;
        for (const auto& frame : featureFrames()) {
            if (frame[2] == indexOf(0x8071) && (frame[3] >> 4) == function) {
                frames.push_back(frame);
            }
        }
        return frames;
    }

    static KeyboardEffect wave()
    {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return KeyboardEffect::wave(*EffectPeriod::create(5000), *EffectBrightness::create(100));
    }

    std::map<std::uint8_t, std::uint16_t> locations;
    std::map<std::uint8_t, std::vector<std::uint16_t>> ids;
    std::function<std::vector<Frame>(const Frame&)> fn1Responder;
};

TEST_F(G915HidppSaveEffectTest, WaveOnTheKeysSendsFn1WithTheIndexFromGetInfoAndPersist)
{
    const auto saved = session.setEffect(KeyboardZone::Keys, wave());

    ASSERT_TRUE(saved.has_value()) << saved.error().detail;
    EXPECT_EQ(framesOfFunction(1),
              (std::vector<Frame>{longReport(device, indexOf(0x8071), fn(1),
                                             {0x01, 0x04, 0, 0, 0, 0, 0, 0, 0x88, 0x01, 0x64, 0x13, 0x01})}));
}

TEST_F(G915HidppSaveEffectTest, TheIndexOfTheSameEffectDiffersPerClusterOnTheLogo)
{
    const auto saved =
        session.setEffect(KeyboardZone::Logo, KeyboardEffect::cycle(*EffectPeriod::create(2000),     // NOLINT
                                                                    *EffectBrightness::create(80))); // NOLINT

    ASSERT_TRUE(saved.has_value()) << saved.error().detail;
    const auto sent = framesOfFunction(1);
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(sent.front()[4], 0x00); // cluster do logo
    EXPECT_EQ(sent.front()[5], 0x02); // Cycle (0x0003) é o efeito 2 no logo
}

TEST_F(G915HidppSaveEffectTest, WaveOnTheLogoIsInvalidAndSendsNoFn1)
{
    const auto saved = session.setEffect(KeyboardZone::Logo, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(framesOfFunction(1).empty());
}

TEST_F(G915HidppSaveEffectTest, DeviceErrorOnFn1IsReturnedAndNothingIsResent)
{
    fn1Responder = [](const Frame& frame) -> std::vector<Frame> {
        return {longReport(frame[1], 0xFF, frame[2], {frame[3], 0x02})};
    };

    const auto saved = session.setEffect(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(framesOfFunction(1).size(), 1U);
}

TEST_F(G915HidppSaveEffectTest, MissingResponseTimesOutWithoutResending)
{
    fn1Responder = [](const Frame&) -> std::vector<Frame> { return {}; };

    const auto saved = session.setEffect(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(framesOfFunction(1).size(), 1U);
}

TEST_F(G915HidppSaveEffectTest, ReplyWithAnotherSoftwareIdIsIgnored)
{
    fn1Responder = [](const Frame& frame) -> std::vector<Frame> {
        return {longReport(frame[1], frame[2], static_cast<std::uint8_t>((1 << 4) | 0x05), {})};
    };

    const auto saved = session.setEffect(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(framesOfFunction(1).size(), 1U);
}

TEST_F(G915HidppSaveEffectTest, UnexpectedClusterLocationWritesNothing)
{
    locations[1] = 0x0002; // o "teclado" diz ser o logo

    const auto saved = session.setEffect(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(framesOfFunction(1).empty());
}

TEST_F(G915HidppSaveEffectTest, GetInfoIsReadOncePerSession)
{
    ASSERT_TRUE(session.setEffect(KeyboardZone::Keys, wave()).has_value());
    const auto readsAfterFirst = framesOfFunction(0).size();
    EXPECT_GT(readsAfterFirst, 0U);

    ASSERT_TRUE(session.setEffect(KeyboardZone::Keys, wave()).has_value());

    EXPECT_EQ(framesOfFunction(0).size(), readsAfterFirst);
    EXPECT_EQ(framesOfFunction(1).size(), 2U);
}

TEST_F(G915HidppSaveEffectTest, ErrorWhileReadingGetInfoWritesNothing)
{
    io->setResponder([this](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == indexOf(0x8071)) {
            return {longReport(frame[1], 0xFF, frame[2], {frame[3], 0x05})};
        }
        return respond(frame);
    });

    const auto saved = session.setEffect(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_TRUE(framesOfFunction(1).empty());
}

TEST_F(G915HidppSaveEffectTest, KeyboardWithout8071IsUnavailable)
{
    features = {0x0001, 0x8081};

    const auto saved = session.setEffect(KeyboardZone::Keys, wave());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::BackendUnavailable);
}

} // namespace

// INC-019 R1: o brilho é o último parâmetro do efeito (captura C-04, 2026-10-15 18:23:42 e 18:23:47). O período de
// 2100 ms é o que o G HUB usava na captura (0x0834).
TEST_F(G915HidppSaveEffectTest, BrightnessFortyNineAndOneHundredMatchTheCaptureBytesOnKeysAndLogo)
{
    struct Case {
        std::uint32_t percent;
        std::uint8_t byte;
    };
    std::vector<Frame> expected;
    for (const auto& [percent, byte] : {Case{49, 0x31}, Case{100, 0x64}}) {
        const auto wave =
            KeyboardEffect::wave(*EffectPeriod::create(2100), *EffectBrightness::create(percent)); // NOLINT
        const auto cycle =
            KeyboardEffect::cycle(*EffectPeriod::create(2100), *EffectBrightness::create(percent)); // NOLINT
        ASSERT_TRUE(session.setEffect(KeyboardZone::Keys, wave).has_value());
        ASSERT_TRUE(session.setEffect(KeyboardZone::Logo, cycle).has_value());
        // Teclas (cluster 1, efeito 4): 01 04 00*6 34 01 <brilho> 08 01. Logo (cluster 0, efeito 2): 00 02 00*5 08 34
        // <brilho> 00 00 01.
        expected.push_back(
            longReport(device, indexOf(0x8071), fn(1), {0x01, 0x04, 0, 0, 0, 0, 0, 0, 0x34, 0x01, byte, 0x08, 0x01}));
        expected.push_back(
            longReport(device, indexOf(0x8071), fn(1), {0x00, 0x02, 0, 0, 0, 0, 0, 0x08, 0x34, byte, 0, 0, 0x01}));
    }

    EXPECT_EQ(framesOfFunction(1), expected);
}
