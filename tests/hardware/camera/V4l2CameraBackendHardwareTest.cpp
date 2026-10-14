// Teste físico opt-in (TEST_STRATEGY.md §1): LEITURA da Brio 100 real via V4L2 (open O_RDONLY +
// ioctls de consulta). Executar com GHUB_HW_TESTS=1 e a Brio 100 ligada; registrar o resultado
// no VERIFICATION_LOG.
#include "modules/camera/infrastructure/LinuxV4l2Api.hpp"
#include "modules/camera/infrastructure/V4l2CameraBackend.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>

using ghub::camera::ControlKind;
using ghub::camera::LinuxV4l2Api;
using ghub::camera::V4l2CameraBackend;

namespace {

std::string_view kindName(ControlKind kind)
{
    switch (kind) {
    case ControlKind::Integer:
        return "Integer";
    case ControlKind::Boolean:
        return "Boolean";
    case ControlKind::Menu:
        return "Menu";
    }
    return "?";
}

} // namespace

TEST(CameraHardware, ReadsBrio100FromV4l2)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* enabled = std::getenv("GHUB_HW_TESTS");
    if (enabled == nullptr || std::string_view{enabled} != "1") {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1";
    }
    LinuxV4l2Api api;
    V4l2CameraBackend backend{{.vendorId = 0x046d, .productId = 0x094c}, api};

    const auto result = backend.readCamera();

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    if (!result->has_value()) {
        FAIL() << "Brio 100 não detectada em /sys/class/video4linux";
    }
    const auto& camera = **result;
    std::cout << "câmera: " << camera.name << "\n";
    for (const auto& mode : camera.modes) {
        std::cout << "modo " << mode.format().fourcc() << " " << mode.width() << "x" << mode.height() << " taxas:";
        for (const auto& rate : mode.frameRates()) {
            std::cout << " " << rate.frames() << "/" << rate.seconds();
        }
        std::cout << "\n";
    }
    for (const auto& control : camera.controls) {
        std::cout << "controle '" << control.name() << "' " << kindName(control.kind()) << " min=" << control.minimum()
                  << " max=" << control.maximum() << " passo=" << control.step() << " padrão=" << control.defaultValue()
                  << " valor=";
        if (control.currentValue().has_value()) {
            std::cout << *control.currentValue();
        } else {
            std::cout << "sem valor";
        }
        std::cout << " inativo=" << control.inactive() << " somente-leitura=" << control.readOnly();
        for (const auto& option : control.options()) {
            std::cout << " [" << option.value << "=" << option.label << "]";
        }
        std::cout << "\n";
    }

    EXPECT_NE(camera.name.find("Brio 100"), std::string::npos);
    const auto fullHd = std::ranges::find_if(camera.modes, [](const ghub::camera::VideoMode& mode) {
        return mode.format().fourcc() == "MJPG" && mode.width() == 1920 && mode.height() == 1080;
    });
    ASSERT_NE(fullHd, camera.modes.end());
    EXPECT_EQ(fullHd->maxFrameRate(), ghub::camera::FrameRate::create(30, 1).value());
    const auto brightness = std::ranges::find(camera.controls, "Brightness", &ghub::camera::CameraControl::name);
    ASSERT_NE(brightness, camera.controls.end());
    EXPECT_EQ(brightness->kind(), ControlKind::Integer);
    EXPECT_EQ(brightness->minimum(), 0);
    EXPECT_EQ(brightness->maximum(), 255);
}
