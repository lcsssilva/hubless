#pragma once

#include "modules/device_inventory/api/DeviceInventory.hpp"
#include "shared_kernel/Result.hpp"

#include <linux/videodev2.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ghub::camera {

// Localização dos nós V4L2 de uma câmera USB, compartilhada pelo backend de leitura e pelo de captura.

// Nomes "video<N>" cujo dispositivo USB (modalias em `<root>/<nome>/device/modalias`) é o alvo, em ordem numérica.
// Erro BackendUnavailable se `root` não puder ser listado.
shared_kernel::Result<std::vector<std::string>> findCandidateNodes(const std::filesystem::path& root,
                                                                   device_inventory::UsbId target);

// Capacidades deste nó: `device_caps` quando o driver o informa, senão `capabilities`.
std::uint32_t nodeCapabilities(const v4l2_capability& capability);

} // namespace ghub::camera
