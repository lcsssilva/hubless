# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0020).
add_library(ghub_profiles_domain STATIC)
target_sources(ghub_profiles_domain PRIVATE
    domain/EqualizerGains.cpp
    domain/Profile.cpp
    domain/ProfileLibrary.cpp
    domain/ProfileName.cpp
    domain/ApplicationId.cpp)
target_link_libraries(ghub_profiles_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_profiles_domain)

# Camada de aplicação: ports e casos de uso; sem Qt nem I/O.
add_library(ghub_profiles_application STATIC)
target_sources(ghub_profiles_application PRIVATE
    application/ApplyProfile.cpp
    application/AutomaticProfileSwitcher.cpp
    application/ProfileBook.cpp)
target_link_libraries(ghub_profiles_application PUBLIC ghub_profiles_domain)
ghub_set_warnings(ghub_profiles_application)

# Infraestrutura (JSON em disco): só com Qt (o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus GLOBAL)
    add_library(ghub_profiles_infrastructure STATIC)
    target_sources(ghub_profiles_infrastructure PRIVATE
        infrastructure/CameraSettingsAdapter.hpp
        infrastructure/CameraSettingsAdapter.cpp
        infrastructure/EqualizerSettingsAdapter.hpp
        infrastructure/JsonProfileFormat.hpp
        infrastructure/JsonProfileFormat.cpp
        infrastructure/JsonProfileTransfer.hpp
        infrastructure/JsonProfileTransfer.cpp
        infrastructure/JsonProfileRepository.hpp
        infrastructure/JsonProfileRepository.cpp
        infrastructure/KWinActiveApplicationSource.hpp
        infrastructure/KWinActiveApplicationSource.cpp)
    set_target_properties(ghub_profiles_infrastructure PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_profiles_infrastructure PUBLIC ghub_profiles_application ghub_camera_api
                                                       ghub_audio_processing_api Qt6::Core Qt6::DBus)
    ghub_set_warnings(ghub_profiles_infrastructure)
endif()

# Apresentação: controlador QObject; I/O em thread de trabalho (QtConcurrent). Só com Qt.
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Concurrent GLOBAL)
    add_library(ghub_profiles_presentation STATIC)
    target_sources(ghub_profiles_presentation PRIVATE
        presentation/ProfilesController.hpp
        presentation/ProfilesController.cpp)
    set_target_properties(ghub_profiles_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_profiles_presentation PUBLIC ghub_profiles_application Qt6::Core
                                                     PRIVATE Qt6::Concurrent)
    ghub_set_warnings(ghub_profiles_presentation)
endif()
