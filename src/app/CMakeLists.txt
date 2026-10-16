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

# Componentes QML de src/ui/qml (INC-014). Theme.qml é singleton; o alias mantém o nome do arquivo no recurso,
# que o qmldir de src/ui/qml usa nos testes de apresentação.
set(GHUB_QML_DIR "${PROJECT_SOURCE_DIR}/src/ui/qml")
set(GHUB_QML_FILES Main.qml DeviceCard.qml DeviceDrawing.qml BatteryIcon.qml DevicePage.qml PanelCard.qml PanelPage.qml SideTab.qml TopBarTab.qml SectionTitle.qml GHubButton.qml GHubSlider.qml GHubCheckBox.qml
    GHubSwitch.qml GHubComboBox.qml GHubSpinBox.qml GHubRadioButton.qml GHubTextField.qml ColorSwatch.qml InnerTabs.qml
    HelpIcon.qml SectionHeader.qml SwitchRow.qml BatteryBlock.qml EqualizerView.qml InfoPage.qml HeadsetLightingSection.qml
    GHubDialog.qml GHubBusyRing.qml Theme.qml)
set(GHUB_QML_PATHS)
foreach(qml_file IN LISTS GHUB_QML_FILES)
    set_source_files_properties("${GHUB_QML_DIR}/${qml_file}" PROPERTIES QT_RESOURCE_ALIAS ${qml_file})
    list(APPEND GHUB_QML_PATHS "${GHUB_QML_DIR}/${qml_file}")
endforeach()
set_source_files_properties("${GHUB_QML_DIR}/Theme.qml" PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Ícones SVG próprios (INC-014 parte 5): ficam em icons/ dentro do módulo, ao lado dos QML que os usam.
file(GLOB GHUB_ICON_FILES CONFIGURE_DEPENDS "${GHUB_QML_DIR}/icons/*.svg")
set(GHUB_ICON_PATHS)
foreach(icon_file IN LISTS GHUB_ICON_FILES)
    get_filename_component(icon_name "${icon_file}" NAME)
    set_source_files_properties("${icon_file}" PROPERTIES QT_RESOURCE_ALIAS "icons/${icon_name}")
    list(APPEND GHUB_ICON_PATHS "${icon_file}")
endforeach()
qt_add_qml_module(ghub-linux
    URI GHub
    VERSION 1.0
    QML_FILES ${GHUB_QML_PATHS}
    RESOURCES ${GHUB_ICON_PATHS})

# Estilo Material escuro lido pelo Qt Quick Controls na raiz do recurso (INC-014 R1); as cores vêm do singleton Theme.qml.
qt_add_resources(ghub-linux "controls_conf"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/src/ui"
    FILES "${PROJECT_SOURCE_DIR}/src/ui/qtquickcontrols2.conf")

target_link_libraries(ghub-linux PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Widgets
    ghub_app_support ghub_app_tray
    ghub_battery_infrastructure ghub_battery_uevent ghub_battery_hidpp_polling ghub_battery_presentation
    ghub_camera_infrastructure ghub_camera_background ghub_camera_preview ghub_camera_presentation
    ghub_device_access_infrastructure ghub_device_access_logind
    ghub_device_inventory_infrastructure ghub_device_inventory_presentation
    ghub_diagnostics_infrastructure ghub_diagnostics_presentation
    ghub_pointer_infrastructure ghub_pointer_background ghub_pointer_presentation ghub_pointer_hidpp
    ghub_lighting_infrastructure ghub_lighting_presentation
    ghub_headset_infrastructure ghub_headset_hidpp ghub_headset_presentation
    ghub_audio_processing_infrastructure ghub_audio_processing_presentation
    ghub_profiles_infrastructure ghub_profiles_presentation)
ghub_set_warnings(ghub-linux)
target_compile_definitions(ghub-linux PRIVATE GHUB_VERSION="${PROJECT_VERSION}")

install(TARGETS ghub-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
