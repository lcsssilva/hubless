// Teste físico opt-in (TEST_STRATEGY.md §1, ADR-0010 decisão 9): ESCRITA de controles na Brio 100 real.
// Só roda com GHUB_HW_TESTS=1 E GHUB_HW_CAMERA_WRITE=1 e a Brio 100 ligada. Um controle por vez: lê o original,
// grava um valor válido diferente, relê, restaura o original. Os originais voltam em ordem inversa (dependente antes
// do mestre) também quando uma asserção falha (guarda RAII). Registrar o resultado no VERIFICATION_LOG.
#include "modules/camera/application/SetCameraControl.hpp"
#include "modules/camera/infrastructure/LinuxV4l2Api.hpp"
#include "modules/camera/infrastructure/V4l2CameraBackend.hpp"
#include "modules/camera/infrastructure/V4l2ControlAccess.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using ghub::camera::CameraControl;
using ghub::camera::ControlKind;
using ghub::camera::LinuxV4l2Api;
using ghub::camera::SetCameraControl;
using ghub::camera::V4l2CameraBackend;
using ghub::camera::V4l2ControlAccess;
using ghub::device_access::AccessRegistry;
using ghub::device_access::PhysicalInterfaceId;
using ghub::device_access::SteadyClock;

namespace {

constexpr ghub::device_inventory::UsbId brio100{.vendorId = 0x046d, .productId = 0x094c};

bool envIsOne(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
}

// Controle automático que precisa ficar em manual para o dependente aceitar escrita.
struct Master {
    std::string_view dependent;
    std::string_view master;
    std::int64_t manualValue;
};
constexpr std::array<Master, 2> masters{
    Master{.dependent = "White Balance Temperature", .master = "White Balance, Automatic", .manualValue = 0},
    Master{.dependent = "Exposure Time, Absolute", .master = "Auto Exposure", .manualValue = 1}, // 1 = Manual Mode
};

const Master* masterOf(std::string_view dependent)
{
    for (const Master& entry : masters) {
        if (entry.dependent == dependent) {
            return &entry;
        }
    }
    return nullptr;
}

const CameraControl* findByName(const std::vector<CameraControl>& controls, std::string_view name)
{
    for (const CameraControl& control : controls) {
        if (control.name() == name) {
            return &control;
        }
    }
    return nullptr;
}

// Valor válido diferente do original; vazio se não houver.
std::optional<std::int64_t> differentValue(const CameraControl& control, std::int64_t original)
{
    switch (control.kind()) {
    case ControlKind::Integer:
        if (original + control.step() <= control.maximum()) {
            return original + control.step();
        }
        if (original - control.step() >= control.minimum()) {
            return original - control.step();
        }
        return std::nullopt;
    case ControlKind::Boolean:
        return original == 0 ? 1 : 0;
    case ControlKind::Menu:
        for (const auto& option : control.options()) {
            if (option.value != original) {
                return option.value;
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

// Regrava os valores originais em ordem inversa à das alterações, no destrutor ou quando pedido.
class RestoreGuard {
public:
    explicit RestoreGuard(const SetCameraControl& setCameraControl) : set_(&setCameraControl) {}
    RestoreGuard(const RestoreGuard&) = delete;
    RestoreGuard& operator=(const RestoreGuard&) = delete;
    RestoreGuard(RestoreGuard&&) = delete;
    RestoreGuard& operator=(RestoreGuard&&) = delete;
    ~RestoreGuard() { restoreAll(); }

    // Registra ANTES de gravar, para que uma falha no meio da escrita também seja desfeita.
    void remember(const CameraControl& control, std::int64_t original)
    {
        steps_.push_back({.id = control.id(), .name = control.name(), .original = original});
    }

    void restoreAll()
    {
        while (!steps_.empty()) {
            const Step step = steps_.back();
            steps_.pop_back();
            const auto restored = set_->setValue(step.id, step.original);
            if (restored.has_value()) {
                std::cout << "  restaurado '" << step.name << "' para " << step.original << " (relido "
                          << restored->currentValue().value_or(-1) << ")\n";
            } else {
                ADD_FAILURE() << "falha ao restaurar '" << step.name << "' para " << step.original << ": "
                              << restored.error().detail;
            }
        }
    }

private:
    struct Step {
        std::uint32_t id;
        std::string name;
        std::int64_t original;
    };

    const SetCameraControl* set_;
    std::vector<Step> steps_;
};

// Gain, Exposure Time Absolute e White Balance Temperature são reajustados pelo firmware quando o respectivo
// automático está ligado (evidence/2026-10-13-brio100-preview-stream-inc006.md): não se compara o valor final.
bool firmwareMayChange(const std::vector<CameraControl>& finalControls, std::string_view name)
{
    if (name == "Gain") {
        return true;
    }
    const auto automatic = [&finalControls](std::string_view masterName, std::int64_t autoValue, bool equals) {
        const CameraControl* master = findByName(finalControls, masterName);
        if (master == nullptr || !master->currentValue().has_value()) {
            return false;
        }
        return (*master->currentValue() == autoValue) == equals;
    };
    if (name == "Exposure Time, Absolute") {
        return automatic("Auto Exposure", 1, false); // qualquer modo diferente de Manual
    }
    if (name == "White Balance Temperature") {
        return automatic("White Balance, Automatic", 1, true);
    }
    return false;
}

} // namespace

TEST(CameraHardware, WritesAndRestoresBrio100Controls)
{
    if (!envIsOne("GHUB_HW_TESTS") || !envIsOne("GHUB_HW_CAMERA_WRITE")) {
        GTEST_SKIP() << "teste físico de ESCRITA: defina GHUB_HW_TESTS=1 e GHUB_HW_CAMERA_WRITE=1";
    }
    LinuxV4l2Api api;
    V4l2CameraBackend backend{brio100, api};
    const auto before = backend.readCamera();
    ASSERT_TRUE(before.has_value()) << before.error().detail;
    const auto& beforeCamera = before.value();
    if (!beforeCamera.has_value()) {
        FAIL() << "Brio 100 não detectada em /sys/class/video4linux";
    }
    const std::vector<CameraControl> originals = beforeCamera.value().controls;
    ASSERT_FALSE(originals.empty());

    V4l2ControlAccess access{brio100, api};
    const SteadyClock clock;
    AccessRegistry registry{clock};
    const SetCameraControl setCameraControl{access, registry, PhysicalInterfaceId::create("v4l2:046d:094c").value()};

    {
        RestoreGuard guard{setCameraControl};
        for (const CameraControl& control : originals) {
            std::cout << "controle '" << control.name() << "'\n";
            const auto currentValue = control.currentValue();
            if (control.readOnly() || !currentValue.has_value()) {
                std::cout << "  ignorado (somente leitura ou sem valor atual)\n";
                continue;
            }
            const std::int64_t original = *currentValue;
            if (control.inactive()) {
                const Master* master = masterOf(control.name());
                const CameraControl* masterControl =
                    master == nullptr ? nullptr : findByName(originals, master->master);
                const auto masterOriginal = masterControl == nullptr ? std::nullopt : masterControl->currentValue();
                if (!masterOriginal.has_value()) {
                    std::cout << "  ignorado (inativo e sem mestre conhecido)\n";
                    continue;
                }
                std::cout << "  mestre '" << master->master << "' para manual (" << master->manualValue << ")\n";
                guard.remember(*masterControl, *masterOriginal);
                const auto manual = setCameraControl.setValue(masterControl->id(), master->manualValue);
                ASSERT_TRUE(manual.has_value()) << manual.error().detail;
            }
            // O caso de uso relê e valida contra o estado atual, já com o mestre em manual.
            const auto wanted = differentValue(control, original);
            if (!wanted.has_value()) {
                std::cout << "  sem valor alternativo; ignorado\n";
                guard.restoreAll();
                continue;
            }
            guard.remember(control, original);
            const auto written = setCameraControl.setValue(control.id(), *wanted);
            ASSERT_TRUE(written.has_value()) << control.name() << ": " << written.error().detail;
            std::cout << "  original=" << original << " gravado=" << *wanted
                      << " relido=" << written->currentValue().value_or(-1) << "\n";
            EXPECT_EQ(written->currentValue(), wanted) << control.name();
            guard.restoreAll(); // dependente antes do mestre
        }
    }

    const auto after = backend.readCamera();
    ASSERT_TRUE(after.has_value()) << after.error().detail;
    const auto& afterCamera = after.value();
    if (!afterCamera.has_value()) {
        FAIL() << "Brio 100 sumiu depois do teste";
    }
    const auto& finalControls = afterCamera.value().controls;
    for (const CameraControl& control : originals) {
        const auto originalValue = control.currentValue();
        if (control.readOnly() || !originalValue.has_value()) {
            continue;
        }
        const CameraControl* now = findByName(finalControls, control.name());
        ASSERT_NE(now, nullptr) << control.name();
        if (firmwareMayChange(finalControls, control.name())) {
            std::cout << "final '" << control.name() << "': original=" << *control.currentValue()
                      << " atual=" << now->currentValue().value_or(-1) << " (firmware pode reajustar; não comparado)\n";
            continue;
        }
        EXPECT_EQ(now->currentValue(), control.currentValue()) << control.name() << " não voltou ao original";
    }
}
