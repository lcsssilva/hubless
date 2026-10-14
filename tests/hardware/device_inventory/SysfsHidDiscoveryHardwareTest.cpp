// Teste físico opt-in (TEST_STRATEGY.md §1): somente leitura dos nomes de /sys/bus/hid/devices real.
// Executar com GHUB_HW_TESTS=1 e G915 + G703 + G733 ligados; registrar resultado no VERIFICATION_LOG.
#include "modules/device_inventory/application/ListDetectedDevices.hpp"
#include "modules/device_inventory/infrastructure/SysfsHidDiscovery.hpp"
#include "modules/device_inventory/infrastructure/SysfsVideoDiscovery.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>

using ghub::device_inventory::ConnectionPath;
using ghub::device_inventory::DeviceModel;
using ghub::device_inventory::ListDetectedDevices;
using ghub::device_inventory::SysfsHidDiscovery;
using ghub::device_inventory::SysfsVideoDiscovery;

TEST(DeviceInventoryHardware, DetectsG915G703AndG733FromSysfs)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* enabled = std::getenv("GHUB_HW_TESTS");
    if (enabled == nullptr || std::string_view{enabled} != "1") {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1";
    }
    SysfsHidDiscovery discovery;

    const auto detections = ListDetectedDevices{{&discovery}}.execute();

    for (const auto& detection : detections) {
        ASSERT_TRUE(detection.paths.has_value());
        std::cout << "dispositivo " << ghub::device_inventory::modelName(detection.model) << ": "
                  << detection.paths->size() << " caminho(s)\n";
    }
    const auto detectedVia = [&detections](DeviceModel model, ConnectionPath path) {
        const auto found = std::ranges::find(detections, model, &ghub::device_inventory::DeviceDetection::model);
        return found != detections.end() && found->paths.has_value() && std::ranges::contains(*found->paths, path);
    };
    EXPECT_TRUE(detectedVia(DeviceModel::G915, ConnectionPath::LightspeedReceiver));
    EXPECT_TRUE(detectedVia(DeviceModel::G703, ConnectionPath::LightspeedReceiver));
    EXPECT_TRUE(detectedVia(DeviceModel::G733, ConnectionPath::UsbDongle));
}

TEST(DeviceInventoryHardware, DetectsBrio100FromVideo4linux)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* enabled = std::getenv("GHUB_HW_TESTS");
    if (enabled == nullptr || std::string_view{enabled} != "1") {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1";
    }
    SysfsHidDiscovery hidDiscovery;
    SysfsVideoDiscovery videoDiscovery;

    const auto detections = ListDetectedDevices{{&hidDiscovery, &videoDiscovery}}.execute();

    const auto found =
        std::ranges::find(detections, DeviceModel::Brio100, &ghub::device_inventory::DeviceDetection::model);
    ASSERT_NE(found, detections.end());
    ASSERT_TRUE(found->paths.has_value());
    std::cout << "dispositivo Brio 100: " << found->paths->size() << " caminho(s)\n";
    EXPECT_TRUE(std::ranges::contains(*found->paths, ConnectionPath::UsbCable));
}
