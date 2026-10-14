// Teste físico opt-in (TEST_STRATEGY.md §1): pré-visualização da Brio 100 real. ATENÇÃO: este teste INICIA O STREAM
// da câmera (acende o LED). No dispositivo ele faz, e só isto:
//   1. lê a câmera (open O_RDONLY + ioctls de consulta) e anota o valor de todos os controles;
//   2. abre o nó de captura com O_RDWR (exigência do mmap) e executa VIDIOC_S_FMT (modo escolhido por
//      selectPreviewMode), VIDIOC_REQBUFS, VIDIOC_QUERYBUF, VIDIOC_QBUF e VIDIOC_STREAMON;
//   3. lê até 30 quadros (VIDIOC_DQBUF/QBUF, prazo de 1 s cada, no máximo 10 s no total);
//   4. destrói a sessão (VIDIOC_STREAMOFF, munmap, close);
//   5. lê a câmera de novo e confere que nenhum controle de configuração mudou de valor (os que o firmware
//      ajusta sozinho com os modos automáticos ligados são só impressos).
// Não escreve controles (nada de S_CTRL/S_EXT_CTRLS) nem o intervalo de quadros (S_PARM).
// Só o agente principal o executa, com autorização do usuário na sessão: exige GHUB_HW_TESTS=1 e, por iniciar o
// stream, também GHUB_HW_CAMERA_STREAM=1. Registrar o resultado no VERIFICATION_LOG.
#include "modules/camera/infrastructure/LinuxV4l2Api.hpp"
#include "modules/camera/infrastructure/V4l2CameraBackend.hpp"
#include "modules/camera/infrastructure/V4l2VideoCapture.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using ghub::camera::CameraDescription;
using ghub::camera::LinuxV4l2Api;
using ghub::camera::V4l2CameraBackend;
using ghub::camera::V4l2VideoCapture;

namespace {

bool enabled(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
}

// Controles que o firmware ajusta sozinho durante o stream e que, por isso, não entram na comparação:
// os marcados como inativos (o modo automático correspondente está ligado: tempo de exposição com
// exposição automática, temperatura com balanço de branco automático) e o ganho, que a exposição
// automática também altera sem ficar inativo. Observado na Brio 100 em 2026-10-13
// (evidence/2026-10-13-brio100-preview-stream-inc006.md): só esses três mudaram.
bool firmwareDriven(const ghub::camera::CameraControl& control)
{
    return control.inactive() || control.name() == "Gain";
}

// Valor atual de cada controle de configuração, por nome (sem valor = nullopt).
std::map<std::string, std::optional<std::int64_t>> settingValues(const CameraDescription& camera)
{
    std::map<std::string, std::optional<std::int64_t>> values;
    for (const auto& control : camera.controls) {
        if (!firmwareDriven(control)) {
            values[control.name()] = control.currentValue();
        }
    }
    return values;
}

// Imprime os controles ajustados pelo firmware (informativo; não é critério do teste).
void printFirmwareDriven(std::string_view label, const CameraDescription& camera)
{
    std::cout << label << ":";
    for (const auto& control : camera.controls) {
        if (firmwareDriven(control)) {
            std::cout << " " << control.name() << "=";
            if (const auto value = control.currentValue(); value.has_value()) {
                std::cout << *value;
            } else {
                std::cout << "?";
            }
            std::cout << ";";
        }
    }
    std::cout << "\n";
}

} // namespace

TEST(CameraHardware, StreamsPreviewFramesFromBrio100)
{
    if (!enabled("GHUB_HW_TESTS") || !enabled("GHUB_HW_CAMERA_STREAM")) {
        GTEST_SKIP() << "teste físico que inicia o stream: defina GHUB_HW_TESTS=1 e GHUB_HW_CAMERA_STREAM=1";
    }
    constexpr ghub::device_inventory::UsbId brio{.vendorId = 0x046d, .productId = 0x094c};
    LinuxV4l2Api api;
    V4l2CameraBackend backend{brio, api};

    const auto before = backend.readCamera();
    ASSERT_TRUE(before.has_value()) << before.error().detail;
    if (!before->has_value()) {
        FAIL() << "Brio 100 não detectada em /sys/class/video4linux";
    }
    const auto& cameraBefore = **before;
    const auto settingsBefore = settingValues(cameraBefore);
    printFirmwareDriven("ajustados pelo firmware, antes", cameraBefore);

    const auto selectedMode = ghub::camera::selectPreviewMode(cameraBefore.modes);
    if (!selectedMode.has_value()) {
        FAIL() << "nenhum modo compatível com a pré-visualização";
    }
    const auto& mode = *selectedMode;
    std::cout << "modo: " << mode.format.fourcc() << " " << mode.width << "x" << mode.height << "\n";

    constexpr int maxFrames = 30;
    constexpr auto frameTimeout = std::chrono::seconds{1};
    constexpr auto totalBudget = std::chrono::seconds{10};
    int frames = 0;
    std::size_t totalBytes = 0;
    int emptyReads = 0;                // leituras sem quadro (prazo esgotado ou quadro descartado)
    std::vector<long long> arrivalsMs; // instante de cada quadro desde o STREAMON, em ms
    std::chrono::steady_clock::duration elapsed{};
    {
        V4l2VideoCapture capture{brio, api};
        auto session = capture.start(mode);
        ASSERT_TRUE(session.has_value()) << session.error().detail;

        const auto begin = std::chrono::steady_clock::now();
        std::vector<std::byte> frame;
        while (frames < maxFrames && std::chrono::steady_clock::now() - begin < totalBudget) {
            const auto got = session.value()->readFrame(frameTimeout, frame);
            ASSERT_TRUE(got.has_value()) << got.error().detail;
            if (!*got) {
                ++emptyReads;
                continue;
            }
            arrivalsMs.push_back(
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin)
                    .count());
            if (mode.format.fourcc() == "MJPG") {
                ASSERT_GE(frame.size(), 2U);
                EXPECT_EQ(frame[0], std::byte{0xFF}) << "quadro " << frames;
                EXPECT_EQ(frame[1], std::byte{0xD8}) << "quadro " << frames;
            }
            ++frames;
            totalBytes += frame.size();
        }
        elapsed = std::chrono::steady_clock::now() - begin;
    } // a sessão é destruída aqui: STREAMOFF, munmap e close

    const double seconds = std::chrono::duration<double>(elapsed).count();
    std::cout << "quadros: " << frames << "\n";
    if (frames > 0) {
        std::cout << "tamanho médio: " << totalBytes / static_cast<std::size_t>(frames) << " bytes\n";
        std::cout << "fps medido: " << (seconds > 0 ? static_cast<double>(frames) / seconds : 0.0) << "\n";
    }
    std::cout << "leituras sem quadro: " << emptyReads << "\n";
    std::cout << "chegada dos quadros (ms):";
    for (const long long arrival : arrivalsMs) {
        std::cout << " " << arrival;
    }
    std::cout << "\n";
    EXPECT_GT(frames, 0) << "nenhum quadro válido chegou";

    const auto after = backend.readCamera();
    ASSERT_TRUE(after.has_value()) << after.error().detail;
    if (!after->has_value()) {
        FAIL() << "Brio 100 sumiu depois do stream";
    }
    printFirmwareDriven("ajustados pelo firmware, depois", **after);
    EXPECT_EQ(settingValues(**after), settingsBefore)
        << "o valor de algum controle de configuração mudou depois do stream";
}
