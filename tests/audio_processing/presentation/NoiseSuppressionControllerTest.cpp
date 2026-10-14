#include "modules/audio_processing/presentation/NoiseSuppressionController.hpp"
#include "fakes/FakeNoiseSuppressionGraph.hpp"
#include "modules/audio_processing/application/HostNoiseSuppression.hpp"

#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

using ghub::audio_processing::HostNoiseSuppression;
using ghub::audio_processing::NoiseSuppressionController;
using ghub::audio_processing::NoiseSuppressionGraph;
using ghub::audio_processing::NoiseSuppressionObserver;
using ghub::audio_processing::VoiceThreshold;
using ghub::fakes::FakeNoiseSuppressionGraph;
using Op = FakeNoiseSuppressionGraph::Op;

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

// Delega ao fake e, se programado, bloqueia dentro de uma operação até o teste liberar a porta.
class GatedGraph final : public NoiseSuppressionGraph {
public:
    FakeNoiseSuppressionGraph inner;

    void gateOn(Op op, Gate& gate)
    {
        gatedOp_ = op;
        gate_ = &gate;
    }

    ghub::shared_kernel::Result<std::string> findHeadsetSource() override
    {
        block(Op::FindSource);
        return inner.findHeadsetSource();
    }
    ghub::shared_kernel::Result<void> createNoiseSuppression(VoiceThreshold threshold,
                                                             const std::string& source) override
    {
        block(Op::Create);
        return inner.createNoiseSuppression(threshold, source);
    }
    ghub::shared_kernel::Result<void> updateNoiseSuppression(VoiceThreshold threshold) override
    {
        block(Op::Update);
        return inner.updateNoiseSuppression(threshold);
    }
    ghub::shared_kernel::Result<void> removeNoiseSuppression() override
    {
        block(Op::Remove);
        return inner.removeNoiseSuppression();
    }
    std::string noiseSuppressionSourceName() const override { return inner.noiseSuppressionSourceName(); }
    ghub::shared_kernel::Result<std::optional<std::string>> configuredDefaultSource() override
    {
        return inner.configuredDefaultSource();
    }
    ghub::shared_kernel::Result<void> setConfiguredDefaultSource(const std::optional<std::string>& name) override
    {
        return inner.setConfiguredDefaultSource(name);
    }
    void setNoiseSuppressionObserver(NoiseSuppressionObserver* observer) override
    {
        inner.setNoiseSuppressionObserver(observer);
    }

private:
    void block(Op op)
    {
        if (gate_ != nullptr && op == gatedOp_) {
            gate_->wait();
        }
    }

    Op gatedOp_ = Op::FindSource;
    Gate* gate_ = nullptr;
};

class NoiseSuppressionControllerTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fixture = std::make_unique<Fixture>(); }
    void cleanup() { fixture.reset(); }

    void construction_CallsNothingInTheGraphAndStartsAtStandard()
    {
        const NoiseSuppressionController controller{fixture->useCase, fixture->graph};

        QVERIFY(fixture->graph.inner.calls.empty());
        QVERIFY(!controller.busy());
        QVERIFY(!controller.enabled());
        QVERIFY(controller.status().isEmpty());
        QCOMPARE(controller.threshold(), 50);
        QCOMPARE(controller.minimumThreshold(), 0);
        QCOMPARE(controller.maximumThreshold(), 99);
    }

    void construction_RegistersItselfAsTheGraphObserver()
    {
        const NoiseSuppressionController controller{fixture->useCase, fixture->graph};

        QVERIFY(fixture->graph.inner.observer != nullptr);
    }

    void enable_TurnsOnInTheGraphAndPublishesTheResult()
    {
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        QSignalSpy changed(&controller, &NoiseSuppressionController::stateChanged);

        controller.enable();
        QVERIFY(controller.busy());
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.enabled());
        QVERIFY(fixture->graph.inner.suppressionExists);
        QCOMPARE(controller.status(),
                 QStringLiteral("Redução de ruído ligada: entrada padrão = G733 + Redução de ruído"));
        QVERIFY(changed.count() >= 2);
    }

    void disable_RestoresTheDefaultInputAndPublishesTheResult()
    {
        fixture->graph.inner.configuredDefault = "entrada.anterior";
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());

        controller.disable();
        QTRY_VERIFY(!controller.enabled() && !controller.busy());

        QVERIFY(!fixture->graph.inner.suppressionExists);
        QCOMPARE(fixture->graph.inner.configuredDefault, (std::optional<std::string>{"entrada.anterior"}));
        QCOMPARE(controller.status(), QStringLiteral("Redução de ruído desligada"));
    }

    void enable_Failure_ReportsDetailAndStaysOff()
    {
        fixture->graph.inner.failOn(Op::FindSource, "G733 não encontrado");
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};

        controller.enable();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.enabled());
        QCOMPARE(controller.status(), QStringLiteral("Falha: G733 não encontrado"));
    }

    void setThreshold_WhileEnabled_ShowsTheValueAtOnceAndUpdatesTheGraph()
    {
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());

        controller.setThreshold(70);
        QCOMPARE(controller.threshold(), 70); // antes de a operação terminar
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->graph.inner.suppressionThreshold->percent(), 70);
        QCOMPARE(fixture->graph.inner.ops().back(), Op::Update);
        QCOMPARE(controller.threshold(), 70);
        QVERIFY(controller.enabled());
        QCOMPARE(controller.status(), QStringLiteral("Sensibilidade aplicada"));
    }

    void setThreshold_WhileDisabled_OnlyKeepsTheValue()
    {
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};

        controller.setThreshold(20);
        QTRY_VERIFY(!controller.busy() && fixture->useCase.threshold().percent() == 20);

        QVERIFY(fixture->graph.inner.calls.empty());
        QVERIFY(!controller.enabled());
        QCOMPARE(controller.status(), QStringLiteral("Sensibilidade guardada; vale ao ligar"));
    }

    void setThreshold_Invalid_ReportsFailureAndRunsNothing()
    {
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};

        for (const int invalid : {-1, 100, 1000}) {
            controller.setThreshold(invalid);
            QVERIFY2(!controller.busy(), "nada deve rodar");
            QVERIFY(controller.status().startsWith(QStringLiteral("Falha: ")));
            QCOMPARE(controller.threshold(), 50);
        }
        QVERIFY(fixture->graph.inner.calls.empty());
    }

    void setThreshold_FailureInTheGraph_RevertsAndExplains()
    {
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.inner.failOn(Op::Update, "nó sumiu");

        controller.setThreshold(10);
        QCOMPARE(controller.threshold(), 10);
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.threshold(), 50);
        QCOMPARE(controller.status(), QStringLiteral("Falha: nó sumiu"));
        QVERIFY(controller.enabled());
    }

    void thresholdRequestsDuringAnOperation_OnlyTheLastOneIsApplied()
    {
        Gate gate;
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Update, gate);

        controller.setThreshold(10);
        QTRY_VERIFY(gate.entered());
        controller.setThreshold(20);
        controller.setThreshold(30);
        QCOMPARE(controller.threshold(), 30);
        gate.open();
        QTRY_VERIFY(!controller.busy());

        const auto ops = fixture->graph.inner.ops();
        QCOMPARE(std::count(ops.begin(), ops.end(), Op::Update), 2); // a em andamento e a última
        QCOMPARE(fixture->graph.inner.suppressionThreshold->percent(), 30);
        QCOMPARE(controller.threshold(), 30);
    }

    void pendingDisable_IsNotLostByLaterThresholdChanges()
    {
        Gate gate;
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Update, gate);

        controller.setThreshold(10);
        QTRY_VERIFY(gate.entered());
        controller.disable();
        controller.setThreshold(20);
        gate.open();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.enabled());
        QVERIFY(!fixture->graph.inner.suppressionExists);
        QCOMPARE(fixture->useCase.threshold().percent(), 20);
    }

    void pendingEnable_IsNotLostByLaterThresholdChanges()
    {
        Gate gate;
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Remove, gate);

        controller.disable();
        QTRY_VERIFY(gate.entered());
        controller.enable();
        controller.setThreshold(65);
        gate.open();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.enabled());
        QVERIFY(fixture->graph.inner.suppressionExists);
        QCOMPARE(fixture->graph.inner.suppressionThreshold->percent(), 65);
    }

    void lastTogglePending_Wins()
    {
        Gate gate;
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Remove, gate);

        controller.disable();
        QTRY_VERIFY(gate.entered());
        controller.enable();
        controller.disable();
        gate.open();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.enabled());
        QVERIFY(!fixture->graph.inner.suppressionExists);
    }

    void noiseSuppressionLost_FromAnotherThread_TurnsOffAndShowsTheReason()
    {
        fixture->graph.inner.configuredDefault = "entrada.anterior";
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());

        std::thread{[this] { fixture->graph.inner.observer->noiseSuppressionLost("G733 removido"); }}.join();
        QTRY_VERIFY(!controller.enabled());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.status(), QStringLiteral("Redução de ruído desligada: G733 removido"));
        QCOMPARE(fixture->graph.inner.configuredDefault, (std::optional<std::string>{"entrada.anterior"}));
    }

    void noiseSuppressionLost_WhileAnOperationRuns_IsSerializedAfterIt()
    {
        Gate gate;
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Update, gate);

        controller.setThreshold(40);
        QTRY_VERIFY(gate.entered());
        std::thread{[this] { fixture->graph.inner.observer->noiseSuppressionLost("nó destruído"); }}.join();
        QCoreApplication::processEvents();
        QVERIFY(controller.enabled()); // ainda não rodou: a operação em andamento vem primeiro
        gate.open();
        QTRY_VERIFY(!controller.enabled());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.status(), QStringLiteral("Redução de ruído desligada: nó destruído"));
    }

    void noiseSuppressionLost_WhenAlreadyOff_ChangesNothing()
    {
        NoiseSuppressionController controller{fixture->useCase, fixture->graph};

        std::thread{[this] { fixture->graph.inner.observer->noiseSuppressionLost("ruído"); }}.join();
        QCoreApplication::processEvents();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.status().isEmpty());
        QVERIFY(fixture->graph.inner.calls.empty());
    }

    void destructor_WhileEnabled_DisablesAndRestoresTheDefaultInput()
    {
        fixture->graph.inner.configuredDefault = "entrada.anterior";
        auto controller = std::make_unique<NoiseSuppressionController>(fixture->useCase, fixture->graph);
        controller->enable();
        QTRY_VERIFY(controller->enabled() && !controller->busy());

        controller.reset();

        QVERIFY(!fixture->graph.inner.suppressionExists);
        QCOMPARE(fixture->graph.inner.configuredDefault, (std::optional<std::string>{"entrada.anterior"}));
        QVERIFY(fixture->graph.inner.observer == nullptr);
    }

    void destructor_WhileEnableIsRunning_WaitsThenDisablesAndDropsPending()
    {
        Gate gate;
        auto controller = std::make_unique<NoiseSuppressionController>(fixture->useCase, fixture->graph);
        fixture->graph.gateOn(Op::Create, gate);
        controller->enable();
        QTRY_VERIFY(gate.entered());
        controller->setThreshold(80); // pendente: descartado

        std::thread opener{[&gate] { gate.open(); }};
        controller.reset();
        opener.join();

        QVERIFY(!fixture->graph.inner.suppressionExists);
        QVERIFY(!fixture->useCase.enabled());
        const auto ops = fixture->graph.inner.ops();
        QCOMPARE(std::count(ops.begin(), ops.end(), Op::Update), 0);
    }

    void destructor_WhileDisabled_CallsNothing()
    {
        {
            const NoiseSuppressionController controller{fixture->useCase, fixture->graph};
        }

        QVERIFY(fixture->graph.inner.calls.empty());
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    struct Fixture {
        GatedGraph graph;
        HostNoiseSuppression useCase{graph};
    };

    std::unique_ptr<Fixture> fixture;
};

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
QTEST_GUILESS_MAIN(NoiseSuppressionControllerTest)
#include "NoiseSuppressionControllerTest.moc"
