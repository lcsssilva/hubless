#include "modules/lighting/presentation/SyncColorController.hpp"

#include <QColor>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>

#include <atomic>
#include <thread>

using ghub::lighting::application::HeadsetColorTarget;
using ghub::lighting::application::KeyboardColorTarget;
using ghub::lighting::application::MouseColorTarget;
using ghub::lighting::application::SyncColor;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::presentation::SyncColorController;
using ghub::shared_kernel::ErrorCode;

namespace {

class Keyboard final : public KeyboardColorTarget {
public:
    [[nodiscard]] bool ready() const override { return live; }
    [[nodiscard]] ghub::shared_kernel::Result<void> applyFixed(const RgbColor& color) override
    {
        ++calls;
        thread = std::this_thread::get_id();
        last = color;
        return {};
    }
    bool live = true;
    int calls = 0;
    std::thread::id thread;
    RgbColor last{0, 0, 0};
};

class Headset final : public HeadsetColorTarget {
public:
    [[nodiscard]] ghub::shared_kernel::Result<void> applyFixed(const RgbColor&) override
    {
        ++calls;
        thread = std::this_thread::get_id();
        if (fail) {
            return ghub::shared_kernel::fail(ErrorCode::DeviceBusy, "headset ocupado");
        }
        return {};
    }
    bool fail = false;
    std::atomic<int> calls = 0;
    std::thread::id thread;
};

class Mouse final : public MouseColorTarget {
public:
    [[nodiscard]] ghub::shared_kernel::Result<void> applyFixed(const RgbColor&) override
    {
        ++calls;
        return {};
    }
    std::atomic<int> calls = 0;
};

class SyncColorControllerTest : public QObject {
    Q_OBJECT

private slots:
    void construction_TouchesNothing()
    {
        Keyboard keyboard;
        Headset headset;
        Mouse mouse;
        const SyncColor useCase{keyboard, headset, mouse};
        SyncColorController controller{useCase};
        QCOMPARE(keyboard.calls, 0);
        QCOMPARE(headset.calls.load(), 0);
        QCOMPARE(mouse.calls.load(), 0);
        QVERIFY(!controller.busy());
        QVERIFY(controller.results().isEmpty());
    }

    void apply_AllDevices_KeyboardOnTheOwnerThread_OthersOffIt_AndListsEachResult()
    {
        Keyboard keyboard;
        Headset headset;
        Mouse mouse;
        const SyncColor useCase{keyboard, headset, mouse};
        SyncColorController controller{useCase};
        QSignalSpy spy{&controller, &SyncColorController::finished};

        controller.apply(QColor(10, 20, 30), true, true, true, true);

        QVERIFY(spy.wait(5000));
        QCOMPARE(keyboard.calls, 1);
        QCOMPARE(keyboard.thread, std::this_thread::get_id());
        QCOMPARE(keyboard.last, RgbColor(10, 20, 30));
        QCOMPARE(headset.calls.load(), 1);
        QVERIFY(headset.thread != std::this_thread::get_id());
        QCOMPARE(mouse.calls.load(), 1);
        QCOMPARE(controller.results().size(), 3);
        QVERIFY(!controller.busy());
    }

    void apply_MouseWithoutConfirmation_IsSkippedAndNotWritten()
    {
        Keyboard keyboard;
        Headset headset;
        Mouse mouse;
        const SyncColor useCase{keyboard, headset, mouse};
        SyncColorController controller{useCase};
        QSignalSpy spy{&controller, &SyncColorController::finished};

        controller.apply(QColor(1, 2, 3), false, false, true, false);

        QVERIFY(spy.wait(5000));
        QCOMPARE(mouse.calls.load(), 0);
        QCOMPARE(controller.results().size(), 1);
        QVERIFY(controller.results().first().contains(QStringLiteral("G703")));
        QVERIFY(controller.results().first().contains(QStringLiteral("pulado")));
    }

    void apply_HeadsetFailure_IsListedAndDoesNotStopTheMouse()
    {
        Keyboard keyboard;
        keyboard.live = false;
        Headset headset;
        headset.fail = true;
        Mouse mouse;
        const SyncColor useCase{keyboard, headset, mouse};
        SyncColorController controller{useCase};
        QSignalSpy spy{&controller, &SyncColorController::finished};

        controller.apply(QColor(1, 2, 3), true, true, true, true);

        QVERIFY(spy.wait(5000));
        QCOMPARE(keyboard.calls, 0);
        QCOMPARE(mouse.calls.load(), 1);
        const QStringList results = controller.results();
        QCOMPARE(results.size(), 3);
        QVERIFY(results.at(0).contains(QStringLiteral("pulado")));
        QVERIFY(results.at(1).contains(QStringLiteral("falhou")));
        QVERIFY(results.at(1).contains(QStringLiteral("headset ocupado")));
        QVERIFY(results.at(2).contains(QStringLiteral("aplicado")));
    }

    void apply_WhileBusy_IsIgnored()
    {
        Keyboard keyboard;
        Headset headset;
        Mouse mouse;
        const SyncColor useCase{keyboard, headset, mouse};
        SyncColorController controller{useCase};
        QSignalSpy spy{&controller, &SyncColorController::finished};

        controller.apply(QColor(1, 2, 3), false, true, false, false);
        QVERIFY(controller.busy());
        controller.apply(QColor(4, 5, 6), false, true, false, false);

        QVERIFY(spy.wait(5000));
        QCOMPARE(headset.calls.load(), 1);
    }

    void apply_NothingSelected_SendsNothingAndSaysSo()
    {
        Keyboard keyboard;
        Headset headset;
        Mouse mouse;
        const SyncColor useCase{keyboard, headset, mouse};
        SyncColorController controller{useCase};

        controller.apply(QColor(1, 2, 3), false, false, false, false);

        QVERIFY(!controller.busy());
        QCOMPARE(headset.calls.load(), 0);
        QVERIFY(!controller.status().isEmpty());
    }

    void apply_InvalidColor_IsRefused()
    {
        Keyboard keyboard;
        Headset headset;
        Mouse mouse;
        const SyncColor useCase{keyboard, headset, mouse};
        SyncColorController controller{useCase};

        controller.apply(QColor(), true, true, true, true);

        QVERIFY(!controller.busy());
        QCOMPARE(keyboard.calls, 0);
        QCOMPARE(headset.calls.load(), 0);
    }
};

} // namespace

QTEST_GUILESS_MAIN(SyncColorControllerTest)
#include "SyncColorControllerTest.moc"
