#pragma once

#include "modules/camera/application/VideoCapture.hpp"
#include "modules/camera/infrastructure/V4l2Api.hpp"
#include "modules/device_inventory/api/DeviceInventory.hpp"

#include <filesystem>
#include <memory>

namespace ghub::camera {

// Captura de vídeo via V4L2 (ADR-0009): streaming por mmap do nó de captura da câmera alvo.
// Não escreve controles nem intervalo de quadros (nenhum ioctl de escrita de controle ou de parâmetros): só negocia o
// formato (VIDIOC_S_FMT) e liga o stream. `api` deve sobreviver a este objeto e às sessões que ele criou. Uso por uma
// thread por vez (a thread de trabalho da pré-visualização): nem este objeto nem as sessões são sincronizados.
class V4l2VideoCapture final : public VideoCapture {
public:
    V4l2VideoCapture(device_inventory::UsbId target, V4l2Api& api,
                     std::filesystem::path sysfsRoot = "/sys/class/video4linux",
                     std::filesystem::path devRoot = "/dev");

    // Erros: BackendUnavailable (câmera não detectada, open negado, ioctl ou mmap falhou, o driver não aceitou
    // exatamente o modo pedido), DeviceBusy (-EBUSY: outro programa transmite) e MalformedData (buffer incoerente).
    // Nunca devolve sessão parcial: qualquer falha desfaz o que já havia sido feito.
    [[nodiscard]] shared_kernel::Result<std::unique_ptr<VideoCaptureSession>> start(const PreviewMode& mode) override;

private:
    device_inventory::UsbId target_;
    V4l2Api* api_;
    std::filesystem::path sysfsRoot_;
    std::filesystem::path devRoot_;
};

} // namespace ghub::camera
