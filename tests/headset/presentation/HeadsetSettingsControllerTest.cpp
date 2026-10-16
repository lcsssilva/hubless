#include "modules/headset/presentation/HeadsetSettingsController.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeHeadsetSettingsWriter.hpp"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::PhysicalInterfaceId;
using ghub::headset::ChangeHeadsetSetting;
using ghub::headset::HeadsetSettingsController;
using ghub::shared_kernel::ErrorCode;
using ghub::shared_kernel::fail;

namespace {

// Porta que bloqueia a thread de trabalho até o teste liberar (sem sleep).
class Gate {
public:
    void wait()
    {
        entered_.store(true);
        std::unique_lock lock{mutex_};
        condition_.wait(lock, [this] { return open_; });
    }

    void open()
    {
        {
            const std::scoped_lock lock{mutex_};
            open_ = true;
        }
        condition_.notify_all();
    }

    [[nodiscard]] bool entered() const { return entered_.load(); }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool open_ = false;
    std::atomic<bool> entered_{false};
};

class HeadsetSettingsControllerTest : public QObject {
    Q_OBJECT

private slots:
    // Fixture nova a cada teste: os membros de um QObject de teste sobreviveriam entre os slots.
    void init() { fixture = std::make_unique<Fixture>(); }

    void cleanup() { fixture.reset(); }

    void construction_WritesNothingAndStartsEmpty()
    {
        const HeadsetSettingsController controller{fixture->useCase};

        QCOMPARE(fixture->writer.calls(), 0);
        QCOMPARE(fixture->probe.calls(), 0);
        QVERIFY(!controller.busy());
        QVERIFY(controller.status().isEmpty());
        QCOMPARE(controller.lastSidetone(), -1);
        QCOMPARE(controller.lightsState(), QString{});
    }

    void setSidetone_RunsOffTheCallingThread_ThenPublishesTheResult()
    {
        Gate gate;
        fixture->writer.setOnCall([&gate] { gate.wait(); });
        HeadsetSettingsController controller{fixture->useCase};
        QSignalSpy changed(&controller, &HeadsetSettingsController::settingsChanged);

        controller.setSidetone(40); // retorna sem esperar o writer
        QVERIFY(controller.busy());
        QTRY_VERIFY(gate.entered());
        QVERIFY(controller.status().isEmpty());
        gate.open();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->writer.sidetoneValues(), (std::vector<int>{40}));
        QCOMPARE(controller.status(), QStringLiteral("Sidetone aplicado: 40"));
        QCOMPARE(controller.lastSidetone(), 40);
        QVERIFY(changed.count() >= 2);
    }

    void appliedObserver_GetsTheSidetoneOnlyOnSuccess()
    {
        HeadsetSettingsController controller{fixture->useCase};
        std::vector<int> seen;
        controller.setSidetoneObserver([&seen](int level) { seen.push_back(level); });

        controller.setSidetone(70);
        QTRY_VERIFY(!controller.busy() && !controller.status().isEmpty());
        QCOMPARE(seen, (std::vector<int>{70}));

        fixture->writer.setResult(ghub::shared_kernel::fail(ErrorCode::DeviceBusy, "ocupado"));
        controller.setSidetone(10);
        QTRY_VERIFY(!controller.busy() && controller.status().startsWith(QStringLiteral("Falha")));
        QCOMPARE(seen, (std::vector<int>{70}));
    }

    void setLightsOn_ReportsCycleEffect()
    {
        HeadsetSettingsController controller{fixture->useCase};

        controller.setLights(true);
        QTRY_VERIFY(!controller.busy() && !controller.status().isEmpty());

        QCOMPARE(fixture->writer.lightsValues(), (std::vector<bool>{true}));
        QCOMPARE(controller.status(), QStringLiteral("Luzes ligadas (ciclo)"));
        QCOMPARE(controller.lightsState(), QStringLiteral("on"));
        QCOMPARE(controller.lastSidetone(), -1);
    }

    void setLightsOff_ReportsOff()
    {
        HeadsetSettingsController controller{fixture->useCase};

        controller.setLights(false);
        QTRY_VERIFY(!controller.busy() && !controller.status().isEmpty());

        QCOMPARE(fixture->writer.lightsValues(), (std::vector<bool>{false}));
        QCOMPARE(controller.status(), QStringLiteral("Luzes desligadas"));
        QCOMPARE(controller.lightsState(), QStringLiteral("off"));
    }

    void failure_ShowsDetail_AndKeepsTheLastAppliedValues()
    {
        HeadsetSettingsController controller{fixture->useCase};
        controller.setSidetone(20);
        QTRY_COMPARE(controller.lastSidetone(), 20);
        fixture->writer.setResult(fail(ErrorCode::BackendUnavailable, "headset não encontrado"));

        controller.setSidetone(60);
        QTRY_COMPARE(controller.status(), QStringLiteral("Falha: headset não encontrado"));

        QVERIFY(!controller.busy());
        QCOMPARE(controller.lastSidetone(), 20);
        QCOMPARE(controller.lightsState(), QString{});
    }

    void failure_DoesNotRetry()
    {
        fixture->writer.setResult(fail(ErrorCode::MalformedData, "json inválido"));
        HeadsetSettingsController controller{fixture->useCase};

        controller.setLights(true);
        QTRY_COMPARE(controller.status(), QStringLiteral("Falha: json inválido"));
        QCoreApplication::processEvents();

        QCOMPARE(fixture->writer.calls(), 1);
        QCOMPARE(controller.lightsState(), QString{});
    }

    void externalOwner_IsReportedAsBusy_WithoutCallingTheWriter()
    {
        fixture->probe.setResult(std::vector<ExternalOwner>{ExternalOwner::OpenRgb});
        HeadsetSettingsController controller{fixture->useCase};

        controller.setSidetone(30);
        QTRY_COMPARE(controller.status(), QStringLiteral("Falha: em uso por openrgb"));

        QCOMPARE(fixture->writer.calls(), 0);
    }

    void invalidSidetone_FailsWithoutRunning()
    {
        HeadsetSettingsController controller{fixture->useCase};
        QSignalSpy changed(&controller, &HeadsetSettingsController::settingsChanged);

        controller.setSidetone(101);

        QVERIFY(!controller.busy());
        QVERIFY(controller.status().startsWith(QStringLiteral("Falha: ")));
        QVERIFY(changed.count() >= 1);
        controller.setSidetone(-1);
        QVERIFY(controller.status().startsWith(QStringLiteral("Falha: ")));
        QCoreApplication::processEvents();
        QCOMPARE(fixture->writer.calls(), 0);
        QCOMPARE(fixture->probe.calls(), 0);
        QCOMPARE(controller.lastSidetone(), -1);
    }

    void requestsDuringOperation_AreCoalescedIntoTheLastOne()
    {
        Gate gate;
        fixture->writer.setOnCall([&gate] { gate.wait(); });
        HeadsetSettingsController controller{fixture->useCase};

        controller.setSidetone(10);
        QTRY_VERIFY(gate.entered());
        controller.setSidetone(20);
        controller.setLights(true); // substitui o pendente anterior
        controller.setSidetone(30);
        QVERIFY(controller.busy());
        gate.open();
        QTRY_COMPARE(controller.lastSidetone(), 30);
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->writer.sidetoneValues(), (std::vector<int>{10, 30}));
        QCOMPARE(fixture->writer.lightsCalls(), 0);
        QCOMPARE(controller.status(), QStringLiteral("Sidetone aplicado: 30"));
    }

    void destroyingDuringOperation_WaitsAndDropsPendingRequest()
    {
        Gate gate;
        fixture->writer.setOnCall([&gate] { gate.wait(); });
        auto controller = std::make_unique<HeadsetSettingsController>(fixture->useCase);

        controller->setSidetone(10);
        QTRY_VERIFY(gate.entered());
        controller->setSidetone(20);
        gate.open();
        controller.reset(); // espera a escrita em andamento; o pendente é descartado

        QCOMPARE(fixture->writer.sidetoneValues(), (std::vector<int>{10}));
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    struct Fixture {
        ghub::fakes::FakeClock clock;
        AccessRegistry registry{clock};
        ghub::fakes::FakeExternalProcessProbe probe;
        ghub::fakes::FakeHeadsetSettingsWriter writer;
        ChangeHeadsetSetting useCase{writer, probe, registry, PhysicalInterfaceId::create("hidpp:046d:0afe").value()};

        Fixture() { probe.setResult(std::vector<ExternalOwner>{}); }
    };

    std::unique_ptr<Fixture> fixture;
};

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): o construtor do teste usa Result::value() em dado constante válido.
QTEST_GUILESS_MAIN(HeadsetSettingsControllerTest)
#include "HeadsetSettingsControllerTest.moc"
