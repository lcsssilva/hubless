# Testes físicos: sempre compilados, mas pulados (GTEST_SKIP) sem GHUB_HW_TESTS=1. Rótulo `hardware`.
add_executable(ghub_hardware_battery_tests battery/SysfsBatterySourceHardwareTest.cpp)
target_link_libraries(ghub_hardware_battery_tests PRIVATE ghub_battery_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_hardware_battery_tests)
gtest_discover_tests(ghub_hardware_battery_tests TEST_PREFIX "hardware.battery." PROPERTIES LABELS hardware)

add_executable(ghub_hardware_device_inventory_tests device_inventory/SysfsHidDiscoveryHardwareTest.cpp)
target_link_libraries(ghub_hardware_device_inventory_tests PRIVATE
    ghub_device_inventory_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_hardware_device_inventory_tests)
gtest_discover_tests(ghub_hardware_device_inventory_tests TEST_PREFIX "hardware.device_inventory." PROPERTIES LABELS hardware)

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_hardware_uevent_tests battery/UeventSystemTest.cpp)
    set_target_properties(ghub_hardware_uevent_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_hardware_uevent_tests PRIVATE ghub_battery_uevent Qt6::Test)
    ghub_set_warnings(ghub_hardware_uevent_tests)
    add_test(NAME hardware.battery.UeventSystemTest COMMAND ghub_hardware_uevent_tests)
    # Sem as variáveis o Qt Test só marca SKIP e sai com 0.
    set_tests_properties(hardware.battery.UeventSystemTest PROPERTIES LABELS hardware TIMEOUT 360)

    add_executable(ghub_hardware_headsetcontrol_tests battery/HeadsetControlHardwareTest.cpp)
    set_target_properties(ghub_hardware_headsetcontrol_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_hardware_headsetcontrol_tests PRIVATE
        ghub_battery_headsetcontrol ghub_device_access_infrastructure Qt6::Test)
    ghub_set_warnings(ghub_hardware_headsetcontrol_tests)
    add_test(NAME hardware.battery.HeadsetControlG733 COMMAND ghub_hardware_headsetcontrol_tests)
    set_tests_properties(hardware.battery.HeadsetControlG733 PROPERTIES LABELS hardware TIMEOUT 60)
endif()

add_executable(ghub_hardware_camera_tests
    camera/V4l2CameraBackendHardwareTest.cpp
    camera/V4l2VideoCaptureHardwareTest.cpp
    camera/V4l2ControlAccessHardwareTest.cpp)
target_link_libraries(ghub_hardware_camera_tests PRIVATE
    ghub_camera_infrastructure ghub_device_access_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_hardware_camera_tests)
gtest_discover_tests(ghub_hardware_camera_tests TEST_PREFIX "hardware.camera." PROPERTIES LABELS hardware)

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus)
    add_executable(ghub_hardware_pointer_tests pointer/RatbagPointerBackendHardwareTest.cpp)
    target_link_libraries(ghub_hardware_pointer_tests PRIVATE ghub_pointer_infrastructure Qt6::Core Qt6::DBus GTest::gtest)
    ghub_set_warnings(ghub_hardware_pointer_tests)
    # Sem GHUB_HW_TESTS=1 e GHUB_HW_RATBAG=1 o teste só marca SKIP. Ativa o ratbagd (root) no barramento de sistema.
    gtest_discover_tests(ghub_hardware_pointer_tests TEST_PREFIX "hardware.pointer." PROPERTIES LABELS hardware TIMEOUT 60)
endif()

add_executable(ghub_hardware_lighting_tests
    lighting/HidppReadHardwareTest.cpp
    lighting/G915LiveHardwareTest.cpp
    lighting/G915EffectHardwareTest.cpp
    lighting/G915PreviewHardwareTest.cpp)
target_link_libraries(ghub_hardware_lighting_tests PRIVATE
    ghub_lighting_infrastructure ghub_device_access_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_hardware_lighting_tests)
# Sem GHUB_HW_TESTS=1 e GHUB_HW_LIGHTING_READ=1 o teste só marca SKIP. Com elas: leitura HID++ (sem escrita de estado).
gtest_discover_tests(ghub_hardware_lighting_tests TEST_PREFIX "hardware.lighting." PROPERTIES LABELS hardware TIMEOUT 60)

# INC-011 fase A: leitura onboard do G703 pelo canal HID++ compartilhado e a G703OnboardPolicy (só fn0/2/4/5 da 0x8100). Sem
# GHUB_HW_TESTS=1 e GHUB_HW_POINTER_ONBOARD=1 só marca SKIP.
add_executable(ghub_hardware_pointer_onboard_tests
    pointer/G703OnboardReadHardwareTest.cpp
    # INC-011 (ADR-0017): escrita em etapas A/B/C, cada uma só com GHUB_HW_POINTER_WRITE=<etapa>.
    pointer/G703OnboardWriteHardwareTest.cpp)
target_compile_definitions(ghub_hardware_pointer_onboard_tests PRIVATE GHUB_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(ghub_hardware_pointer_onboard_tests PRIVATE
    ghub_pointer_hidpp ghub_device_access_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_hardware_pointer_onboard_tests)
gtest_discover_tests(ghub_hardware_pointer_onboard_tests TEST_PREFIX "hardware.pointer_onboard." PROPERTIES LABELS hardware TIMEOUT 60)
