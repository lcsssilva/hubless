find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick Widgets)
qt_standard_project_setup(REQUIRES 6.12)

# Apoio do composition root (sinal de encerramento, autostart, controlador de desktop), separado do
# main para ser testado em tests/app. Sem Qt Widgets: isso fica no adaptador ghub_app_tray.
qt_add_library(ghub_app_support STATIC TerminationSignals.cpp TerminationSignals.hpp AutostartEntry.cpp
    AutostartEntry.hpp AutostartManager.cpp AutostartManager.hpp DesktopController.cpp DesktopController.hpp)
target_include_directories(ghub_app_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ghub_app_support PUBLIC Qt6::Core ghub_shared_kernel)
ghub_set_warnings(ghub_app_support)

# Bandeja do sistema: único alvo de src/app que usa Qt Widgets (QSystemTrayIcon + QMenu), separado para
# o link com Widgets não vazar para o resto do apoio e do QML.
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Widgets)
    qt_add_library(ghub_app_tray STATIC SystemTrayIcon.cpp SystemTrayIcon.hpp)
    target_link_libraries(ghub_app_tray PUBLIC ghub_app_support Qt6::Widgets)
    ghub_set_warnings(ghub_app_tray)
endif()

qt_add_executable(ghub-linux main.cpp)

set_source_files_properties("${PROJECT_SOURCE_DIR}/src/ui/qml/Main.qml" PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(ghub-linux
    URI GHub
    VERSION 1.0
    QML_FILES "${PROJECT_SOURCE_DIR}/src/ui/qml/Main.qml")

target_link_libraries(ghub-linux PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Widgets
    ghub_app_support ghub_app_tray
    ghub_battery_infrastructure ghub_battery_uevent ghub_battery_headsetcontrol ghub_battery_presentation
    ghub_camera_infrastructure ghub_camera_background ghub_camera_preview ghub_camera_presentation
    ghub_device_access_infrastructure ghub_device_access_logind
    ghub_device_inventory_infrastructure ghub_device_inventory_presentation
    ghub_diagnostics_infrastructure ghub_diagnostics_presentation
    ghub_pointer_infrastructure ghub_pointer_background ghub_pointer_presentation ghub_pointer_hidpp
    ghub_lighting_infrastructure ghub_lighting_presentation
    ghub_headset_infrastructure ghub_headset_presentation
    ghub_audio_processing_infrastructure ghub_audio_processing_presentation)
ghub_set_warnings(ghub-linux)
target_compile_definitions(ghub-linux PRIVATE GHUB_VERSION="${PROJECT_VERSION}")
