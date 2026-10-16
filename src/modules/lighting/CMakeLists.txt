# INC-008 parte A (ADR-0013): transporte HID++ 2.0 sobre hidraw. Só infraestrutura; sem Qt, sem hidapi.
add_library(ghub_lighting_infrastructure STATIC)
target_sources(ghub_lighting_infrastructure PRIVATE
    infrastructure/LightingHidppPolicy.cpp
    infrastructure/G915PerKeyFrames.cpp
    infrastructure/G915EffectFrames.cpp
    infrastructure/G915HidppSession.cpp
    infrastructure/G915HidppLighting.cpp
    infrastructure/HidppKeyboardStatusSource.cpp)
target_link_libraries(ghub_lighting_infrastructure PUBLIC ghub_shared_kernel ghub_platform_hidpp ghub_lighting_application)
ghub_set_warnings(ghub_lighting_infrastructure)

# INC-008 R1 (ADR-0013 decisão 2): domínio de iluminação. Só STL + shared_kernel; sem Qt, sem I/O.
add_library(ghub_lighting_domain STATIC)
target_sources(ghub_lighting_domain PRIVATE
    domain/EffectParameters.cpp
    domain/KeyboardEffect.cpp
    domain/HeadsetEffect.cpp
    domain/G915KeyCatalog.cpp
    domain/KeyColorMap.cpp
    domain/AnimationFrames.cpp
    domain/ScreenRegions.cpp
    domain/KeyboardStatus.cpp)
target_link_libraries(ghub_lighting_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_lighting_domain)

# INC-008 parte C (ADR-0013 decisão 3): ports e casos de uso; sem Qt nem I/O. Outros módulos só pela API.
add_library(ghub_lighting_application STATIC)
target_sources(ghub_lighting_application PRIVATE
    application/KeyboardAccess.cpp
    application/LiveKeyboardSession.cpp
    application/SaveKeyboardEffect.cpp
    application/RunSoftwareAnimation.cpp
    application/AudioVisualizer.cpp
    application/ScreenSampler.cpp
    application/ReadKeyboardStatus.cpp)
target_link_libraries(ghub_lighting_application PUBLIC ghub_lighting_domain ghub_device_access_api ghub_audio_processing_api)
ghub_set_warnings(ghub_lighting_application)

# INC-008 parte C (apresentação): controlador do modo ao vivo do G915 para a janela. Só depende da camada
# application; a infraestrutura é montada no composition root.
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Concurrent)
    add_library(ghub_lighting_presentation STATIC)
    target_sources(ghub_lighting_presentation PRIVATE
        presentation/KeyboardLightingController.hpp
        presentation/KeyboardLightingController.cpp
        presentation/KeyboardStatusViewModel.hpp
        presentation/KeyboardStatusViewModel.cpp
        presentation/SoftwareAnimationController.hpp
        presentation/SoftwareAnimationController.cpp)
    set_target_properties(ghub_lighting_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_lighting_presentation PUBLIC ghub_lighting_application Qt6::Core Qt6::Gui
                                                      PRIVATE Qt6::Concurrent)
    ghub_set_warnings(ghub_lighting_presentation)
endif()

# INC-021 R4 (ADR-0030): amostragem da tela pelo portal ScreenCast (QtDBus) e libpipewire. Só com o app.
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GHUB_SCREEN_PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3>=1.6.9)
    add_library(ghub_lighting_screen_infrastructure STATIC)
    target_sources(ghub_lighting_screen_infrastructure PRIVATE
        infrastructure/ScreenCastPortal.cpp
        infrastructure/QtDBusScreenCastPortal.cpp
        infrastructure/PortalScreenColorSource.cpp
        infrastructure/PipeWireScreenStream.cpp)
    set_target_properties(ghub_lighting_screen_infrastructure PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_lighting_screen_infrastructure PUBLIC
        ghub_lighting_application Qt6::Core Qt6::DBus PkgConfig::GHUB_SCREEN_PIPEWIRE)
    ghub_set_warnings(ghub_lighting_screen_infrastructure)
endif()
