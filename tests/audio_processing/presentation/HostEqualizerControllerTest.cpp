#include "modules/audio_processing/presentation/HostEqualizerController.hpp"
#include "fakes/FakeAudioGraph.hpp"
#include "modules/audio_processing/application/HostEqualizer.hpp"

#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

using ghub::audio_processing::AudioGraph;
using ghub::audio_processing::AudioGraphObserver;
using ghub::audio_processing::EqualizerCurve;
using ghub::audio_processing::HostEqualizer;
using ghub::audio_processing::HostEqualizerController;
using ghub::fakes::FakeAudioGraph;
using Op = FakeAudioGraph::Op;

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

// Delega ao FakeAudioGraph e, se programado, bloqueia dentro de uma operação até o teste liberar a porta.
class GatedGraph final : public AudioGraph {
public:
    FakeAudioGraph inner;

    void gateOn(Op op, Gate& gate)
    {
        gatedOp_ = op;
        gate_ = &gate;
    }

    ghub::shared_kernel::Result<std::string> findHeadsetSink() override
    {
        block(Op::FindSink);
        return inner.findHeadsetSink();
    }
    ghub::shared_kernel::Result<void> createEqualizer(const EqualizerCurve& curve, const std::string& target) override
    {
        block(Op::Create);
        return inner.createEqualizer(curve, target);
    }
    ghub::shared_kernel::Result<void> updateEqualizer(const EqualizerCurve& curve) override
    {
        block(Op::Update);
        return inner.updateEqualizer(curve);
    }
    ghub::shared_kernel::Result<void> removeEqualizer() override
    {
        block(Op::Remove);
        return inner.removeEqualizer();
    }
    std::string equalizerSinkName() const override { return inner.equalizerSinkName(); }
    ghub::shared_kernel::Result<std::optional<std::string>> configuredDefaultSink() override
    {
        return inner.configuredDefaultSink();
    }
    ghub::shared_kernel::Result<void> setConfiguredDefaultSink(const std::optional<std::string>& sink) override
    {
        return inner.setConfiguredDefaultSink(sink);
    }
    void setObserver(AudioGraphObserver* observer) override { inner.setObserver(observer); }

private:
    void block(Op op)
    {
        if (gate_ != nullptr && op == gatedOp_) {
            gate_->wait();
        }
    }

    Op gatedOp_ = Op::FindSink;
    Gate* gate_ = nullptr;
};

class HostEqualizerControllerTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fixture = std::make_unique<Fixture>(); }
    void cleanup() { fixture.reset(); }

    void enable_TurnsOnInTheGraphAndPublishesTheResult()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        QSignalSpy changed(&controller, &HostEqualizerController::stateChanged);

        controller.enable();
        QVERIFY(controller.busy());
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.enabled());
        QVERIFY(fixture->graph.inner.equalizerExists);
        QCOMPARE(controller.status(), QStringLiteral("EQ ligado: saída padrão = G733 + EQ"));
        QVERIFY(changed.count() >= 2);
    }

    void construction_CallsNothingInTheGraphAndStartsFlat()
    {
        const HostEqualizerController controller{fixture->equalizer, fixture->graph};

        QVERIFY(fixture->graph.inner.calls.empty());
        QVERIFY(!controller.busy());
        QVERIFY(!controller.enabled());
        QVERIFY(controller.status().isEmpty());
        QCOMPARE(controller.gains(), (QList<int>(10, 0)));
        QCOMPARE(controller.bandFrequencies(),
                 (QStringList{"32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"}));
        QCOMPARE(controller.minimumGain(), -12);
        QCOMPARE(controller.maximumGain(), 12);
    }

    void disable_RestoresTheDefaultOutputAndPublishesTheResult()
    {
        fixture->graph.inner.configuredDefault = "saida.anterior";
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());

        controller.disable();
        QTRY_VERIFY(!controller.enabled() && !controller.busy());

        QVERIFY(!fixture->graph.inner.equalizerExists);
        QCOMPARE(fixture->graph.inner.configuredDefault, (std::optional<std::string>{"saida.anterior"}));
        QCOMPARE(controller.status(), QStringLiteral("EQ desligado"));
    }

    void setBand_WhileEnabled_ShowsTheGainAtOnceAndUpdatesTheGraph()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());

        controller.setBand(5, 7);
        QCOMPARE(controller.gains()[5], 7); // antes de a operação terminar
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->graph.inner.equalizerCurve->gainsDb()[5], 7);
        QCOMPARE(fixture->graph.inner.ops().back(), Op::Update);
        QCOMPARE(controller.gains()[5], 7);
        QVERIFY(controller.enabled());
    }

    void setBand_WhileDisabled_OnlyKeepsTheCurve()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};

        controller.setBand(0, -3);
        QTRY_VERIFY(!controller.busy() && fixture->equalizer.curve().gainsDb()[0] == -3);

        QVERIFY(fixture->graph.inner.calls.empty());
        QVERIFY(!controller.enabled());
    }

    void setBand_Invalid_ReportsFailureAndRunsNothing()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};

        const std::pair<int, int> invalid[] = {{10, 0}, {-1, 0}, {0, 13}, {0, -13}};
        for (const auto& [band, gain] : invalid) {
            controller.setBand(band, gain);
            QVERIFY2(!controller.busy(), "nada deve rodar");
            QVERIFY(controller.status().startsWith(QStringLiteral("Falha: ")));
            QCOMPARE(controller.gains(), (QList<int>(10, 0)));
        }
        QVERIFY(fixture->graph.inner.calls.empty());
    }

    void flat_ResetsEveryBand()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        controller.setBand(2, 6);
        QTRY_VERIFY(!controller.busy());

        controller.flat();
        QCOMPARE(controller.gains(), (QList<int>(10, 0)));
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->graph.inner.equalizerCurve.value(), EqualizerCurve::flat());
    }

    void setBand_FailureInTheGraph_RevertsGainsAndExplains()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.inner.failOn(Op::Update, "nó sumiu");

        controller.setBand(3, 4);
        QCOMPARE(controller.gains()[3], 4);
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.gains(), (QList<int>(10, 0)));
        QCOMPARE(controller.status(), QStringLiteral("Falha: nó sumiu"));
        QVERIFY(controller.enabled());
    }

    void enable_Failure_ReportsDetailAndStaysOff()
    {
        fixture->graph.inner.failOn(Op::FindSink, "G733 não encontrado");
        HostEqualizerController controller{fixture->equalizer, fixture->graph};

        controller.enable();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.enabled());
        QCOMPARE(controller.status(), QStringLiteral("Falha: G733 não encontrado"));
    }

    void curveRequestsDuringAnOperation_OnlyTheLastOneIsApplied()
    {
        Gate gate;
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Update, gate);

        controller.setBand(0, 1);
        QTRY_VERIFY(gate.entered());
        controller.setBand(1, 2);
        controller.setBand(2, 3);
        QCOMPARE(controller.gains(), (QList<int>{1, 2, 3, 0, 0, 0, 0, 0, 0, 0}));
        gate.open();
        QTRY_VERIFY(!controller.busy());

        const auto ops = fixture->graph.inner.ops();
        const auto updates = std::count(ops.begin(), ops.end(), Op::Update);
        QCOMPARE(updates, 2); // a primeira (em andamento) e a última
        QCOMPARE(fixture->graph.inner.equalizerCurve->gainsDb()[2], 3);
        QCOMPARE(controller.gains(), (QList<int>{1, 2, 3, 0, 0, 0, 0, 0, 0, 0}));
    }

    void pendingDisable_IsNotLostByLaterCurveChanges()
    {
        Gate gate;
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Update, gate);

        controller.setBand(0, 1);
        QTRY_VERIFY(gate.entered());
        controller.disable();
        controller.setBand(1, 2);
        gate.open();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.enabled());
        QVERIFY(!fixture->graph.inner.equalizerExists);
        QCOMPARE(fixture->equalizer.curve().gainsDb()[1], 2);
    }

    void pendingEnable_IsNotLostByLaterCurveChanges()
    {
        Gate gate;
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Remove, gate);

        controller.disable();
        QTRY_VERIFY(gate.entered());
        controller.enable();
        controller.setBand(4, -5);
        gate.open();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.enabled());
        QVERIFY(fixture->graph.inner.equalizerExists);
        QCOMPARE(fixture->graph.inner.equalizerCurve->gainsDb()[4], -5);
    }

    void lastTogglePending_Wins()
    {
        Gate gate;
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
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
        QVERIFY(!fixture->graph.inner.equalizerExists);
    }

    void construction_RegistersItselfAsTheGraphObserver()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};

        QVERIFY(fixture->graph.inner.observer != nullptr);
    }

    void equalizerLost_FromAnotherThread_TurnsOffAndShowsTheReason()
    {
        fixture->graph.inner.configuredDefault = "saida.anterior";
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());

        std::thread{[this] { fixture->graph.inner.observer->equalizerLost("G733 removido"); }}.join();
        QTRY_VERIFY(!controller.enabled());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.status(), QStringLiteral("EQ desligado: G733 removido"));
        QCOMPARE(fixture->graph.inner.configuredDefault, (std::optional<std::string>{"saida.anterior"}));
    }

    void equalizerLost_WhileAnOperationRuns_IsSerializedAfterIt()
    {
        Gate gate;
        HostEqualizerController controller{fixture->equalizer, fixture->graph};
        controller.enable();
        QTRY_VERIFY(controller.enabled() && !controller.busy());
        fixture->graph.gateOn(Op::Update, gate);

        controller.setBand(0, 2);
        QTRY_VERIFY(gate.entered());
        std::thread{[this] { fixture->graph.inner.observer->equalizerLost("nó destruído"); }}.join();
        QCoreApplication::processEvents();
        QVERIFY(controller.enabled()); // ainda não rodou: a operação em andamento vem primeiro
        gate.open();
        QTRY_VERIFY(!controller.enabled());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.status(), QStringLiteral("EQ desligado: nó destruído"));
    }

    void equalizerLost_WhenAlreadyOff_ChangesNothing()
    {
        HostEqualizerController controller{fixture->equalizer, fixture->graph};

        std::thread{[this] { fixture->graph.inner.observer->equalizerLost("ruído"); }}.join();
        QCoreApplication::processEvents();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.status().isEmpty());
        QVERIFY(fixture->graph.inner.calls.empty());
    }

    void destructor_WhileEnabled_DisablesAndRestoresTheDefaultOutput()
    {
        fixture->graph.inner.configuredDefault = "saida.anterior";
        auto controller = std::make_unique<HostEqualizerController>(fixture->equalizer, fixture->graph);
        controller->enable();
        QTRY_VERIFY(controller->enabled() && !controller->busy());

        controller.reset();

        QVERIFY(!fixture->graph.inner.equalizerExists);
        QCOMPARE(fixture->graph.inner.configuredDefault, (std::optional<std::string>{"saida.anterior"}));
        QVERIFY(fixture->graph.inner.observer == nullptr);
    }

    void destructor_WhileEnableIsRunning_WaitsThenDisablesAndDropsPending()
    {
        Gate gate;
        auto controller = std::make_unique<HostEqualizerController>(fixture->equalizer, fixture->graph);
        fixture->graph.gateOn(Op::Create, gate);
        controller->enable();
        QTRY_VERIFY(gate.entered());
        controller->setBand(0, 4); // pendente: descartado

        std::thread opener{[&gate] { gate.open(); }};
        controller.reset();
        opener.join();

        QVERIFY(!fixture->graph.inner.equalizerExists);
        QVERIFY(!fixture->equalizer.enabled());
        const auto ops = fixture->graph.inner.ops();
        QCOMPARE(std::count(ops.begin(), ops.end(), Op::Update), 0);
    }

    void destructor_WhileDisabled_CallsNothing()
    {
        {
            const HostEqualizerController controller{fixture->equalizer, fixture->graph};
        }

        QVERIFY(fixture->graph.inner.calls.empty());
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    struct Fixture {
        GatedGraph graph;
        HostEqualizer equalizer{graph};
    };

    std::unique_ptr<Fixture> fixture;
};

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
QTEST_GUILESS_MAIN(HostEqualizerControllerTest)
#include "HostEqualizerControllerTest.moc"
