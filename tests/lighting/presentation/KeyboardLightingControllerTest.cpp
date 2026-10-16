#include "modules/lighting/presentation/KeyboardLightingController.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeKeyboardLighting.hpp"
#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <QColor>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::PhysicalInterfaceId;
using ghub::lighting::application::StartLiveKeyboardSession;
using ghub::lighting::domain::EffectBrightness;
using ghub::lighting::domain::EffectPeriod;
using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::KeyboardEffect;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::domain::RippleSpeed;
using ghub::lighting::presentation::KeyboardLightingController;
using ghub::shared_kernel::ErrorCode;
using Calls = std::vector<std::string>;

namespace {

PhysicalInterfaceId g915Interface()
{
    return PhysicalInterfaceId::create("hidpp:046d:407c").value(); // NOLINT(bugprone-unchecked-optional-access)
}

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

// Teclado cuja primeira pintura espera a porta e que guarda a cor (do primeiro ponto) de cada pintura.
class GatedKeyboard final : public ghub::lighting::application::KeyboardLighting {
public:
    explicit GatedKeyboard(Gate& gate) : gate_(&gate) {}

    [[nodiscard]] ghub::shared_kernel::Result<std::unique_ptr<ghub::lighting::application::KeyboardLightingSession>>
    open() override
    {
        return std::make_unique<Session>(*this);
    }

    std::vector<RgbColor> painted; // só a thread de trabalho escreve; o teste lê depois do sinal

private:
    class Session final : public ghub::lighting::application::KeyboardLightingSession {
    public:
        explicit Session(GatedKeyboard& owner) : owner_(&owner) {}
        ghub::shared_kernel::Result<void> beginLive() override { return {}; }
        ghub::shared_kernel::Result<void> paint(const ghub::lighting::domain::KeyColorMap& colors) override
        {
            owner_->painted.push_back(colors.begin()->second);
            if (owner_->painted.size() == 1) {
                owner_->gate_->wait();
            }
            return {};
        }
        ghub::shared_kernel::Result<void> setEffect(ghub::lighting::domain::KeyboardZone,
                                                    const ghub::lighting::domain::KeyboardEffect&) override
        {
            return {}; // este teste não grava efeitos
        }
        ghub::shared_kernel::Result<void> endLive() override { return {}; }
        ghub::shared_kernel::Result<bool> controlLost() override { return false; }

    private:
        GatedKeyboard* owner_;
    };

    Gate* gate_;
};

// Teclado cuja primeira consulta de perda do controle espera a porta (sem sleep).
class CheckGatedKeyboard final : public ghub::lighting::application::KeyboardLighting {
public:
    explicit CheckGatedKeyboard(Gate& gate) : gate_(&gate) {}

    [[nodiscard]] ghub::shared_kernel::Result<std::unique_ptr<ghub::lighting::application::KeyboardLightingSession>>
    open() override
    {
        return std::make_unique<Session>(*this);
    }

    std::vector<RgbColor> painted; // só a thread de trabalho escreve; o teste lê depois do sinal

private:
    class Session final : public ghub::lighting::application::KeyboardLightingSession {
    public:
        explicit Session(CheckGatedKeyboard& owner) : owner_(&owner) {}
        ghub::shared_kernel::Result<void> beginLive() override { return {}; }
        ghub::shared_kernel::Result<void> paint(const ghub::lighting::domain::KeyColorMap& colors) override
        {
            owner_->painted.push_back(colors.begin()->second);
            return {};
        }
        ghub::shared_kernel::Result<void> setEffect(ghub::lighting::domain::KeyboardZone,
                                                    const ghub::lighting::domain::KeyboardEffect&) override
        {
            return {};
        }
        ghub::shared_kernel::Result<void> endLive() override { return {}; }
        ghub::shared_kernel::Result<bool> controlLost() override
        {
            if (!owner_->waited_.exchange(true)) {
                owner_->gate_->wait();
            }
            return false;
        }

    private:
        CheckGatedKeyboard* owner_;
    };

    Gate* gate_;
    std::atomic<bool> waited_{false};
};

class KeyboardLightingControllerTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        env = std::make_unique<Env>();
        env->probe.setResult(std::vector<ExternalOwner>{});
    }

    void doesNotTouchAnything_OnConstruction()
    {
        const KeyboardLightingController controller{env->useCase};

        QCOMPARE(env->probe.calls(), 0);
        QVERIFY(env->keyboard.calls().empty());
        QVERIFY(!controller.live());
        QVERIFY(!controller.busy());
        QVERIFY(controller.status().isEmpty());
    }

    void startLive_OpensAndTakesControl()
    {
        KeyboardLightingController controller{env->useCase};
        QSignalSpy changed(&controller, &KeyboardLightingController::controllerChanged);

        controller.startLive();
        QVERIFY(controller.busy());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin"}));
        QVERIFY(controller.live());
        QCOMPARE(controller.status(), QStringLiteral("modo ao vivo ligado: o aplicativo controla as cores"));
        QVERIFY(changed.count() >= 2);
    }

    void startLive_WhenAlreadyLive_DoesNothing()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);

        controller.startLive();

        QVERIFY(!controller.busy());
        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin"}));
    }

    void paintAll_WithoutLive_DoesNotWrite()
    {
        KeyboardLightingController controller{env->useCase};

        controller.paintAll(QColor(255, 0, 0));

        QVERIFY(!controller.busy());
        QVERIFY(env->keyboard.calls().empty());
        QCOMPARE(env->probe.calls(), 0);
        QCOMPARE(controller.status(), QStringLiteral("ligue o modo ao vivo antes de escolher uma cor ou efeito"));
    }

    void paintAll_PaintsEveryCatalogKeyWithTheColor()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);

        paintAndWait(controller, QColor(10, 20, 30));

        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin", "paint"}));
        QCOMPARE(env->keyboard.painted().size(), std::size_t{1});
        const auto& map = env->keyboard.painted().front();
        QCOMPARE(map.size(), G915KeyCatalog::keys().size());
        for (const auto& key : G915KeyCatalog::keys()) {
            QVERIFY(map.colorOf(key.id) == std::optional<RgbColor>{RgbColor(10, 20, 30)});
        }
        QCOMPARE(controller.status(), QStringLiteral("cor aplicada"));
        QVERIFY(controller.live());
    }

    void applyEffect_WithoutLive_DoesNotWrite()
    {
        KeyboardLightingController controller{env->useCase};

        controller.applyEffect(QStringLiteral("wave"), QColor(255, 0, 0));

        QVERIFY(!controller.busy());
        QVERIFY(env->keyboard.calls().empty());
        QCOMPARE(controller.status(), QStringLiteral("ligue o modo ao vivo antes de escolher uma cor ou efeito"));
    }

    void applyEffect_SendsTheFixedParametersPerKind_ToTheKeysAndTheLogoCompanion()
    {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        const auto period = *EffectPeriod::create(5000);
        const auto brightness = *EffectBrightness::create(100);
        const RgbColor red{255, 0, 0};
        const std::vector<std::pair<QString, KeyboardEffect>> expected{
            {QStringLiteral("wave"), KeyboardEffect::wave(period, brightness)},
            {QStringLiteral("cycle"), KeyboardEffect::cycle(period, brightness)},
            {QStringLiteral("breathing"), KeyboardEffect::breathing(red, period, brightness)},
            {QStringLiteral("ripple"), KeyboardEffect::ripple(red, *RippleSpeed::create(20))},
            {QStringLiteral("static"), KeyboardEffect::staticColor(red)},
        };
        // NOLINTEND(bugprone-unchecked-optional-access)
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);

        for (const auto& [kind, effect] : expected) {
            controller.applyEffect(kind, QColor(255, 0, 0));
            QTRY_VERIFY(!controller.busy());
            const auto& saved = env->keyboard.saved();
            QVERIFY(saved.at(saved.size() - 2) == effect);
            QVERIFY(saved.back() == logoCompanion(effect));
        }

        QCOMPARE(env->keyboard.saved().size(), expected.size() * 2);
        QCOMPARE(env->keyboard.calls().at(2), std::string{"save:keys:4"});
        QCOMPARE(env->keyboard.calls().at(3), std::string{"save:logo:3"});
        QCOMPARE(env->keyboard.calls().back(), std::string{"save:logo:1"});
        QCOMPARE(controller.status(), QStringLiteral("efeito aplicado (prévia: some ao desligar o modo ao vivo)"));
        QVERIFY(controller.live());
    }

    void applyEffect_Wave_SendsWaveToKeysAndCycleWithSamePeriodAndBrightnessToLogo()
    {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        const auto period = *EffectPeriod::create(5000);
        const auto brightness = *EffectBrightness::create(100);
        // NOLINTEND(bugprone-unchecked-optional-access)
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);

        controller.applyEffect(QStringLiteral("wave"), QColor(1, 2, 3));
        QTRY_VERIFY(!controller.busy());

        using Zone = ghub::lighting::domain::KeyboardZone;
        QCOMPARE(env->keyboard.savedZones(), (std::vector<Zone>{Zone::Keys, Zone::Logo}));
        QVERIFY(env->keyboard.saved().front() == KeyboardEffect::wave(period, brightness));
        QVERIFY(env->keyboard.saved().back() == KeyboardEffect::cycle(period, brightness));
    }

    void brightness_DefaultsToOneHundredPercent()
    {
        KeyboardLightingController controller{env->useCase};

        QCOMPARE(controller.brightness(), 100);
    }

    void brightness_IsTheBrightnessParameterOfTheNextEffects_KeysAndLogoCompanion()
    {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        const auto period = *EffectPeriod::create(5000);
        const auto half = *EffectBrightness::create(49); // C-04: 49 % = 0x31
        const RgbColor red{255, 0, 0};
        const std::vector<std::pair<QString, KeyboardEffect>> expected{
            {QStringLiteral("wave"), KeyboardEffect::wave(period, half)},
            {QStringLiteral("cycle"), KeyboardEffect::cycle(period, half)},
            {QStringLiteral("breathing"), KeyboardEffect::breathing(red, period, half)},
        };
        // NOLINTEND(bugprone-unchecked-optional-access)
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);
        controller.setBrightness(49);
        QCOMPARE(controller.brightness(), 49);

        for (const auto& [kind, effect] : expected) {
            controller.applyEffect(kind, QColor(255, 0, 0));
            QTRY_VERIFY(!controller.busy());
            const auto& saved = env->keyboard.saved();
            QVERIFY(saved.at(saved.size() - 2) == effect);
            QVERIFY(saved.back() == logoCompanion(effect));
        }
    }

    void brightness_OutOfRange_IsClampedToOneToOneHundred()
    {
        KeyboardLightingController controller{env->useCase};

        controller.setBrightness(0);
        QCOMPARE(controller.brightness(), 1);
        controller.setBrightness(250);
        QCOMPARE(controller.brightness(), 100);
        controller.setBrightness(-5);
        QCOMPARE(controller.brightness(), 1);
    }

    void brightness_ChangingItWritesNothing()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);
        const auto before = env->keyboard.calls().size();

        controller.setBrightness(30);

        QCOMPARE(env->keyboard.calls().size(), before);
    }

    void applyEffect_Ripple_SendsStaticWithTheSameColorToLogo()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);

        controller.applyEffect(QStringLiteral("ripple"), QColor(10, 20, 30));
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->keyboard.saved().size(), std::size_t{2});
        QVERIFY(env->keyboard.saved().back() == KeyboardEffect::staticColor(RgbColor{10, 20, 30}));
        QCOMPARE(env->keyboard.calls().at(3), std::string{"save:logo:1"});
    }

    void applyEffect_KeysFailure_DoesNotSendTheLogo()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);
        env->keyboard.failSave({.code = ErrorCode::MalformedData, .detail = "resposta ruim"});

        controller.applyEffect(QStringLiteral("cycle"), QColor(1, 2, 3));
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin", "save:keys:3", "end"}));
        QVERIFY(!controller.live());
    }

    void applyEffect_UnknownKind_ReportsErrorAndSendsNothing()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);

        controller.applyEffect(QStringLiteral("disco"), QColor(255, 0, 0));

        QVERIFY(!controller.busy());
        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin"}));
        QCOMPARE(controller.status(), QStringLiteral("efeito desconhecido"));
        QVERIFY(controller.live());
    }

    void applyEffectFailure_ClosesLive_AndReportsFailure()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);
        env->keyboard.failSave({.code = ErrorCode::MalformedData, .detail = "resposta ruim"});

        controller.applyEffect(QStringLiteral("wave"), QColor(1, 2, 3));
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.live());
        QCOMPARE(controller.status(), QStringLiteral("falha ao falar com o teclado"));
        QCOMPARE(env->keyboard.calls().back(), std::string{"end"});
    }

    void stopLive_ReturnsControlToFirmware()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);

        controller.stopLive();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin", "end"}));
        QVERIFY(!controller.live());
        QCOMPARE(controller.status(), QStringLiteral("modo ao vivo desligado: o efeito salvo no teclado voltou"));
    }

    void stopLive_WithoutSession_DoesNothing()
    {
        KeyboardLightingController controller{env->useCase};

        controller.stopLive();

        QVERIFY(!controller.busy());
        QVERIFY(env->keyboard.calls().empty());
    }

    void externalProcess_MakesItBusy_NamingTheProcess()
    {
        env->probe.setResult(std::vector<ExternalOwner>{ExternalOwner::OpenRgb});
        KeyboardLightingController controller{env->useCase};

        controller.startLive();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.live());
        QVERIFY2(controller.status().startsWith(QStringLiteral("teclado ocupado: ")), qPrintable(controller.status()));
        QVERIFY2(controller.status().contains(QStringLiteral("openrgb")), qPrintable(controller.status()));
        QVERIFY(env->keyboard.calls().empty());
    }

    void unavailableKeyboard_ReportsItWithoutLive()
    {
        env->keyboard.failOpen({.code = ErrorCode::BackendUnavailable, .detail = "sem nó"});
        KeyboardLightingController controller{env->useCase};

        controller.startLive();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.live());
        QCOMPARE(controller.status(), QStringLiteral("teclado indisponível (desligado, dormindo ou sem permissão)"));
    }

    void paintFailure_ClosesLive_AndReportsFailure()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);
        env->keyboard.failPaint({.code = ErrorCode::MalformedData, .detail = "resposta ruim"});

        paintAndWait(controller, QColor(1, 2, 3));

        QVERIFY(!controller.live());
        QCOMPARE(controller.status(), QStringLiteral("falha ao falar com o teclado"));
        // A sessão falha fechou sozinha e devolveu o controle; pintar de novo não escreve.
        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin", "paint", "end"}));
        controller.paintAll(QColor(1, 2, 3));
        QCOMPARE(env->keyboard.calls().size(), std::size_t{4});
    }

    void leaseLost_ReportsAccessLost()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);
        env->keyboard.failPaint({.code = ErrorCode::LeaseRevoked, .detail = {}});

        paintAndWait(controller, QColor(1, 2, 3));

        QVERIFY(!controller.live());
        QCOMPARE(controller.status(), QStringLiteral("modo ao vivo encerrado: acesso ao teclado perdido"));
    }

    void destroyingWithOpenSession_ReturnsControl()
    {
        {
            KeyboardLightingController controller{env->useCase};
            startAndWait(controller);
            QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin"}));
        }

        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin", "end"}));
    }

    void requestsDuringAnOperation_OnlyTheLastOneCounts()
    {
        Gate gate;
        GatedKeyboard gated{gate};
        const StartLiveKeyboardSession gatedUseCase{gated, env->registry, env->probe, g915Interface()};
        KeyboardLightingController controller{gatedUseCase};
        startAndWait(controller);

        controller.paintAll(QColor(1, 1, 1));
        QTRY_VERIFY(gate.entered());
        controller.paintAll(QColor(2, 2, 2));
        controller.paintAll(QColor(3, 3, 3));
        QVERIFY(controller.busy());
        gate.open();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(gated.painted.size(), std::size_t{2});
        QVERIFY(gated.painted[0] == RgbColor(1, 1, 1));
        QVERIFY(gated.painted[1] == RgbColor(3, 3, 3));
        QCOMPARE(controller.status(), QStringLiteral("cor aplicada"));
    }

    void controlLost_DropsLive_WithoutSendingAndExplainsWhy()
    {
        KeyboardLightingController controller{env->useCase, std::chrono::milliseconds{10}};
        startAndWait(controller);

        env->keyboard.setControlLost(true);

        QTRY_VERIFY(!controller.live());
        QCOMPARE(controller.status(), QStringLiteral("o teclado entrou em repouso e retomou o efeito salvo; ligue o "
                                                     "modo ao vivo de novo"));
        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin"}));
    }

    void controlKept_ChecksPeriodically_AndChangesNothing()
    {
        KeyboardLightingController controller{env->useCase, std::chrono::milliseconds{10}};
        startAndWait(controller);
        const auto status = controller.status();

        QTRY_VERIFY(env->keyboard.controlChecks() >= 3);

        QVERIFY(controller.live());
        QVERIFY(!controller.busy());
        QCOMPARE(controller.status(), status);
        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin"}));
    }

    void controlCheckError_KeepsLive()
    {
        KeyboardLightingController controller{env->useCase, std::chrono::milliseconds{10}};
        startAndWait(controller);
        env->keyboard.failControlCheck(true);

        QTRY_VERIFY(env->keyboard.controlChecks() >= 3);

        QVERIFY(controller.live());
        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin"}));
    }

    void userRequestDuringACheck_IsNotDiscarded()
    {
        Gate gate;
        CheckGatedKeyboard gated{gate};
        const StartLiveKeyboardSession gatedUseCase{gated, env->registry, env->probe, g915Interface()};
        KeyboardLightingController controller{gatedUseCase, std::chrono::milliseconds{10}};
        startAndWait(controller);
        QTRY_VERIFY(gate.entered()); // a consulta está em andamento

        controller.paintAll(QColor(7, 8, 9));
        gate.open();

        QTRY_COMPARE(controller.status(), QStringLiteral("cor aplicada"));
        QCOMPARE(gated.painted.size(), std::size_t{1});
        QVERIFY(gated.painted[0] == RgbColor(7, 8, 9));
        QVERIFY(controller.live());
    }

    void releaseForSleep_WithSession_ReturnsControlOnceAndTurnsLiveOff()
    {
        KeyboardLightingController controller{env->useCase};
        startAndWait(controller);
        QSignalSpy changed(&controller, &KeyboardLightingController::controllerChanged);

        controller.releaseForSleep();

        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin", "end"}));
        QVERIFY(!controller.live());
        QVERIFY(!controller.busy());
        QCOMPARE(controller.status(), QStringLiteral("modo ao vivo desligado: o sistema vai suspender"));
        QVERIFY(changed.count() >= 1);

        controller.releaseForSleep();
        QCOMPARE(env->keyboard.calls(), (Calls{"open", "begin", "end"}));
    }

    void releaseForSleep_WithoutSession_DoesNothing()
    {
        KeyboardLightingController controller{env->useCase};

        controller.releaseForSleep();

        QVERIFY(env->keyboard.calls().empty());
        QCOMPARE(env->probe.calls(), 0);
        QVERIFY(!controller.live());
    }

    void releaseForSleep_WaitsForTheRunningOperation_AndDropsThePendingOne()
    {
        Gate gate;
        GatedKeyboard gated{gate};
        const StartLiveKeyboardSession gatedUseCase{gated, env->registry, env->probe, g915Interface()};
        KeyboardLightingController controller{gatedUseCase};
        startAndWait(controller);
        controller.paintAll(QColor(1, 1, 1));
        QTRY_VERIFY(gate.entered());
        controller.paintAll(QColor(2, 2, 2)); // pendente: deve ser descartado
        std::thread opener{[&gate] { gate.open(); }};

        controller.releaseForSleep();
        opener.join();

        QVERIFY(!controller.live());
        QVERIFY(!controller.busy());
        QCOMPARE(gated.painted.size(), std::size_t{1});
        QCOMPARE(controller.status(), QStringLiteral("modo ao vivo desligado: o sistema vai suspender"));
        QCoreApplication::processEvents(); // eventos tardios do watcher não podem mudar o estado
        QVERIFY(!controller.live());
        QCOMPARE(controller.status(), QStringLiteral("modo ao vivo desligado: o sistema vai suspender"));
    }

private: // NOLINT(readability-redundant-access-specifiers): fora de "private slots", não são testes
    static void startAndWait(KeyboardLightingController& controller)
    {
        controller.startLive();
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.live());
    }

    static void paintAndWait(KeyboardLightingController& controller, const QColor& color)
    {
        controller.paintAll(color);
        QTRY_VERIFY(!controller.busy());
    }

    // Estado novo a cada teste: o QObject de teste vive durante todos eles.
    struct Env {
        ghub::fakes::FakeClock clock;
        AccessRegistry registry{clock};
        ghub::fakes::FakeExternalProcessProbe probe;
        ghub::fakes::FakeKeyboardLighting keyboard;
        StartLiveKeyboardSession useCase{keyboard, registry, probe, g915Interface()};
    };
    std::unique_ptr<Env> env;
};

} // namespace

QTEST_GUILESS_MAIN(KeyboardLightingControllerTest)
#include "KeyboardLightingControllerTest.moc"
