#include "modules/pointer/infrastructure/BackgroundPointerReader.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakePointerBackend.hpp"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <memory>
#include <semaphore>
#include <thread>
#include <variant>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::PhysicalInterfaceId;
using ghub::pointer::BackgroundPointerReader;
using ghub::pointer::DiscoveredPointer;
using ghub::pointer::PointerConfiguration;
using ghub::pointer::PointerProfile;
using ghub::pointer::ReadPointerConfiguration;
using ghub::pointer::Ready;
using ghub::pointer::Unavailable;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;

namespace {

constexpr int waitMs = 10000;

PointerConfiguration g703Configuration()
{
    const auto profile = PointerProfile::create({
                                                    .index = 0,
                                                    .name = "",
                                                    .active = true,
                                                    .enabled = true,
                                                    .reportRate = 1000,
                                                    .supportedReportRates = {1000},
                                                    .stages = {},
                                                    .buttons = {},
                                                    .leds = {},
                                                })
                             .value();
    return PointerConfiguration::create("Logitech G703 LS", {profile}).value();
}

// Cenário completo (backend, sonda, registro) com a leitura travável pelo teste.
class Scenario {
public:
    explicit Scenario(bool blocking)
        : interfaceId_(PhysicalInterfaceId::create("ratbag:046d:4086").value()),
          useCase_(backend_, probe_, registry_, interfaceId_)
    {
        std::vector<DiscoveredPointer> devices;
        devices.push_back({
            .usbId = ghub::device_inventory::UsbId{.vendorId = 0x046D, .productId = 0x4086},
            .name = "Logitech G703 LS",
            .configuration = g703Configuration(),
        });
        backend_.setResult(std::move(devices));
        backend_.setOnRead([this, blocking] {
            active_.fetch_add(1);
            const int now = active_.load();
            int seen = maxActive_.load();
            while (now > seen && !maxActive_.compare_exchange_weak(seen, now)) {
            }
            started_.release();
            if (blocking) {
                release_.acquire();
            }
            active_.fetch_sub(1);
            finished_.store(true);
        });
    }

    [[nodiscard]] const ReadPointerConfiguration& useCase() const { return useCase_; }
    [[nodiscard]] bool waitStarted() { return started_.try_acquire_for(10s); }
    void letOneRead() { release_.release(); }
    [[nodiscard]] int calls() const { return backend_.calls(); }
    [[nodiscard]] int maxActive() const { return maxActive_.load(); }
    [[nodiscard]] bool finished() const { return finished_.load(); }
    void failBackend() { backend_.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "sem ratbagd")); }

private:
    ghub::fakes::FakeClock clock_;
    AccessRegistry registry_{clock_};
    ghub::fakes::FakeExternalProcessProbe probe_;
    ghub::fakes::FakePointerBackend backend_;
    PhysicalInterfaceId interfaceId_;
    ReadPointerConfiguration useCase_;
    std::atomic<int> active_{0};
    std::atomic<int> maxActive_{0};
    std::atomic<bool> finished_{false};
    std::counting_semaphore<> started_{0};
    std::counting_semaphore<> release_{0};
};

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class BackgroundPointerReaderTest : public QObject {
    Q_OBJECT

private slots:
    void readsNothing_UntilStarted()
    {
        Scenario scenario{false};
        BackgroundPointerReader reader{scenario.useCase()};

        QVERIFY(!reader.isReading());
        QVERIFY(!reader.lastStatus().has_value());
        QCOMPARE(scenario.calls(), 0);
    }

    void startDoesNotBlock_WhileTheReadIsBlocked()
    {
        Scenario scenario{true};
        BackgroundPointerReader reader{scenario.useCase()};
        QSignalSpy spy(&reader, &BackgroundPointerReader::readFinished);

        reader.start(); // retorna com o backend ainda travado

        QVERIFY(scenario.waitStarted());
        QVERIFY(reader.isReading());
        QVERIFY(!reader.lastStatus().has_value());
        scenario.letOneRead();
        QVERIFY(spy.wait(waitMs));
    }

    void emitsAndExposesTheStatus_WhenTheReadFinishes()
    {
        Scenario scenario{false};
        BackgroundPointerReader reader{scenario.useCase()};
        QSignalSpy spy(&reader, &BackgroundPointerReader::readFinished);

        reader.start();
        QVERIFY(spy.wait(waitMs));

        QVERIFY(!reader.isReading());
        const auto status = reader.lastStatus();
        if (!status.has_value()) {
            QFAIL("a leitura deveria ter resultado");
        }
        const auto* ready = std::get_if<Ready>(&*status);
        QVERIFY(ready != nullptr);
        QCOMPARE(QString::fromStdString(ready->configuration.name()), QStringLiteral("Logitech G703 LS"));
    }

    void exposesAFailureAsUnavailable()
    {
        Scenario scenario{false};
        scenario.failBackend();
        BackgroundPointerReader reader{scenario.useCase()};
        QSignalSpy spy(&reader, &BackgroundPointerReader::readFinished);

        reader.start();
        QVERIFY(spy.wait(waitMs));

        const auto status = reader.lastStatus();
        if (!status.has_value()) {
            QFAIL("a leitura deveria ter resultado");
        }
        const auto* failure = std::get_if<Unavailable>(&*status);
        QVERIFY(failure != nullptr);
        QCOMPARE(failure->error.code, ErrorCode::BackendUnavailable);
    }

    void ignoresStart_WhileReading()
    {
        Scenario scenario{true};
        BackgroundPointerReader reader{scenario.useCase()};
        QSignalSpy spy(&reader, &BackgroundPointerReader::readFinished);
        reader.start();
        QVERIFY(scenario.waitStarted());

        reader.start();
        reader.start();
        scenario.letOneRead();
        QVERIFY(spy.wait(waitMs));

        QCOMPARE(scenario.calls(), 1);
        QCOMPARE(scenario.maxActive(), 1);
        QCOMPARE(spy.count(), 1);
    }

    void startReadsAgain_AfterThePreviousFinished()
    {
        Scenario scenario{false};
        BackgroundPointerReader reader{scenario.useCase()};
        QSignalSpy spy(&reader, &BackgroundPointerReader::readFinished);
        reader.start();
        QVERIFY(spy.wait(waitMs));

        reader.start();
        QVERIFY(spy.wait(waitMs));

        QCOMPARE(scenario.calls(), 2);
    }

    void destructorWaitsForRunningRead()
    {
        Scenario scenario{true};
        auto reader = std::make_unique<BackgroundPointerReader>(scenario.useCase());
        reader->start();
        QVERIFY(scenario.waitStarted());
        std::atomic<bool> destroying{false};
        // Libera o backend de outra thread assim que o teste inicia a destruição. A espera do destrutor é o
        // que impede o retorno antes de a leitura terminar; sem ela, `finished()` seria falso (ou o caso de
        // uso seria usado depois de destruído, o que o ASan acusa).
        std::thread releaser([&scenario, &destroying] {
            destroying.wait(false);
            scenario.letOneRead();
        });

        destroying.store(true);
        destroying.notify_all();
        reader.reset();

        QVERIFY(scenario.finished());
        releaser.join();
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(BackgroundPointerReaderTest)
#include "BackgroundPointerReaderTest.moc"
