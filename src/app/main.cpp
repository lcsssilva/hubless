// Composition root (ADR-0002 §7): monta os módulos e carrega a janela QML.
// Ligado à UI: módulos device_inventory (ADR-0006), battery (kernel power_supply, ADR-0004/ADR-0005;
// G733 via HeadsetControl, ADR-0007) e camera (Brio 100 por V4L2: leitura só com ioctls de consulta,
// ADR-0008; pré-visualização por streaming V4L2, ADR-0009) e pointer (G703: configuração lida do setor onboard
// ativo por HID++ próprio, somente leitura, ADR-0027) e lighting (modo ao vivo do G915 por HID++, ADR-0013).
// A leitura do G703 só começa por clique (botão "Ler configuração do mouse"): lê o setor onboard ativo sem ativar o
// ratbagd; nada é lido na abertura nem no smoke test.
// O stream da câmera só começa por ação do usuário (botão da janela). Escrita em dispositivo: só os controles
// UVC da Brio 100 (VIDIOC_S_CTRL sob DeviceLease, ADR-0010), e apenas por ação do usuário na janela; nada é
// gravado na abertura, na releitura nem na retomada. A única outra escrita é o modo ao vivo (volátil) do G915,
// também só por ação do usuário na janela (ADR-0013): nada é aberto na abertura nem no smoke test, e fechar o
// modo ou o aplicativo (inclusive por SIGTERM/SIGINT/SIGHUP) devolve o controle ao firmware; SIGKILL não (testes
// físicos de 2026-10-14). Repouso do teclado (o firmware retoma o controle) desliga o modo na janela sem escrever,
// e a suspensão do sistema devolve o controle antes de suspender (ADR-0014). Sidetone e luzes do G733 (HeadsetControl,
// ADR-0015): só por clique na janela, nunca na abertura nem na reconexão. EQ de saída do G733 no host (PipeWire
// filter-chain dentro do processo, ADR-0016) e redução de ruído do microfone (RNNoise, ADR-0018): só por clique; ao
// ligar, trocam a saída/entrada padrão e a restauram ao desligar/fechar. Nenhuma outra escrita de configuração.
// Perfis (INC-012, ADR-0020): salvar, atualizar e aplicar controles da Brio 100 e a curva do EQ só por clique;
// a abertura só lê ~/.config/ghub-linux/profiles.json.
// INC-013 (ADR-0019): integração com o desktop — bandeja do sistema (X-03), inicialização automática XDG
// (X-04) e diagnóstico/logs (X-06). A bandeja só existe fora do smoke test e com uma bandeja disponível;
// fechar a janela esconde para a bandeja (o processo continua) e só "Sair" encerra. O autostart é opt-in na
// janela: nada é gravado sem ação explícita; `--autostart` inicia escondido. O diagnóstico vive só em memória
// e nada é gravado até o usuário exportar o relatório.
#include "DesktopController.hpp"
#include "SystemTrayIcon.hpp"
#include "TerminationSignals.hpp"
#include "modules/audio_processing/application/HostEqualizer.hpp"
#include "modules/audio_processing/application/HostNoiseSuppression.hpp"
#include "modules/audio_processing/infrastructure/PipeWireAudioGraph.hpp"
#include "modules/audio_processing/presentation/HostEqualizerController.hpp"
#include "modules/audio_processing/presentation/NoiseSuppressionController.hpp"
#include "modules/battery/application/GetBatteryStatus.hpp"
#include "modules/battery/infrastructure/HeadsetControlBatterySource.hpp"
#include "modules/battery/infrastructure/KernelUevent.hpp"
#include "modules/battery/infrastructure/SysfsBatterySource.hpp"
#include "modules/battery/infrastructure/UeventBatteryMonitor.hpp"
#include "modules/battery/presentation/BatteryViewModel.hpp"
#include "modules/camera/api/CameraSettings.hpp"
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
#include "modules/device_inventory/application/ReadDeviceIdentity.hpp"
#include "modules/device_inventory/infrastructure/HidppIdentitySource.hpp"
#include "modules/device_inventory/infrastructure/SysfsHidDiscovery.hpp"
#include "modules/device_inventory/infrastructure/SysfsVideoDiscovery.hpp"
#include "modules/device_inventory/presentation/DeviceIdentityViewModel.hpp"
#include "modules/device_inventory/presentation/DeviceInventoryViewModel.hpp"
#include "modules/diagnostics/application/DiagnosticLog.hpp"
#include "modules/diagnostics/application/ExportDiagnosticReport.hpp"
#include "modules/diagnostics/domain/DiagnosticTypes.hpp"
#include "modules/diagnostics/infrastructure/QtMessageLogSink.hpp"
#include "modules/diagnostics/infrastructure/XdgReportWriter.hpp"
#include "modules/diagnostics/presentation/DiagnosticsViewModel.hpp"
#include "modules/headset/application/ChangeHeadsetSetting.hpp"
#include "modules/headset/application/ReadHeadsetStatus.hpp"
#include "modules/headset/infrastructure/HeadsetControlSettingsWriter.hpp"
#include "modules/headset/infrastructure/HidppHeadsetStatusSource.hpp"
#include "modules/headset/presentation/HeadsetSettingsController.hpp"
#include "modules/headset/presentation/HeadsetStatusViewModel.hpp"
#include "modules/lighting/application/LiveKeyboardSession.hpp"
#include "modules/lighting/application/ReadKeyboardStatus.hpp"
#include "modules/lighting/infrastructure/G915HidppLighting.hpp"
#include "modules/lighting/infrastructure/HidppKeyboardStatusSource.hpp"
#include "modules/lighting/presentation/KeyboardLightingController.hpp"
#include "modules/lighting/presentation/KeyboardStatusViewModel.hpp"
#include "modules/onboard_memory/api/FileOnboardBackupStore.hpp"
#include "modules/pointer/application/G703OnboardSettings.hpp"
#include "modules/pointer/application/ReadPointerConfiguration.hpp"
#include "modules/pointer/application/ReadPointerSensorStatus.hpp"
#include "modules/pointer/infrastructure/BackgroundPointerReader.hpp"
#include "modules/pointer/infrastructure/G703NodeLocator.hpp"
#include "modules/pointer/infrastructure/HidppPointerSensorSource.hpp"
#include "modules/pointer/infrastructure/OnboardPointerBackend.hpp"
#include "modules/pointer/presentation/G703OnboardController.hpp"
#include "modules/pointer/presentation/PointerSensorStatusViewModel.hpp"
#include "modules/pointer/presentation/PointerViewModel.hpp"
#include "modules/profiles/application/ApplyProfile.hpp"
#include "modules/profiles/application/ProfileBook.hpp"
#include "modules/profiles/infrastructure/CameraSettingsAdapter.hpp"
#include "modules/profiles/infrastructure/EqualizerSettingsAdapter.hpp"
#include "modules/profiles/infrastructure/InactiveApplicationSource.hpp"
#include "modules/profiles/infrastructure/JsonProfileRepository.hpp"
#include "modules/profiles/infrastructure/KWinActiveApplicationSource.hpp"
#include "modules/profiles/presentation/ProfilesController.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QSysInfo>
#include <QtQml/qqml.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

// R12 (INC-008): um registro por comando HID++, sem os bytes dos parâmetros. Sucesso em debug (desligado por padrão;
// QT_LOGGING_RULES="ghub.lighting.hidpp.debug=true"), falha em warning.
Q_LOGGING_CATEGORY(lcLightingHidpp, "ghub.lighting.hidpp", QtInfoMsg)

void traceHidppCommand(const ghub::platform::hidpp::HidppCommandTrace& trace)
{
    if (!trace.failure.has_value()) {
        qCDebug(lcLightingHidpp, "dev %u feature 0x%04x fn %u (%zu bytes): ok", trace.deviceIndex, trace.featureId,
                trace.function, trace.paramCount);
        return;
    }
    qCWarning(lcLightingHidpp, "dev %u feature 0x%04x fn %u (%zu bytes): falha tipo %d, código 0x%02x",
              trace.deviceIndex, trace.featureId, trace.function, trace.paramCount, static_cast<int>(*trace.failure),
              trace.errorCode);
}

// INC-013 R15: sonda simples para a saúde dos backends, usada só para exibir (nada é alterado).
bool serviceRegistered(const QString& service)
{
    auto* const interface = QDBusConnection::systemBus().interface();
    return interface != nullptr && interface->isServiceRegistered(service);
}

// Socket padrão do PipeWire na sessão do usuário ($XDG_RUNTIME_DIR/pipewire-0). O módulo de áudio é de
// outro incremento; aqui só se informa se o servidor do host está no ar.
bool pipeWireSocketExists()
{
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    return !runtimeDir.isEmpty() && QFileInfo::exists(runtimeDir + QStringLiteral("/pipewire-0"));
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ghub-linux"));
    QCoreApplication::setApplicationVersion(QStringLiteral(GHUB_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("ghub-linux"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("ghub-linux")));
    // Diagnóstico (INC-013 R10–R15): buffer circular em memória das mensagens do Qt. Declarado cedo para
    // capturar os logs dos módulos e destruído por último; o tratador é global e nada toca o disco até exportar.
    ghub::diagnostics::DiagnosticLog diagnosticLog{500};
    ghub::diagnostics::QtMessageLogSink messageLogSink{diagnosticLog};
    // SIGTERM/SIGINT/SIGHUP encerram pelo laço de eventos, para que os destrutores rodem (ex.: devolver o controle
    // do G915 ao firmware). Declarado logo após o app: restaura os tratadores anteriores ao ser destruído.
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
    const QCommandLineOption autostart(QStringLiteral("autostart"),
                                       QStringLiteral("Inicia escondido na bandeja (entrada XDG de autostart)."));
    parser.addOption(autostart);
    parser.process(app);

    const bool smokeTestRun = parser.isSet(smokeTest);
    // Bandeja (INC-013 R1/R2): `QSystemTrayIcon` exige `QApplication`. No smoke test nem se consulta o tema da
    // plataforma; sem bandeja nada é criado e o app funciona como antes.
    const bool trayAvailable = !smokeTestRun && ghub::app::SystemTrayIcon::trayAvailable();
    ghub::app::DesktopController desktopController{trayAvailable, parser.isSet(autostart)};
    QObject::connect(&desktopController, &ghub::app::DesktopController::quitRequested, &app,
                     [] { QCoreApplication::quit(); });

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
    // Identidade HID++ (ADR-0024): nome, tipo, firmware e mapa de features de G915, G703 e G733, só leitura, só por
    // clique em DeviceIdentityViewModel::read(); nada é aberto nem lido na construção. Ordem de destruição (inversa à
    // declaração): ViewModel (espera a leitura em andamento), caso de uso, fonte e, por fim, o registro de leases.
    ghub::device_inventory::HidppIdentitySource hidppIdentitySource{ghub::platform::hidpp::HidppNodeLocator{},
                                                                    steadyClock};
    const ghub::device_inventory::ReadDeviceIdentity readDeviceIdentity{hidppIdentitySource, accessRegistry};
    ghub::device_inventory::DeviceIdentityViewModel deviceIdentityViewModel{readDeviceIdentity};
    // Leituras de estado HID++ (ADR-0025): brilho e report rate do G915, DPI e report rate do G703, bandas do EQ do
    // G733. Só leitura, só por clique em read() de cada ViewModel; nada é aberto nem lido na construção. Mesma
    // ordem de destruição da identidade: ViewModel, caso de uso, fonte.
    ghub::lighting::infrastructure::HidppKeyboardStatusSource hidppKeyboardStatusSource{
        ghub::platform::hidpp::HidppNodeLocator{}, steadyClock};
    const ghub::lighting::application::ReadKeyboardStatus readKeyboardStatus{hidppKeyboardStatusSource, accessRegistry};
    ghub::lighting::presentation::KeyboardStatusViewModel keyboardStatusViewModel{readKeyboardStatus};
    ghub::pointer::infrastructure::HidppPointerSensorSource hidppPointerSensorSource{
        ghub::platform::hidpp::HidppNodeLocator{}, steadyClock};
    const ghub::pointer::ReadPointerSensorStatus readPointerSensorStatus{hidppPointerSensorSource, accessRegistry};
    ghub::pointer::PointerSensorStatusViewModel pointerStatusViewModel{readPointerSensorStatus};
    ghub::headset::infrastructure::HidppHeadsetStatusSource hidppHeadsetStatusSource{
        ghub::platform::hidpp::HidppNodeLocator{}, steadyClock};
    const ghub::headset::ReadHeadsetStatus readHeadsetStatus{hidppHeadsetStatusSource, accessRegistry};
    ghub::headset::HeadsetStatusViewModel headsetStatusViewModel{readHeadsetStatus};
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
    // Mouse (ADR-0027): a configuração do G703 é lida do setor onboard ativo por HID++ próprio (sem ratbagd), só por
    // clique em PointerViewModel::read(); nada é lido na construção. Ordem de destruição (inversa à declaração):
    // ViewModel, leitor (espera a leitura em andamento), caso de uso, backend, conector e, por fim, o registro de
    // leases e o probe (declarados acima).
    ghub::pointer::infrastructure::G703OnboardConnector g703OnboardConnector{
        ghub::pointer::infrastructure::G703NodeLocator{}, steadyClock};
    ghub::pointer::infrastructure::OnboardPointerBackend onboardPointerBackend{g703OnboardConnector};
    auto pointerInterface = ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:c539");
    if (!pointerInterface) {
        qCritical("pointer: identificador de interface inválido: %s",
                  qPrintable(QString::fromStdString(pointerInterface.error().detail)));
        return EXIT_FAILURE;
    }
    const ghub::pointer::ReadPointerConfiguration readPointerConfiguration{onboardPointerBackend, externalProcessProbe,
                                                                           accessRegistry, *pointerInterface};
    ghub::pointer::BackgroundPointerReader pointerReader{readPointerConfiguration};
    ghub::pointer::PointerViewModel pointerViewModel{pointerReader};
    QObject::connect(&pointerReader, &ghub::pointer::BackgroundPointerReader::readFinished, &pointerViewModel,
                     &ghub::pointer::PointerViewModel::onReadFinished);
    // Memória onboard do G703 (ADR-0017, ADR-0027): HID++ próprio, só por clique em G703OnboardController (ler,
    // gravar, restaurar); nada é aberto nem escrito na construção. Ordem de destruição (inversa à declaração): o
    // controlador primeiro (espera a operação em andamento), depois o caso de uso e a cópia de segurança.
    const auto g703BackupStore = ghub::onboard_memory::makeFileOnboardBackupStore(
        std::filesystem::path{(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                               QStringLiteral("/ghub-linux/backup"))
                                  .toStdString()},
        "g703-4086-sector-");
    const ghub::pointer::G703OnboardSettings g703OnboardSettings{externalProcessProbe, accessRegistry,
                                                                 g703OnboardConnector, *g703BackupStore};
    ghub::pointer::G703OnboardController g703OnboardController{g703OnboardSettings};
    // Teclado (ADR-0013): o modo ao vivo do G915 só é aberto por clique em KeyboardLightingController::startLive();
    // nada é aberto nem escrito na construção. Ordem de destruição (inversa à declaração): o controlador primeiro
    // (espera a operação em andamento e devolve o controle ao firmware), depois o caso de uso, o adaptador e,
    // por fim, o registro de leases e o relógio (declarados acima).
    ghub::lighting::infrastructure::G915HidppLighting g915Lighting{ghub::platform::hidpp::HidppNodeLocator{},
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
    // EQ do host (ADR-0016): nada conecta ao PipeWire na construção nem no smoke test. Ordem de destruição:
    // controlador (desliga o EQ e restaura a saída padrão), caso de uso e, por fim, o grafo (desconecta).
    ghub::audio_processing::PipeWireAudioGraph audioGraph;
    ghub::audio_processing::HostEqualizer hostEqualizer{audioGraph};
    ghub::audio_processing::HostEqualizerController hostEqualizerController{hostEqualizer, audioGraph};
    // Redução de ruído do microfone (ADR-0018): mesma conexão; independente do EQ; só por clique.
    ghub::audio_processing::HostNoiseSuppression hostNoiseSuppression{audioGraph};
    ghub::audio_processing::NoiseSuppressionController noiseSuppressionController{hostNoiseSuppression, audioGraph};
    // Perfis (ADR-0020): salvar, aplicar e remover só por clique em ProfilesController; load() só lê o arquivo e nada
    // é aplicado na abertura. A câmera usa um backend V4L2 PRÓPRIO (o do BackgroundCameraReader roda em outra thread)
    // e o mesmo SetCameraControl da seção Câmera (lease compartilhado). Ordem de destruição (inversa à declaração):
    // o controlador primeiro (espera a operação em andamento), antes do controlador do EQ e dos casos de uso.
    ghub::camera::V4l2CameraBackend profilesCameraBackend{brio100, v4l2Api};
    const ghub::camera::CameraSettings cameraSettings{profilesCameraBackend, setCameraControl};
    ghub::profiles::CameraSettingsAdapter profilesCameraAdapter{cameraSettings};
    ghub::profiles::EqualizerSettingsAdapter profilesEqualizerAdapter{hostEqualizerController};
    ghub::profiles::JsonProfileRepository profileRepository{ghub::profiles::JsonProfileRepository::defaultFilePath()};
    ghub::profiles::ProfileBook profileBook{profileRepository, profilesCameraAdapter};
    const ghub::profiles::ApplyProfile applyProfile{profilesCameraAdapter, profilesEqualizerAdapter};
    // Troca automática por aplicativo (ADR-0022): a fonte só é iniciada com a opção ligada pelo usuário (e persistida);
    // declarada ANTES do controlador, que a para no destrutor. No smoke test a fonte é inerte (nunca toca no KWin
    // nem no barramento de sessão).
    const std::unique_ptr<ghub::profiles::ActiveApplicationSource> activeApplicationSource =
        smokeTestRun ? std::unique_ptr<ghub::profiles::ActiveApplicationSource>{std::make_unique<
                           ghub::profiles::InactiveApplicationSource>()}
                     : std::make_unique<ghub::profiles::KWinActiveApplicationSource>(
                           QDBusConnection::sessionBus(), qEnvironmentVariable("XDG_RUNTIME_DIR"));
    ghub::profiles::ProfilesController profilesController{profileBook, applyProfile, profilesEqualizerAdapter,
                                                          *activeApplicationSource};
    // Depois de aplicar um perfil com câmera, a janela relê a câmera.
    QObject::connect(&profilesController, &ghub::profiles::ProfilesController::cameraApplied, &cameraReader,
                     &ghub::camera::BackgroundCameraReader::refresh);
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

    // Diagnóstico (INC-013 R13–R15): o relatório é montado a partir do que está exibido e gravado só por clique
    // (XdgReportWriter). A saúde dos backends é montada aqui, no composition root (R15): o módulo `diagnostics`
    // não conhece os outros módulos. Declarado antes do engine (destruído depois dele).
    using ghub::diagnostics::BackendHealth;
    const ghub::diagnostics::XdgReportWriter reportWriter;
    const ghub::diagnostics::ExportDiagnosticReport exportDiagnosticReport{reportWriter};
    const std::string platform =
        (QSysInfo::prettyProductName() + QStringLiteral(" / ") + QGuiApplication::platformName()).toStdString();
    ghub::diagnostics::DiagnosticsViewModel diagnosticsViewModel{
        diagnosticLog, exportDiagnosticReport, GHUB_VERSION, platform, [&]() -> std::vector<BackendHealth> {
            std::vector<BackendHealth> health;
            health.push_back({"Bandeja do sistema", trayAvailable,
                              trayAvailable ? "ícone disponível nesta sessão" : "sem bandeja nesta sessão"});
            const bool uevent = batteryMonitor != nullptr;
            health.push_back({"Notificação por uevent do kernel", uevent,
                              uevent ? "socket netlink aberto" : "socket netlink indisponível"});
            const bool headsetControl = !headsetControlExecutable.isEmpty();
            health.push_back({"HeadsetControl (G733)", headsetControl,
                              headsetControl ? headsetControlExecutable.toStdString() : "não encontrado no PATH"});
            const bool ratbagd = serviceRegistered(QStringLiteral("org.freedesktop.ratbag1"));
            health.push_back(
                {"ratbagd (G703)", ratbagd, ratbagd ? "serviço presente no barramento de sistema" : "serviço ausente"});
            const bool pipeWire = pipeWireSocketExists();
            health.push_back(
                {"PipeWire (áudio no host)", pipeWire, pipeWire ? "servidor do usuário presente" : "servidor ausente"});
            const bool logind = serviceRegistered(QStringLiteral("org.freedesktop.login1"));
            health.push_back({"logind (suspensão)", logind,
                              logind ? "serviço presente no barramento de sistema" : "serviço ausente"});
            for (const auto& detection : listDetectedDevices.execute()) {
                const bool detected = detection.paths.has_value() && !detection.paths->empty();
                health.push_back({std::string{ghub::device_inventory::modelName(detection.model)}, detected,
                                  detected ? "detectado" : "não detectado"});
            }
            return health;
        }};

    // Declarado depois dos objetos acima: é destruído antes deles, sem referências pendentes no QML.
    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("batteryViewModel"), QVariant::fromValue(&batteryViewModel)},
        {QStringLiteral("deviceInventoryViewModel"), QVariant::fromValue(&deviceInventoryViewModel)},
        {QStringLiteral("deviceIdentityViewModel"), QVariant::fromValue(&deviceIdentityViewModel)},
        {QStringLiteral("keyboardStatusViewModel"), QVariant::fromValue(&keyboardStatusViewModel)},
        {QStringLiteral("pointerStatusViewModel"), QVariant::fromValue(&pointerStatusViewModel)},
        {QStringLiteral("headsetStatusViewModel"), QVariant::fromValue(&headsetStatusViewModel)},
        {QStringLiteral("cameraViewModel"), QVariant::fromValue(&cameraViewModel)},
        {QStringLiteral("cameraPreviewViewModel"), QVariant::fromValue(&cameraPreviewViewModel)},
        {QStringLiteral("cameraControlEditor"), QVariant::fromValue(&cameraControlEditor)},
        {QStringLiteral("pointerViewModel"), QVariant::fromValue(&pointerViewModel)},
        {QStringLiteral("g703OnboardController"), QVariant::fromValue(&g703OnboardController)},
        {QStringLiteral("keyboardLightingController"), QVariant::fromValue(&keyboardLightingController)},
        {QStringLiteral("headsetSettingsController"), QVariant::fromValue(&headsetSettingsController)},
        {QStringLiteral("hostEqualizerController"), QVariant::fromValue(&hostEqualizerController)},
        {QStringLiteral("noiseSuppressionController"), QVariant::fromValue(&noiseSuppressionController)},
        {QStringLiteral("profilesController"), QVariant::fromValue(&profilesController)},
        {QStringLiteral("desktopController"), QVariant::fromValue(&desktopController)},
        {QStringLiteral("diagnosticsViewModel"), QVariant::fromValue(&diagnosticsViewModel)},
    });
    profilesController.load(); // só lê o arquivo de perfis; nada é aplicado
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    // No teste de fumaça qualquer aviso do QML (ex.: TypeError em binding) é falha.
    bool qmlWarned = false;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&qmlWarned](const QList<QQmlError>&) { qmlWarned = true; });
    engine.loadFromModule("GHub", "Main");

    if (smokeTestRun) {
        return engine.rootObjects().isEmpty() || qmlWarned ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    // Bandeja do sistema (INC-013 R2): criada só fora do smoke test e com bandeja disponível; sem ela, nenhum
    // ícone e o app segue como antes, com um aviso. O adaptador é destruído ao sair (esconde o ícone).
    std::unique_ptr<ghub::app::SystemTrayIcon> systemTray;
    if (desktopController.trayAvailable()) {
        systemTray = std::make_unique<ghub::app::SystemTrayIcon>(desktopController);
    } else {
        qWarning("app: bandeja do sistema indisponível; fechar a janela encerra o aplicativo");
    }
    // "Mostrar janela"/ativação do ícone trazem a janela de volta (INC-013 R2). O objeto raiz do QML sobrevive
    // ao engine; o lambda o mantém por ponteiro só enquanto o engine vive (conexão destruída com o controller).
    QObject* const mainWindow = engine.rootObjects().constFirst();
    QObject::connect(&desktopController, &ghub::app::DesktopController::showWindowRequested, mainWindow, [mainWindow] {
        QMetaObject::invokeMethod(mainWindow, "show");
        QMetaObject::invokeMethod(mainWindow, "raise");
        QMetaObject::invokeMethod(mainWindow, "requestActivate");
    });
    // ADR-0014 (R16): antes de suspender, devolve o controle do G915 ao firmware sob um inhibitor "delay" do logind.
    // Fora do smoke test, para o CTest não tocar no logind real. Sem logind: só um aviso.
    ghub::device_access::LogindSleepMonitor sleepMonitor{QDBusConnection::systemBus()};
    QObject::connect(&sleepMonitor, &ghub::device_access::LogindSleepMonitor::aboutToSleep, &keyboardLightingController,
                     &ghub::lighting::presentation::KeyboardLightingController::releaseForSleep);
    if (!sleepMonitor.start()) {
        qWarning("app: aviso de suspensão do logind indisponível (%s); o modo ao vivo não é desligado antes de "
                 "suspender",
                 qPrintable(sleepMonitor.lastError()));
    }
    return QCoreApplication::exec();
}
