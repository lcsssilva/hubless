find_package(Qt6 6.12 REQUIRED COMPONENTS Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.12)

# Tratamento de sinais de encerramento, separado do main para ser testado (tests/app).
qt_add_library(ghub_app_support STATIC TerminationSignals.cpp TerminationSignals.hpp)
target_include_directories(ghub_app_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ghub_app_support PUBLIC Qt6::Core)
ghub_set_warnings(ghub_app_support)

qt_add_executable(ghub-linux main.cpp)

set_source_files_properties("${PROJECT_SOURCE_DIR}/src/ui/qml/Main.qml" PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(ghub-linux
    URI GHub
    VERSION 1.0
    QML_FILES "${PROJECT_SOURCE_DIR}/src/ui/qml/Main.qml")

target_link_libraries(ghub-linux PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick
    ghub_app_support
    ghub_battery_infrastructure ghub_battery_uevent ghub_battery_headsetcontrol ghub_battery_presentation
    ghub_camera_infrastructure ghub_camera_background ghub_camera_preview ghub_camera_presentation
    ghub_device_access_infrastructure ghub_device_access_logind
    ghub_device_inventory_infrastructure ghub_device_inventory_presentation
    ghub_pointer_infrastructure ghub_pointer_background ghub_pointer_presentation
    ghub_lighting_infrastructure ghub_lighting_presentation
    ghub_headset_infrastructure ghub_headset_presentation)
ghub_set_warnings(ghub-linux)
target_compile_definitions(ghub-linux PRIVATE GHUB_VERSION="${PROJECT_VERSION}")
