#include "modules/profiles/presentation/ProfilesController.hpp"

#include "fakes/FakeProfilePorts.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

using ghub::profiles::ApplicationId;
using ghub::profiles::ApplyProfile;
using ghub::profiles::CameraControlSetting;
using ghub::profiles::EqualizerGains;
using ghub::profiles::Profile;
using ghub::profiles::ProfileBook;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;
using ghub::profiles::ProfilesController;
using ghub::shared_kernel::ErrorCode;
using ghub::shared_kernel::fail;

namespace {

constexpr std::uint32_t brightnessId = 0x00980900;
constexpr std::uint32_t contrastId = 0x00980901;
constexpr std::uint32_t gainId = 0x00980913;

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

// Repositório cuja leitura bloqueia até o teste liberar.
class GatedRepository final : public ghub::profiles::ProfileRepository {
public:
    Gate gate;
    int saveCalls = 0;

    ghub::shared_kernel::Result<ProfileLibrary> load() override
    {
        gate.wait();
        return ProfileLibrary{};
    }

    ghub::shared_kernel::Result<void> save(const ProfileLibrary&) override
    {
        ++saveCalls;
        return {};
    }
};

Profile makeProfile(const char* name, std::vector<CameraControlSetting> camera, bool withEqualizer)
{
    std::optional<EqualizerGains> equalizer;
    if (withEqualizer) {
        equalizer = EqualizerGains::create({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}).value();
    }
    return Profile::create(ProfileName::create(name).value(), std::move(camera), equalizer).value();
}

ProfileLibrary libraryOf(std::vector<Profile> profiles)
{
    return ProfileLibrary::create(std::move(profiles)).value();
}

// Perfil só de câmera (um controle) com aplicativos associados.
Profile makeProfileFor(const char* name, std::uint32_t controlId, std::int64_t value,
                       std::vector<const char*> applications)
{
    std::vector<ApplicationId> ids;
    for (const char* application : applications) {
        ids.push_back(ApplicationId::create(application).value());
    }
    return Profile::create(ProfileName::create(name).value(), {{controlId, "Controle", value}}, std::nullopt,
                           std::move(ids))
        .value();
}

ProfileLibrary switchingLibrary(std::vector<Profile> profiles, bool switching = true)
{
    ProfileLibrary library = libraryOf(std::move(profiles));
    library.setAutomaticSwitching(switching);
    return library;
}

// Câmera cuja aplicação bloqueia até o teste liberar (sem sleep).
class GatedCamera final : public ghub::test::FakeCameraSettingsPort {
public:
    Gate gate;

    ghub::shared_kernel::Result<std::int64_t> apply(std::uint32_t id, std::int64_t value) override
    {
        gate.wait();
        return FakeCameraSettingsPort::apply(id, value);
    }
};

struct Fixture {
    ghub::test::FakeProfileRepository repository;
    ghub::test::FakeCameraSettingsPort camera;
    ghub::test::FakeEqualizerSettingsPort equalizer;
    ghub::test::FakeProfileTransfer transfer;
    ProfileBook book{repository, camera, &transfer};
    ApplyProfile applyProfile{camera, equalizer};
    ghub::test::FakeActiveApplicationSource source;
    ProfilesController controller;

    explicit Fixture(std::chrono::milliseconds settleDelay = std::chrono::milliseconds{20})
        : controller(book, applyProfile, equalizer, source, settleDelay)
    {
    }

    // Espera tudo o que já foi enfileirado para o dono ser processado (os avisos da fonte chegam enfileirados).
    void drainQueue()
    {
        bool reached = false;
        QMetaObject::invokeMethod(&controller, [&reached] { reached = true; }, Qt::QueuedConnection);
        QTRY_VERIFY_WITH_TIMEOUT(reached, 5000);
    }

    // Depois de um aviso: o prazo de foco cumpriu (ou nunca começou) e qualquer aplicação terminou.
    void settle()
    {
        drainQueue();
        auto* timer = controller.findChild<QTimer*>(QStringLiteral("profilesSettleTimer"));
        QVERIFY(timer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(!timer->isActive(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    }

    // Carrega uma biblioteca com a troca ligada e consome o primeiro aviso (a janela ativa ao carregar o script).
    void startSwitching(ProfileLibrary library)
    {
        loadLibrary(std::move(library));
        QCOMPARE(source.startCalls, 1);
        source.emitActivated("janela-inicial");
        drainQueue();
    }

    // Carrega a biblioteca configurada e espera terminar.
    void loadLibrary(ProfileLibrary library)
    {
        repository.loadResult = std::move(library);
        controller.load();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    }
};

class ProfilesControllerTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fixture = std::make_unique<Fixture>(); }
    void cleanup() { fixture.reset(); }

    void constructor_DoesNothing()
    {
        QCOMPARE(fixture->repository.loadCalls, 0);
        QCOMPARE(fixture->repository.saveCalls, 0);
        QVERIFY(!fixture->controller.busy());
        QVERIFY(fixture->controller.profileNames().isEmpty());
        QVERIFY(!fixture->controller.loadFailed());
    }

    void load_PublishesNamesInLibraryOrder()
    {
        fixture->loadLibrary(libraryOf({makeProfile("Zeta", {}, true), makeProfile("Alfa", {}, true)}));

        QCOMPARE(fixture->controller.profileNames(), (QStringList{"Zeta", "Alfa"}));
        QVERIFY(!fixture->controller.loadFailed());
        QVERIFY(fixture->controller.exists("Alfa"));
        QVERIFY(fixture->controller.exists("  Alfa "));
        QVERIFY(!fixture->controller.exists("Beta"));
        QCOMPARE(fixture->camera.applied.size(), size_t{0});
    }

    void load_Failure_SetsLoadFailedAndStatus()
    {
        fixture->repository.loadResult = fail(ErrorCode::MalformedData, "JSON truncado");
        fixture->controller.load();
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QVERIFY(fixture->controller.loadFailed());
        QVERIFY2(fixture->controller.status().contains("JSON truncado"), qPrintable(fixture->controller.status()));
        QVERIFY(fixture->controller.profileNames().isEmpty());
    }

    void saveCurrent_StoresCameraAndTheEqualizerReadOnTheOwnerThread()
    {
        fixture->camera.current = std::vector<CameraControlSetting>{{brightnessId, "Brightness", 128}};
        fixture->equalizer.gains = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

        fixture->controller.saveCurrent("Jogos", false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 1);
        const auto* saved = fixture->repository.lastSaved.profiles().data();
        QCOMPARE(fixture->repository.lastSaved.profiles().size(), size_t{1});
        QCOMPARE(saved->camera().size(), size_t{1});
        QCOMPARE(saved->equalizer()->values(), fixture->equalizer.gains);
        QCOMPARE(fixture->controller.profileNames(), (QStringList{"Jogos"}));
        QVERIFY(fixture->controller.report().contains("EQ: curva incluída"));
        QVERIFY2(fixture->controller.status().contains("Jogos"), qPrintable(fixture->controller.status()));
    }

    void saveCurrent_CameraAbsent_ReportsTheReason()
    {
        fixture->camera.current = fail(ErrorCode::BackendUnavailable, "câmera não detectada");

        fixture->controller.saveCurrent("So EQ", false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QVERIFY(fixture->controller.report().contains("Câmera não incluída: câmera não detectada"));
        QCOMPARE(fixture->repository.saveCalls, 1);
    }

    void saveCurrent_Duplicate_WithoutReplace_IsRefusedAndWritesNothing()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));

        fixture->controller.saveCurrent("A", false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 0);
        QVERIFY2(fixture->controller.status().contains("já existe"), qPrintable(fixture->controller.status()));
        QCOMPARE(fixture->controller.profileNames(), (QStringList{"A"}));
    }

    void saveCurrent_Duplicate_WithReplace_Overwrites()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));
        fixture->camera.current = std::vector<CameraControlSetting>{{brightnessId, "Brightness", 99}};

        fixture->controller.saveCurrent("A", true);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 1);
        QCOMPARE(fixture->repository.lastSaved.profiles().front().camera().front().value, 99);
    }

    void apply_OneControlFails_OthersAreAppliedAndReportListsEachItem()
    {
        fixture->loadLibrary(libraryOf({makeProfile(
            "A", {{brightnessId, "Brightness", 128}, {contrastId, "Contrast", 40}, {gainId, "Gain", 7}}, true)}));
        fixture->camera.applyResults.emplace(contrastId, fail(ErrorCode::DeviceBusy, "câmera em uso"));
        QSignalSpy cameraApplied{&fixture->controller, &ProfilesController::cameraApplied};

        fixture->controller.apply("A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->camera.applied.size(), size_t{3});
        QCOMPARE(fixture->controller.report(),
                 (QStringList{"Brightness: 128 aplicado", "Contrast: falhou — câmera em uso", "Gain: 7 aplicado",
                              "EQ: curva definida (vale com o EQ ligado)"}));
        QCOMPARE(fixture->equalizer.requested.size(), size_t{1});
        QCOMPARE(cameraApplied.count(), 1);
    }

    void apply_EqualizerRefusedByTheOwner_IsReported()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {}, true)}));
        fixture->equalizer.requestResult = fail(ErrorCode::InvalidArgument, "ganho fora da faixa");
        QSignalSpy cameraApplied{&fixture->controller, &ProfilesController::cameraApplied};

        fixture->controller.apply("A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->controller.report(), (QStringList{"EQ: falhou — ganho fora da faixa"}));
        QCOMPARE(cameraApplied.count(), 0);
        QVERIFY(fixture->camera.applied.empty());
    }

    void apply_UnknownName_DoesNothing()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, true)}));

        fixture->controller.apply("Fantasma");

        QVERIFY(!fixture->controller.busy());
        QVERIFY2(fixture->controller.status().contains("não encontrado"), qPrintable(fixture->controller.status()));
        QVERIFY(fixture->camera.applied.empty());
        QVERIFY(fixture->equalizer.requested.empty());
    }

    void request_DuringAnOperation_IsRefusedWithoutQueueing()
    {
        GatedRepository gated;
        ghub::test::FakeCameraSettingsPort camera;
        ghub::test::FakeEqualizerSettingsPort equalizer;
        ProfileBook book{gated, camera};
        ApplyProfile applyProfile{camera, equalizer};
        ghub::test::FakeActiveApplicationSource source;
        ProfilesController controller{book, applyProfile, equalizer, source};

        controller.load();
        QTRY_VERIFY_WITH_TIMEOUT(gated.gate.entered(), 5000);
        QVERIFY(controller.busy());

        QSignalSpy changed{&controller, &ProfilesController::profilesChanged};
        const std::vector<std::function<void()>> requests{
            [&] { controller.saveCurrent("X", false); }, [&] { controller.remove("X"); },
            [&] { controller.setAutomaticSwitching(true); }, [&] { controller.assignLastApplication("X"); },
            [&] { controller.unassignApplication("x"); }};
        for (const auto& request : requests) {
            const auto before = changed.count();
            request();
            QCOMPARE(changed.count(), before + 1); // cada pedido foi respondido com a recusa
            QCOMPARE(controller.status(), QStringLiteral("aguarde a operação em andamento"));
        }

        gated.gate.open();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(gated.saveCalls, 0);
    }

    void remove_DeletesAndPublishesNames()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {}, true), makeProfile("B", {}, true)}));

        fixture->controller.remove("A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 1);
        QCOMPARE(fixture->controller.profileNames(), (QStringList{"B"}));
    }

    void remove_UnknownName_ReportsFailure()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {}, true)}));

        fixture->controller.remove("Z");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 0);
        QVERIFY2(fixture->controller.status().contains("Falha"), qPrintable(fixture->controller.status()));
        QCOMPARE(fixture->controller.profileNames(), (QStringList{"A"}));
    }

    void saveCurrent_AfterLoadFailure_IsRefused()
    {
        fixture->repository.loadResult = fail(ErrorCode::MalformedData, "ilegível");
        fixture->controller.load();
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        fixture->controller.saveCurrent("A", false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 0);
        QVERIFY(fixture->controller.loadFailed());
    }

    void switchingOff_NeverStartsTheSource()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"firefox"})}, false));

        QCOMPARE(fixture->source.startCalls, 0);
        QVERIFY(!fixture->controller.automaticSwitching());
        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("desligada"));
        QCOMPARE(fixture->controller.profileApplications(), (QVariantMap{{"A", QStringList{"firefox"}}}));
    }

    void load_WithSwitchingOn_StartsTheSourceOnce()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"firefox"})}));

        QCOMPARE(fixture->source.startCalls, 1);
        QVERIFY(fixture->controller.automaticSwitching());
        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("ativa"));
        QVERIFY(fixture->camera.applied.empty());
    }

    void load_SourceStartFails_ReportsUnavailableAndKeepsThePreference()
    {
        fixture->source.startResult = fail(ErrorCode::BackendUnavailable, "KWin ausente");
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"firefox"})}));

        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("indisponível: KWin ausente"));
        QVERIFY(fixture->controller.automaticSwitching());
        QCOMPARE(fixture->repository.saveCalls, 0);
    }

    void firstActivationAfterStart_OnlyUpdatesLastApplication()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"firefox"})}));

        fixture->source.emitActivated("firefox");
        fixture->settle();

        QCOMPARE(fixture->controller.lastApplication(), QStringLiteral("firefox"));
        QVERIFY(fixture->camera.applied.empty());
        QVERIFY(fixture->equalizer.requested.empty());
    }

    void activationAfterTheFirst_AppliesTheAssociatedProfileAndSaysSo()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("A", brightnessId, 11, {"firefox"})}));
        QSignalSpy cameraApplied{&fixture->controller, &ProfilesController::cameraApplied};

        fixture->source.emitActivated("firefox");
        QTRY_COMPARE_WITH_TIMEOUT(cameraApplied.count(), 1, 5000);
        fixture->settle();

        QCOMPARE(fixture->camera.applied.size(), size_t{1});
        QCOMPARE(fixture->camera.applied.front().second, 11);
        QCOMPARE(fixture->controller.status(),
                 QStringLiteral("Perfil A aplicado automaticamente (aplicativo firefox)"));
        QCOMPARE(fixture->controller.lastApplication(), QStringLiteral("firefox"));
    }

    void focusMovingFromAToBWithinTheDelay_AppliesOnlyB()
    {
        fixture = std::make_unique<Fixture>(std::chrono::milliseconds{400});
        fixture->startSwitching(switchingLibrary(
            {makeProfileFor("PA", brightnessId, 1, {"app-a"}), makeProfileFor("PB", contrastId, 2, {"app-b"})}));

        fixture->source.emitActivated("app-a");
        fixture->source.emitActivated("app-b");
        fixture->settle();

        QCOMPARE(fixture->camera.applied.size(), size_t{1});
        QCOMPARE(fixture->camera.applied.front().first, contrastId);
    }

    void ownApplication_IsIgnoredAndDoesNotChangeLastApplication()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("PA", brightnessId, 1, {"app-a", "ghub-linux"})}));
        fixture->source.emitActivated("app-b");
        fixture->settle();
        QCOMPARE(fixture->controller.lastApplication(), QStringLiteral("app-b"));

        fixture->source.emitActivated("ghub-linux");
        fixture->settle();

        QCOMPARE(fixture->controller.lastApplication(), QStringLiteral("app-b"));
        QVERIFY(fixture->camera.applied.empty());
    }

    void applicationWithoutRule_AppliesNothing()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("PA", brightnessId, 1, {"app-a"})}));

        fixture->source.emitActivated("sem-regra");
        fixture->settle();

        QVERIFY(fixture->camera.applied.empty());
        QCOMPARE(fixture->controller.lastApplication(), QStringLiteral("sem-regra"));
    }

    void sameProfileAgain_IsNotReapplied()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("PA", brightnessId, 1, {"app-a", "app-b"})}));

        fixture->source.emitActivated("app-a");
        fixture->settle();
        QCOMPARE(fixture->camera.applied.size(), size_t{1});
        fixture->source.emitActivated("app-b");
        fixture->settle();

        QCOMPARE(fixture->camera.applied.size(), size_t{1});
    }

    void manualApply_CountsAsTheCurrentProfile()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("PA", brightnessId, 1, {"app-a"})}));
        fixture->controller.apply("PA");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);
        QCOMPARE(fixture->camera.applied.size(), size_t{1});

        fixture->source.emitActivated("app-a");
        fixture->settle();

        QCOMPARE(fixture->camera.applied.size(), size_t{1});
    }

    void activationDuringAnOperation_OnlyTheMostRecentApplicationIsEvaluatedAfterwards()
    {
        GatedCamera camera;
        ghub::test::FakeProfileRepository repository;
        ghub::test::FakeEqualizerSettingsPort equalizer;
        ghub::test::FakeActiveApplicationSource source;
        ProfileBook book{repository, camera};
        ApplyProfile applyProfile{camera, equalizer};
        ProfilesController controller{book, applyProfile, equalizer, source, std::chrono::milliseconds{20}};
        repository.loadResult =
            switchingLibrary({makeProfileFor("P0", gainId, 9, {}), makeProfileFor("PA", brightnessId, 1, {"app-a"}),
                              makeProfileFor("PB", contrastId, 2, {"app-b"})});
        controller.load();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        source.emitActivated("janela-inicial");
        auto* timer = controller.findChild<QTimer*>(QStringLiteral("profilesSettleTimer"));
        QVERIFY(timer != nullptr);

        controller.apply("P0");
        QTRY_VERIFY_WITH_TIMEOUT(camera.gate.entered(), 5000);
        source.emitActivated("app-a");
        QTRY_VERIFY_WITH_TIMEOUT(timer->isActive(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!timer->isActive(), 5000); // prazo cumprido durante a operação: guardado
        source.emitActivated("app-b");
        QTRY_VERIFY_WITH_TIMEOUT(timer->isActive(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!timer->isActive(), 5000);
        QVERIFY(controller.busy());

        camera.gate.open();
        QTRY_VERIFY_WITH_TIMEOUT(controller.status().contains("aplicado automaticamente"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);

        QCOMPARE(controller.status(), QStringLiteral("Perfil PB aplicado automaticamente (aplicativo app-b)"));
        QCOMPARE(camera.applied.size(), size_t{2}); // P0 e PB; PA nunca
        QCOMPARE(camera.applied.back().first, contrastId);
    }

    void automaticApplyFailure_IsReportedWithoutRetry()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("PA", brightnessId, 1, {"app-a"})}));
        fixture->camera.applyResults.emplace(brightnessId, fail(ErrorCode::DeviceBusy, "câmera em uso"));

        fixture->source.emitActivated("app-a");
        fixture->settle();
        QTRY_VERIFY_WITH_TIMEOUT(fixture->controller.status().contains("falhas"), 5000);
        fixture->source.emitActivated("app-a");
        fixture->settle();

        QCOMPARE(fixture->camera.applied.size(), size_t{1});
        QVERIFY2(fixture->controller.report().contains("Controle: falhou — câmera em uso"),
                 qPrintable(fixture->controller.report().join('|')));
    }

    void setAutomaticSwitching_On_PersistsAndStartsTheSource()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"x"})}, false));

        fixture->controller.setAutomaticSwitching(true);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 1);
        QVERIFY(fixture->repository.lastSaved.automaticSwitching());
        QCOMPARE(fixture->source.startCalls, 1);
        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("ativa"));
        QVERIFY(fixture->controller.automaticSwitching());
    }

    void setAutomaticSwitching_On_SourceFails_StaysSavedButUnavailable()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"x"})}, false));
        fixture->source.startResult = fail(ErrorCode::BackendUnavailable, "loadScript recusado");

        fixture->controller.setAutomaticSwitching(true);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QVERIFY(fixture->repository.lastSaved.automaticSwitching());
        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("indisponível: loadScript recusado"));
    }

    void setAutomaticSwitching_Off_PersistsAndStopsTheSource()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"app-a"})}));

        fixture->controller.setAutomaticSwitching(false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->source.stopCalls, 1);
        QVERIFY(!fixture->repository.lastSaved.automaticSwitching());
        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("desligada"));

        // Aviso atrasado depois de desligar: ignorado.
        fixture->source.emitActivated("app-a");
        fixture->settle();
        QVERIFY(fixture->camera.applied.empty());
        QCOMPARE(fixture->controller.lastApplication(), QStringLiteral("janela-inicial"));
    }

    void setAutomaticSwitching_PersistenceFailure_DoesNotTouchTheSource()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"x"})}, false));
        fixture->repository.saveResult = fail(ErrorCode::BackendUnavailable, "disco cheio");

        fixture->controller.setAutomaticSwitching(true);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->source.startCalls, 0);
        QVERIFY(!fixture->controller.automaticSwitching());
        QVERIFY2(fixture->controller.status().contains("disco cheio"), qPrintable(fixture->controller.status()));
    }

    void destructor_StopsTheSourceAfterTheWorker()
    {
        ghub::test::FakeActiveApplicationSource source;
        {
            ghub::test::FakeProfileRepository repository;
            ghub::test::FakeCameraSettingsPort camera;
            ghub::test::FakeEqualizerSettingsPort equalizer;
            ProfileBook book{repository, camera};
            ApplyProfile applyProfile{camera, equalizer};
            repository.loadResult = switchingLibrary({makeProfileFor("A", brightnessId, 1, {"x"})});
            ProfilesController controller{book, applyProfile, equalizer, source};
            controller.load();
            QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
            QCOMPARE(source.stopCalls, 0);
        }
        QCOMPARE(source.stopCalls, 1);
    }

    void assignLastApplication_PersistsTheAssociation()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("A", brightnessId, 1, {})}));
        fixture->source.emitActivated("org.mozilla.firefox");
        fixture->settle();

        fixture->controller.assignLastApplication("A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 1);
        const auto application = ApplicationId::create("org.mozilla.firefox").value();
        QVERIFY(fixture->repository.lastSaved.profileFor(application) != nullptr);
        QCOMPARE(fixture->controller.profileApplications(), (QVariantMap{{"A", QStringList{"org.mozilla.firefox"}}}));
    }

    void assignLastApplication_WithoutApplication_ReportsAnErrorAndWritesNothing()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {})}, false));

        fixture->controller.assignLastApplication("A");

        QVERIFY(!fixture->controller.busy());
        QCOMPARE(fixture->repository.saveCalls, 0);
        QVERIFY2(fixture->controller.status().contains("nenhum aplicativo"), qPrintable(fixture->controller.status()));
    }

    void assignLastApplication_AlreadyInAnotherProfile_ReportsTheFailure()
    {
        fixture->startSwitching(switchingLibrary(
            {makeProfileFor("A", brightnessId, 1, {"app-a"}), makeProfileFor("B", contrastId, 2, {})}));
        fixture->source.emitActivated("app-a");
        fixture->settle();
        fixture->controller.assignLastApplication("B");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 0);
        QVERIFY2(fixture->controller.status().contains("Falha"), qPrintable(fixture->controller.status()));
        QVERIFY(fixture->controller.status().contains("A"));
    }

    void unassignApplication_Persists()
    {
        fixture->loadLibrary(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"app-a", "app-b"})}, false));

        fixture->controller.unassignApplication("app-a");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->repository.saveCalls, 1);
        QCOMPARE(fixture->controller.profileApplications(), (QVariantMap{{"A", QStringList{"app-b"}}}));
    }

    void sourceReload_NextActivationOnlyUpdatesLastApplication()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"app-a"})}));

        fixture->source.emitStatus(std::nullopt);
        fixture->source.emitActivated("app-a");
        fixture->settle();

        QCOMPARE(fixture->controller.lastApplication(), QStringLiteral("app-a"));
        QVERIFY(fixture->camera.applied.empty());
        fixture->source.emitActivated("app-a");
        fixture->settle();
        QCOMPARE(fixture->camera.applied.size(), size_t{1});
    }

    void sourceStatusError_ReportsUnavailableAndRecovers()
    {
        fixture->startSwitching(switchingLibrary({makeProfileFor("A", brightnessId, 1, {"app-a"})}));

        fixture->source.emitStatus(ghub::shared_kernel::DomainError{ErrorCode::BackendUnavailable, "KWin saiu"});
        fixture->drainQueue();
        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("indisponível: KWin saiu"));

        fixture->source.emitStatus(std::nullopt);
        fixture->drainQueue();
        QCOMPARE(fixture->controller.switchingStatus(), QStringLiteral("ativa"));
    }

    // ---- INC-017: duplicar, exportar, importar, padrão e persistente ----

    void duplicate_CreatesACopyAndPublishesIt()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));

        fixture->controller.duplicate("A", "A cópia");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->controller.profileNames(), (QStringList{"A", "A cópia"}));
        QCOMPARE(fixture->repository.saveCalls, 1);
        QVERIFY2(fixture->controller.status().contains("A cópia"), qPrintable(fixture->controller.status()));
        QVERIFY(fixture->camera.applied.empty());
    }

    void duplicate_ExistingName_ReportsFailure()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));

        fixture->controller.duplicate("A", "A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QVERIFY2(fixture->controller.status().startsWith("Falha"), qPrintable(fixture->controller.status()));
        QCOMPARE(fixture->repository.saveCalls, 0);
    }

    void exportProfile_SendsThePathToTheTransferAndAcceptsFileUrls()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));

        fixture->controller.exportProfile("A", "file:///tmp/perfil%20a.json");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->transfer.exportedPaths.size(), size_t{1});
        QCOMPARE(QString::fromStdString(fixture->transfer.exportedPaths.front()), QStringLiteral("/tmp/perfil a.json"));
        QCOMPARE(fixture->repository.saveCalls, 0);
        QVERIFY2(fixture->controller.status().contains("exportado"), qPrintable(fixture->controller.status()));
    }

    void importProfile_AddsTheProfileListsIgnoredFieldsAndAppliesNothing()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));
        fixture->transfer.importResult = ghub::profiles::ImportedProfile{
            makeProfile("Novo", {{contrastId, "Contrast", 5}}, true), {"extra", "camera[].zzz"}};

        fixture->controller.importProfile("/tmp/in.json", false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(fixture->controller.profileNames(), (QStringList{"A", "Novo"}));
        QVERIFY2(fixture->controller.status().contains("importado"), qPrintable(fixture->controller.status()));
        QVERIFY(fixture->controller.report().join("\n").contains("extra, camera[].zzz"));
        QVERIFY(fixture->camera.applied.empty());
        QVERIFY(fixture->equalizer.requested.empty());
    }

    void importProfile_ExistingName_AsksBeforeReplacing()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));
        fixture->transfer.importResult =
            ghub::profiles::ImportedProfile{makeProfile("A", {{brightnessId, "Brightness", 77}}, false), {}};
        QSignalSpy needsReplace{&fixture->controller, &ProfilesController::importNeedsReplace};

        fixture->controller.importProfile("/tmp/in.json", false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QCOMPARE(needsReplace.count(), 1);
        QCOMPARE(needsReplace.front().at(0).toString(), QStringLiteral("A"));
        QCOMPARE(needsReplace.front().at(1).toString(), QStringLiteral("/tmp/in.json"));
        QCOMPARE(fixture->repository.saveCalls, 0);

        fixture->controller.importProfile("/tmp/in.json", true);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);
        QCOMPARE(fixture->repository.lastSaved.profiles().front().camera().front().value, 77);
        QCOMPARE(needsReplace.count(), 1);
    }

    void importProfile_Malformed_ReportsFailureWithoutSignal()
    {
        fixture->transfer.importResult = fail(ErrorCode::MalformedData, "arquivo maior que 64 KiB");
        QSignalSpy needsReplace{&fixture->controller, &ProfilesController::importNeedsReplace};

        fixture->controller.importProfile("/tmp/in.json", false);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QVERIFY2(fixture->controller.status().contains("64 KiB"), qPrintable(fixture->controller.status()));
        QCOMPARE(needsReplace.count(), 0);
        QCOMPARE(fixture->repository.saveCalls, 0);
    }

    void setDefaultProfile_PublishesAndClears()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, false)}));
        QVERIFY(fixture->controller.defaultProfile().isEmpty());

        fixture->controller.setDefaultProfile("A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);
        QCOMPARE(fixture->controller.defaultProfile(), QStringLiteral("A"));

        fixture->controller.setDefaultProfile(QString());
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);
        QVERIFY(fixture->controller.defaultProfile().isEmpty());
        QVERIFY(fixture->camera.applied.empty());
    }

    void defaultProfile_IsAppliedWhenTheFocusedApplicationHasNoRule()
    {
        auto library = switchingLibrary(
            {makeProfileFor("PA", brightnessId, 1, {"app-a"}), makeProfileFor("Padrao", contrastId, 9, {})});
        QVERIFY(library.setDefaultProfile(ProfileName::create("Padrao").value()).has_value());
        fixture->startSwitching(std::move(library));
        QSignalSpy cameraApplied{&fixture->controller, &ProfilesController::cameraApplied};

        fixture->source.emitActivated("dolphin");
        QTRY_COMPARE_WITH_TIMEOUT(cameraApplied.count(), 1, 5000);
        fixture->settle();

        QCOMPARE(fixture->camera.applied.size(), size_t{1});
        QCOMPARE(fixture->camera.applied.front().first, contrastId);
    }

    void setPersistent_PublishesAndPersistentSectionsAreNotAppliedWithAProfile()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, true)}));

        fixture->controller.setPersistent("camera", true);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);
        QVERIFY(fixture->controller.persistentCamera());
        QVERIFY(!fixture->controller.persistentEqualizer());

        QSignalSpy cameraApplied{&fixture->controller, &ProfilesController::cameraApplied};
        fixture->controller.apply("A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);

        QVERIFY(fixture->camera.applied.empty());
        QCOMPARE(cameraApplied.count(), 0);
        QCOMPARE(fixture->equalizer.requested.size(), size_t{1});
        QVERIFY(fixture->controller.report().join("\n").contains("Câmera: igual em todos os perfis"));

        fixture->controller.setPersistent("equalizer", true);
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);
        fixture->controller.apply("A");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->controller.busy(), 5000);
        QCOMPARE(fixture->equalizer.requested.size(), size_t{1}); // EQ também persistente: nada novo
    }

    void setPersistent_UnknownSection_IsRefused()
    {
        fixture->loadLibrary(libraryOf({makeProfile("A", {{brightnessId, "Brightness", 1}}, true)}));

        fixture->controller.setPersistent("sidetone", true);

        QVERIFY2(fixture->controller.status().startsWith("Falha"), qPrintable(fixture->controller.status()));
        QCOMPARE(fixture->repository.saveCalls, 0);
    }

private:
    std::unique_ptr<Fixture> fixture;
};

} // namespace

QTEST_GUILESS_MAIN(ProfilesControllerTest)
#include "ProfilesControllerTest.moc"
