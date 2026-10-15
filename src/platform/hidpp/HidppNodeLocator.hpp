#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <utility>

namespace ghub::platform::hidpp {

// Nó hidraw por onde falar HID++ e o índice de dispositivo a usar nos relatórios.
struct HidppNode {
    std::filesystem::path devicePath; // /dev/hidrawN (apenas o caminho; nada é aberto aqui)
    std::uint8_t deviceIndex;

    friend bool operator==(const HidppNode&, const HidppNode&) = default;
};

// Descobre os nós por /sys/class/hidraw/*/device/uevent (HID_ID + HID_PHYS). Só lê sysfs.
class HidppNodeLocator {
public:
    explicit HidppNodeLocator(std::filesystem::path sysRoot = "/sys", std::filesystem::path devRoot = "/dev")
        : sysRoot_(std::move(sysRoot)), devRoot_(std::move(devRoot))
    {
    }

    // G915 (046d:407c) sem fio: nó do receptor c547 interface 2 do mesmo hub USB; índice = N de `input2:N`.
    [[nodiscard]] shared_kernel::Result<HidppNode> locateG915() const;
    // G733 (046d:0afe) interface 3; índice de dispositivo 0xFF.
    [[nodiscard]] shared_kernel::Result<HidppNode> locateG733() const;
    // G703 (046d:4086) sem fio: nó do receptor c539 interface 2 do mesmo hub USB; índice = N de `input2:N`.
    [[nodiscard]] shared_kernel::Result<HidppNode> locateG703() const;

private:
    std::filesystem::path sysRoot_;
    std::filesystem::path devRoot_;
};

} // namespace ghub::platform::hidpp
