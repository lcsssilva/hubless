#pragma once

#include "modules/onboard_memory/api/OnboardMemory.hpp"
#include "shared_kernel/Clock.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <utility>

namespace ghub::pointer::infrastructure {

// Nó hidraw do receptor por onde falar HID++ com o G703 e o índice de dispositivo nos relatórios.
struct G703Node {
    std::filesystem::path devicePath; // /dev/hidrawN (só o caminho; nada é aberto aqui)
    std::uint8_t deviceIndex;

    friend bool operator==(const G703Node&, const G703Node&) = default;
};

// Descobre o nó por /sys/class/hidraw/*/device/uevent (HID_ID + HID_PHYS). Só lê sysfs. O receptor é o 046d:c539
// interface 2; o G703 (046d:4086) aparece como filho com HID_PHYS "<fisico do receptor>/input2:N", N em 1..6, e N
// é o índice de dispositivo.
class G703NodeLocator {
public:
    explicit G703NodeLocator(std::filesystem::path sysRoot = "/sys", std::filesystem::path devRoot = "/dev")
        : sysRoot_(std::move(sysRoot)), devRoot_(std::move(devRoot))
    {
    }

    [[nodiscard]] shared_kernel::Result<G703Node> locate() const;

private:
    std::filesystem::path sysRoot_;
    std::filesystem::path devRoot_;
};

// Abre o hidraw do G703 só durante a operação e entrega a memória onboard HID++ do módulo onboard_memory, com a
// G703OnboardPolicy (fecha ao destruir).
class G703OnboardConnector final : public onboard_memory::OnboardMemoryConnector {
public:
    // O relógio precisa sobreviver ao conector e às memórias abertas por ele.
    G703OnboardConnector(G703NodeLocator locator, const shared_kernel::Clock& clock)
        : locator_(std::move(locator)), clock_(&clock)
    {
    }

    [[nodiscard]] shared_kernel::Result<std::unique_ptr<onboard_memory::OnboardMemory>> open() override;

private:
    G703NodeLocator locator_;
    const shared_kernel::Clock* clock_;
};

} // namespace ghub::pointer::infrastructure
