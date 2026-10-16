#include "modules/lighting/application/SyncColor.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

// INC-021 R5 (ADR-0030): sincronizar uma cor fixa entre G915, G733 e G703. O G703 é persistente e só entra com
// confirmação explícita; falha ou pulo de um dispositivo não impede os outros.
namespace ghub::lighting::application {
namespace {

using domain::RgbColor;
using shared_kernel::ErrorCode;

class FakeKeyboardTarget final : public KeyboardColorTarget {
public:
    explicit FakeKeyboardTarget(std::vector<std::string>& log) : log_(&log) {}
    [[nodiscard]] bool ready() const override { return ready_; }
    [[nodiscard]] shared_kernel::Result<void> applyFixed(const RgbColor& color) override
    {
        log_->push_back("keyboard:" + std::to_string(color.red()) + "," + std::to_string(color.green()) + "," +
                        std::to_string(color.blue()));
        if (failure_) {
            return shared_kernel::fail(ErrorCode::BackendUnavailable, "teclado indisponível");
        }
        return {};
    }
    bool ready_ = true;
    bool failure_ = false;

private:
    std::vector<std::string>* log_;
};

class FakeHeadsetTarget final : public HeadsetColorTarget {
public:
    explicit FakeHeadsetTarget(std::vector<std::string>& log) : log_(&log) {}
    [[nodiscard]] shared_kernel::Result<void> applyFixed(const RgbColor&) override
    {
        log_->emplace_back("headset");
        if (failure_) {
            return shared_kernel::fail(ErrorCode::DeviceBusy, "headset ocupado");
        }
        return {};
    }
    bool failure_ = false;

private:
    std::vector<std::string>* log_;
};

class FakeMouseTarget final : public MouseColorTarget {
public:
    explicit FakeMouseTarget(std::vector<std::string>& log) : log_(&log) {}
    [[nodiscard]] shared_kernel::Result<void> applyFixed(const RgbColor&) override
    {
        log_->emplace_back("mouse");
        if (failure_) {
            return shared_kernel::fail(ErrorCode::MalformedData, "sem resposta");
        }
        return {};
    }
    bool failure_ = false;

private:
    std::vector<std::string>* log_;
};

class SyncColorTest : public ::testing::Test {
protected:
    std::vector<std::string> log;
    FakeKeyboardTarget keyboard{log};
    FakeHeadsetTarget headset{log};
    FakeMouseTarget mouse{log};
    SyncColor sync{keyboard, headset, mouse};

    static SyncRequest all(bool confirmMouse)
    {
        return {.color = RgbColor{10, 20, 30}, .keyboard = true, .headset = true, .mouse = true,
                .confirmPersistentMouse = confirmMouse};
    }
    static const SyncOutcome& of(const std::vector<SyncOutcome>& outcomes, SyncDevice device)
    {
        for (const auto& outcome : outcomes) {
            if (outcome.device == device) {
                return outcome;
            }
        }
        throw std::logic_error("dispositivo ausente");
    }
};

TEST_F(SyncColorTest, AppliesTheSameColorToAllSelectedDevices)
{
    const auto outcomes = sync.apply(all(true));
    ASSERT_EQ(outcomes.size(), 3U);
    for (const auto& outcome : outcomes) {
        EXPECT_EQ(outcome.status, SyncStatus::Applied);
    }
    EXPECT_EQ(log, (std::vector<std::string>{"keyboard:10,20,30", "headset", "mouse"}));
}

TEST_F(SyncColorTest, UnselectedDevicesAreNotTouchedNorListed)
{
    SyncRequest request = all(true);
    request.headset = false;
    request.mouse = false;
    const auto outcomes = sync.apply(request);
    ASSERT_EQ(outcomes.size(), 1U);
    EXPECT_EQ(outcomes.front().device, SyncDevice::Keyboard);
    EXPECT_EQ(log, std::vector<std::string>{"keyboard:10,20,30"});
}

TEST_F(SyncColorTest, MouseWithoutConfirmationIsSkippedWithAWarningAndNothingIsWritten)
{
    const auto outcomes = sync.apply(all(false));
    const auto& mouseOutcome = of(outcomes, SyncDevice::Mouse);
    EXPECT_EQ(mouseOutcome.status, SyncStatus::Skipped);
    EXPECT_FALSE(mouseOutcome.detail.empty());
    EXPECT_EQ(of(outcomes, SyncDevice::Keyboard).status, SyncStatus::Applied);
    EXPECT_EQ(of(outcomes, SyncDevice::Headset).status, SyncStatus::Applied);
    EXPECT_EQ(log, (std::vector<std::string>{"keyboard:10,20,30", "headset"}));
}

TEST_F(SyncColorTest, KeyboardNotLiveIsSkippedWithoutSendingAndTheOthersContinue)
{
    keyboard.ready_ = false;
    const auto outcomes = sync.apply(all(true));
    const auto& keyboardOutcome = of(outcomes, SyncDevice::Keyboard);
    EXPECT_EQ(keyboardOutcome.status, SyncStatus::Skipped);
    EXPECT_FALSE(keyboardOutcome.detail.empty());
    EXPECT_EQ(of(outcomes, SyncDevice::Headset).status, SyncStatus::Applied);
    EXPECT_EQ(of(outcomes, SyncDevice::Mouse).status, SyncStatus::Applied);
    EXPECT_EQ(log, (std::vector<std::string>{"headset", "mouse"}));
}

TEST_F(SyncColorTest, AFailureOnOneDeviceDoesNotStopTheOthers)
{
    headset.failure_ = true;
    const auto outcomes = sync.apply(all(true));
    const auto& headsetOutcome = of(outcomes, SyncDevice::Headset);
    EXPECT_EQ(headsetOutcome.status, SyncStatus::Failed);
    EXPECT_EQ(headsetOutcome.detail, "headset ocupado");
    EXPECT_EQ(of(outcomes, SyncDevice::Keyboard).status, SyncStatus::Applied);
    EXPECT_EQ(of(outcomes, SyncDevice::Mouse).status, SyncStatus::Applied);
    EXPECT_EQ(log.size(), 3U);
}

TEST_F(SyncColorTest, KeyboardAndMouseFailuresAreReportedPerDevice)
{
    keyboard.failure_ = true;
    mouse.failure_ = true;
    const auto outcomes = sync.apply(all(true));
    EXPECT_EQ(of(outcomes, SyncDevice::Keyboard).status, SyncStatus::Failed);
    EXPECT_EQ(of(outcomes, SyncDevice::Mouse).status, SyncStatus::Failed);
    EXPECT_EQ(of(outcomes, SyncDevice::Headset).status, SyncStatus::Applied);
}

TEST_F(SyncColorTest, NothingSelectedSendsNothing)
{
    const auto outcomes = sync.apply({.color = RgbColor{1, 2, 3}, .keyboard = false, .headset = false,
                                      .mouse = false, .confirmPersistentMouse = true});
    EXPECT_TRUE(outcomes.empty());
    EXPECT_TRUE(log.empty());
}

} // namespace
} // namespace ghub::lighting::application
