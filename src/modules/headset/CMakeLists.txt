# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0015).
add_library(ghub_headset_domain STATIC)
target_sources(ghub_headset_domain PRIVATE
    domain/SidetoneLevel.cpp
    domain/HeadsetStatus.cpp)
target_link_libraries(ghub_headset_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_headset_domain)

# Camada de aplicação: port e caso de uso; sem Qt nem I/O.
add_library(ghub_headset_application STATIC)
target_sources(ghub_headset_application PRIVATE
    application/ChangeHeadsetSetting.cpp
    application/ReadHeadsetStatus.cpp)
target_link_libraries(ghub_headset_application PUBLIC ghub_headset_domain ghub_device_access_api)
ghub_set_warnings(ghub_headset_application)

# Infraestrutura HID++ do G733 (ADR-0025): leituras de estado sobre o canal compartilhado; sem Qt.
add_library(ghub_headset_hidpp STATIC)
target_sources(ghub_headset_hidpp PRIVATE
    infrastructure/G733StatusPolicy.hpp
    infrastructure/G733StatusPolicy.cpp
    infrastructure/HidppHeadsetStatusSource.hpp
    infrastructure/HidppHeadsetStatusSource.cpp)
target_link_libraries(ghub_headset_hidpp PUBLIC ghub_headset_application ghub_platform_hidpp)
ghub_set_warnings(ghub_headset_hidpp)

# Infraestrutura (HeadsetControl CLI) e apresentação: só com Qt (o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core GLOBAL)
    add_library(ghub_headset_infrastructure STATIC)
    target_sources(ghub_headset_infrastructure PRIVATE
        infrastructure/HeadsetControlActionJson.hpp
        infrastructure/HeadsetControlActionJson.cpp
        infrastructure/HeadsetControlSettingsWriter.hpp
        infrastructure/HeadsetControlSettingsWriter.cpp)
    target_link_libraries(ghub_headset_infrastructure PUBLIC ghub_headset_application Qt6::Core)
    ghub_set_warnings(ghub_headset_infrastructure)
endif()

# Apresentação: controlador QObject sobre o caso de uso; escrita em thread de trabalho (QtConcurrent).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Concurrent GLOBAL)
    add_library(ghub_headset_presentation STATIC)
    target_sources(ghub_headset_presentation PRIVATE
        presentation/HeadsetSettingsController.hpp
        presentation/HeadsetSettingsController.cpp
        presentation/HeadsetStatusViewModel.hpp
        presentation/HeadsetStatusViewModel.cpp)
    set_target_properties(ghub_headset_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_headset_presentation PUBLIC ghub_headset_application Qt6::Core
                                                    PRIVATE Qt6::Concurrent)
    ghub_set_warnings(ghub_headset_presentation)
endif()
