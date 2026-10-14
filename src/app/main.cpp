// Composition root (ADR-0002 §7): monta os módulos e carrega a janela QML.
// Ligado à UI: módulos device_inventory (ADR-0006), battery (kernel power_supply, ADR-0004/ADR-0005;
// G733 via HeadsetControl, ADR-0007) e camera (Brio 100 por V4L2: leitura só com ioctls de consulta,
// ADR-0008; pré-visualização por streaming V4L2, ADR-0009) e pointer (G703: configuração lida do ratbagd por
// D-Bus, somente leitura, ADR-0011) e lighting (modo ao vivo do G915 por HID++, ADR-0013).
// A leitura do G703 só começa por clique (botão "Ler configuração do mouse"): ela ativa o ratbagd (root) e
// suspende a bateria do G733 enquanto o serviço estiver ativo; nada é lido na abertura nem no smoke test.
// O stream da câmera só começa por ação do usuário (botão da janela). Escrita em dispositivo: só os controles
// UVC da Brio 100 (VIDIOC_S_CTRL sob DeviceLease, ADR-0010), e apenas por ação do usuário na janela; nada é
// gravado na abertura, na releitura nem na retomada. A única outra escrita é o modo ao vivo (volátil) do G915,
// também só por ação do usuário na janela (ADR-0013): nada é aberto na abertura nem no smoke test, e fechar o
// modo ou o aplicativo (inclusive por SIGTERM/SIGINT/SIGHUP) devolve o controle ao firmware; SIGKILL não (testes
// físicos de 2026-10-14). Repouso do teclado (o firmware retoma o controle) desliga o modo na janela sem escrever,
// e a suspensão do sistema devolve o controle antes de suspender (ADR-0014). Sidetone e luzes do G733 (HeadsetControl, ADR-0015): só por clique
// na janela, nunca na abertura nem na reconexão. Nenhuma outra escrita de configuração.
#include "TerminationSignals.hpp"
#include "modules/battery/application/GetBatteryStatus.hpp"
#include "modules/battery/infrastructure/HeadsetControlBatterySource.hpp"
#include "modules/battery/infrastructure/KernelUevent.hpp"
#include "modules/battery/infrastructure/SysfsBatterySource.hpp"
#include "modules/battery/infrastructure/UeventBatteryMonitor.hpp"
#include "modules/battery/presentation/BatteryViewModel.hpp"
#include "modules/camera/application/GetCameraStatus.hpp"
#include "modules/camera/application/SetCameraControl.hpp"
#include "modules/camera/application/StartCameraPreview.hpp"
#include "modules/camera/infrastructure/BackgroundCameraReader.hpp"
#include "modules/camera/infrastructure/CameraPreviewWorker.hpp"
#include "modules/camera/infrastructure/LinuxV4l2Api.hpp"
#include "modules/camera/infrastructure/V4l2CameraBackend.hpp"
#include "modules/camera/infrastructure/V4l2ControlAccess.hpp"
#include "modules/camera/infrastructure/V4l2VideoCapture.hpp"
#include "modules/camera/presentation/CameraControlEditor.hpp"
#include "modules/camera/presentation/CameraPreviewItem.hpp"
#include "modules/camera/presentation/CameraPreviewViewModel.hpp"
#include "modules/camera/presentation/CameraViewModel.hpp"
#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/infrastructure/LogindSleepMonitor.hpp"
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/device_inventory/application/ListDetectedDevices.hpp"
#include "modules/device_inventory/infrastructure/SysfsHidDiscovery.hpp"
#include "modules/device_inventory/infrastructure/SysfsVideoDiscovery.hpp"
#include "modules/device_inventory/presentation/DeviceInventoryViewModel.hpp"
#include "modules/headset/application/ChangeHeadsetSetting.hpp"
#include "modules/headset/infrastructure/HeadsetControlSettingsWriter.hpp"
#include "modules/headset/presentation/HeadsetSettingsController.hpp"
#include "modules/lighting/application/LiveKeyboardSession.hpp"
#include "modules/lighting/infrastructure/G915HidppLighting.hpp"
#include "modules/lighting/infrastructure/HidppNodeLocator.hpp"
#include "modules/lighting/presentation/KeyboardLightingController.hpp"
#include "modules/pointer/application/ReadPointerConfiguration.hpp"
#include "modules/pointer/infrastructure/BackgroundPointerReader.hpp"
#include "modules/pointer/infrastructure/RatbagPointerBackend.hpp"
#include "modules/pointer/presentation/PointerViewModel.hpp"

#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QtQml/qqml.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <utility>

namespace {

// R12 (INC-008): um registro por comando HID++, sem os bytes dos parâmetros. Sucesso em debug (desligado por padrão;
// QT_LOGGING_RULES="ghub.lighting.hidpp.debug=true"), falha em warning.
Q_LOGGING_CATEGORY(lcLightingHidpp, "ghub.lighting.hidpp", QtInfoMsg)

void traceHidppCommand(const ghub::lighting::infrastructure::HidppCommandTrace& trace)
{
    if (!trace.failure.has_value()) {
        qCDebug(lcLightingHidpp, "dev %u feature 0x%04x fn %u (%zu bytes): ok", trace.deviceIndex, trace.featureId,
                trace.function, trace.paramCount);
        return;
    }
    qCWarning(lcLightingHidpp, "dev %u feature 0x%04x fn %u (%zu bytes): falha tipo %d, código 0x%02x",
              trace.deviceIndex, trace.featureId, trace.function, trace.paramCount,
              static_cast<int>(*trace.failure), trace.errorCode);
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ghub-linux"));
    QGuiApplication::setApplicationVersion(QStringLiteral(GHUB_VERSION));
    // SIGTERM/SIGINT/SIGHUP encerram pelo laço de eventos, para que os destrutores rodem (ex.: devolver o controle
    // do G915 ao firmware). Declarado logo após o app: é destruído por último e restaura os tratadores anteriores.
    ghub::app::TerminationSignals terminationSignals;
    if (!terminationSignals.isActive()) {
        qWarning("app: tratamento de SIGTERM/SIGINT/SIGHUP indisponível; esses sinais encerram sem limpeza");
    }
    QObject::connect(&terminationSignals, &ghub::app::TerminationSignals::terminationRequested, &app,
                     [](int) { QCoreApplication::quit(); });

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption smokeTest(QStringLiteral("smoke-test"),
                                       QStringLiteral("Carrega a interface e encerra (usado pelo CTest)."));
    parser.addOption(smokeTest);
    parser.process(app);

    using namespace std::chrono_literals;
    // Registro de leases (DEVICE_ACCESS_POLICY §2), compartilhado por todos os módulos que escrevem ou consultam
    // dispositivos. Declarado antes deles: é destruído depois.
    const ghub::device_access::SteadyClock steadyClock;
    ghub::device_access::AccessRegistry accessRegistry{steadyClock};
    ghub::battery::SysfsBatterySource batterySource;
    // G733 (ADR-0007): consulta assíncrona a cada 60 s, timeout de 10 s, só sem software que
    // disputa o hidraw do headset (DEVICE_ACCESS_POLICY §3) e com o lease do headset (ADR-0015 §5).
    ghub::device_access::ProcExternalProcessProbe externalProcessProbe;
    auto headsetInterface = ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:0afe");
    if (!headsetInterface) {
        qCritical("headset: identificador de interface inválido: %s",
                  qPrintable(QString::fromStdString(headsetInterface.error().detail)));
        return EXIT_FAILURE;
    }
    const auto headsetControlExecutable = QStandardPaths::findExecutable(QStringLiteral("headsetcontrol"));
    ghub::battery::HeadsetControlBatterySource headsetBatterySource{
        headsetControlExecutable, externalProcessProbe, accessRegistry, *headsetInterface, 60s, 10s};
    const ghub::battery::GetBatteryStatus getBatteryStatus{{&batterySource, &headsetBatterySource}};
    ghub::battery::BatteryViewModel batteryViewModel{getBatteryStatus, 30s};
    QObject::connect(&headsetBatterySource, &ghub::battery::HeadsetControlBatterySource::readingsChanged,
                     &batteryViewModel, &ghub::battery::BatteryViewModel::refresh);
    // Inventário de dispositivos (ADR-0006 §6): sysfs HID, somente leitura; releitura a cada 5 s.
    ghub::device_inventory::SysfsHidDiscovery hidDiscovery;
    ghub::device_inventory::SysfsVideoDiscovery videoDiscovery;
    const ghub::device_inventory::ListDetectedDevices listDetectedDevices{{&hidDiscovery, &videoDiscovery}};
    ghub::device_inventory::DeviceInventoryViewModel deviceInventoryViewModel{listDetectedDevices, 5s};
    // Câmera (ADR-0008): Brio 100 lida por V4L2 só com ioctls de consulta, numa thread de trabalho,
    // ao iniciar e a cada 60 s (os ioctls de controle acordam a câmera; não reler mais que isso).
    // O ViewModel só lê o cache: é atualizado quando uma leitura termina; o timer dele é reserva.
    constexpr ghub::device_inventory::UsbId brio100{.vendorId = 0x046d, .productId = 0x094c};
    ghub::camera::LinuxV4l2Api v4l2Api;
    ghub::camera::V4l2CameraBackend v4l2CameraBackend{brio100, v4l2Api};
    ghub::camera::BackgroundCameraReader cameraReader{v4l2CameraBackend, 60s};
    const ghub::camera::GetCameraStatus getCameraStatus{cameraReader};
    ghub::camera::CameraViewModel cameraViewModel{getCameraStatus, 60s};
    QObject::connect(&cameraReader, &ghub::camera::BackgroundCameraReader::cameraRead, &cameraViewModel,
                     &ghub::camera::CameraViewModel::refresh);
    // Pré-visualização (ADR-0009): captura por streaming V4L2 numa thread de trabalho, iniciada só por
    // toggle() (clique do usuário). Ordem de destruição (inversa à declaração): ViewModel (pede stop),
    // worker (junta a thread), captura e, por fim, a API V4L2.
    ghub::camera::V4l2VideoCapture videoCapture{brio100, v4l2Api};
    ghub::camera::CameraPreviewWorker previewWorker{videoCapture};
    const ghub::camera::StartCameraPreview startCameraPreview{cameraReader, previewWorker};
    ghub::camera::CameraPreviewViewModel cameraPreviewViewModel{startCameraPreview, previewWorker};
    QObject::connect(&previewWorker, &ghub::camera::CameraPreviewWorker::started, &cameraPreviewViewModel,
                     &ghub::camera::CameraPreviewViewModel::onStarted);
    QObject::connect(&previewWorker, &ghub::camera::CameraPreviewWorker::frameReady, &cameraPreviewViewModel,
                     &ghub::camera::CameraPreviewViewModel::onFrame);
    QObject::connect(&previewWorker, &ghub::camera::CameraPreviewWorker::ended, &cameraPreviewViewModel,
                     &ghub::camera::CameraPreviewViewModel::onEnded);
    // Escrita de controles (ADR-0010): só por ação do usuário (CameraControlEditor); nada grava na construção.
    // Ordem de destruição (inversa à declaração): editor (espera a escrita em andamento), caso de uso, acesso
    // V4L2, a API V4L2 (declarada acima) e, por fim, o registro de leases e o relógio (declarados no início).
    ghub::camera::V4l2ControlAccess controlAccess{brio100, v4l2Api};
    auto cameraInterface = ghub::device_access::PhysicalInterfaceId::create("v4l2:046d:094c");
    if (!cameraInterface) {
        qCritical("camera: identificador de interface inválido: %s",
                  qPrintable(QString::fromStdString(cameraInterface.error().detail)));
        return EXIT_FAILURE;
    }
    const ghub::camera::SetCameraControl setCameraControl{controlAccess, accessRegistry, *cameraInterface};
    ghub::camera::CameraControlEditor cameraControlEditor{setCameraControl};
    // Após cada escrita (sucesso ou falha) a janela relê a câmera para mostrar valores e flags de inativo.
    QObject::connect(&cameraControlEditor, &ghub::camera::CameraControlEditor::controlWritten, &cameraReader,
                     &ghub::camera::BackgroundCameraReader::refresh);
    // Mouse (ADR-0011): leitura do ratbagd (barramento de sistema) só por clique em PointerViewModel::read();
    // nada é lido na construção. Ordem de destruição (inversa à declaração): ViewModel, leitor (espera a leitura
    // em andamento), caso de uso, backend e, por fim, o registro de leases e o probe (declarados acima).
    ghub::pointer::RatbagPointerBackend ratbagPointerBackend{QDBusConnection::systemBus(),
                                                             QStringLiteral("org.freedesktop.ratbag1"), 30s};
    auto pointerInterface = ghub::device_access::PhysicalInterfaceId::create("ratbag:046d:4086");
    if (!pointerInterface) {
        qCritical("pointer: identificador de interface inválido: %s",
                  qPrintable(QString::fromStdString(pointerInterface.error().detail)));
        return EXIT_FAILURE;
    }
    const ghub::pointer::ReadPointerConfiguration readPointerConfiguration{ratbagPointerBackend, externalProcessProbe,
                                                                           accessRegistry, *pointerInterface};
    ghub::pointer::BackgroundPointerReader pointerReader{readPointerConfiguration};
    ghub::pointer::PointerViewModel pointerViewModel{pointerReader};
    QObject::connect(&pointerReader, &ghub::pointer::BackgroundPointerReader::readFinished, &pointerViewModel,
                     &ghub::pointer::PointerViewModel::onReadFinished);
    // Teclado (ADR-0013): o modo ao vivo do G915 só é aberto por clique em KeyboardLightingController::startLive();
    // nada é aberto nem escrito na construção. Ordem de destruição (inversa à declaração): o controlador primeiro
    // (espera a operação em andamento e devolve o controle ao firmware), depois o caso de uso, o adaptador e,
    // por fim, o registro de leases e o relógio (declarados acima).
    ghub::lighting::infrastructure::G915HidppLighting g915Lighting{ghub::lighting::infrastructure::HidppNodeLocator{},
                                                                   steadyClock, traceHidppCommand};
    auto keyboardInterface = ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:407c");
    if (!keyboardInterface) {
        qCritical("lighting: identificador de interface inválido: %s",
                  qPrintable(QString::fromStdString(keyboardInterface.error().detail)));
        return EXIT_FAILURE;
    }
    const ghub::lighting::application::StartLiveKeyboardSession startLiveKeyboardSession{
        g915Lighting, accessRegistry, externalProcessProbe, *keyboardInterface};
    ghub::lighting::presentation::KeyboardLightingController keyboardLightingController{startLiveKeyboardSession};
    // Headset (ADR-0015): sidetone e luzes do G733 pelo HeadsetControl, só por clique no controlador; nada é
    // executado na construção. Ordem de destruição: controlador (espera a escrita em andamento), caso de uso,
    // writer e, por fim, o registro de leases e o probe (declarados acima).
    ghub::headset::HeadsetControlSettingsWriter headsetSettingsWriter{headsetControlExecutable, 10s};
    const ghub::headset::ChangeHeadsetSetting changeHeadsetSetting{headsetSettingsWriter, externalProcessProbe,
                                                                   accessRegistry, *headsetInterface};
    ghub::headset::HeadsetSettingsController headsetSettingsController{changeHeadsetSetting};
    qmlRegisterType<ghub::camera::CameraPreviewItem>("GHub.Camera", 1, 0, "CameraPreviewItem");
    // Uevent do kernel antecipa a releitura; sem ele vale só o polling de 30 s (ADR-0005 §4).
    std::unique_ptr<ghub::battery::UeventBatteryMonitor> batteryMonitor;
    if (auto socket = ghub::battery::openKernelUeventSocket()) {
        batteryMonitor = std::make_unique<ghub::battery::UeventBatteryMonitor>(std::move(*socket));
        QObject::connect(batteryMonitor.get(), &ghub::battery::UeventBatteryMonitor::batteryChanged, &batteryViewModel,
                         &ghub::battery::BatteryViewModel::refresh);
        // ADR-0006 §6: o mesmo uevent também antecipa a releitura do inventário.
        QObject::connect(batteryMonitor.get(), &ghub::battery::UeventBatteryMonitor::batteryChanged,
                         &deviceInventoryViewModel, &ghub::device_inventory::DeviceInventoryViewModel::refresh);
    } else {
        qWarning("battery: notificação por uevent indisponível (%s); usando só releitura periódica",
                 qPrintable(QString::fromStdString(socket.error().detail)));
    }

    // Declarado depois dos objetos acima: é destruído antes deles, sem referências pendentes no QML.
    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("batteryViewModel"), QVariant::fromValue(&batteryViewModel)},
        {QStringLiteral("deviceInventoryViewModel"), QVariant::fromValue(&deviceInventoryViewModel)},
        {QStringLiteral("cameraViewModel"), QVariant::fromValue(&cameraViewModel)},
        {QStringLiteral("cameraPreviewViewModel"), QVariant::fromValue(&cameraPreviewViewModel)},
        {QStringLiteral("cameraControlEditor"), QVariant::fromValue(&cameraControlEditor)},
        {QStringLiteral("pointerViewModel"), QVariant::fromValue(&pointerViewModel)},
        {QStringLiteral("keyboardLightingController"), QVariant::fromValue(&keyboardLightingController)},
        {QStringLiteral("headsetSettingsController"), QVariant::fromValue(&headsetSettingsController)},
    });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    // No teste de fumaça qualquer aviso do QML (ex.: TypeError em binding) é falha.
    bool qmlWarned = false;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&qmlWarned](const QList<QQmlError>&) { qmlWarned = true; });
    engine.loadFromModule("GHub", "Main");

    if (parser.isSet(smokeTest)) {
        return engine.rootObjects().isEmpty() || qmlWarned ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    // ADR-0014 (R16): antes de suspender, devolve o controle do G915 ao firmware sob um inhibitor "delay" do logind.
    // Fora do smoke test, para o CTest não tocar no logind real. Sem logind: só um aviso.
    ghub::device_access::LogindSleepMonitor sleepMonitor{QDBusConnection::systemBus()};
    QObject::connect(&sleepMonitor, &ghub::device_access::LogindSleepMonitor::aboutToSleep,
                     &keyboardLightingController,
                     &ghub::lighting::presentation::KeyboardLightingController::releaseForSleep);
    if (!sleepMonitor.start()) {
        qWarning("app: aviso de suspensão do logind indisponível (%s); o modo ao vivo não é desligado antes de "
                 "suspender",
                 qPrintable(sleepMonitor.lastError()));
    }
    return QGuiApplication::exec();
}
