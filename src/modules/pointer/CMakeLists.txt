# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0011).
add_library(ghub_pointer_domain STATIC)
target_sources(ghub_pointer_domain PRIVATE
    domain/ResolutionStage.cpp
    domain/ButtonMapping.cpp
    domain/LedState.cpp
    domain/PointerProfile.cpp
    domain/PointerConfiguration.cpp)
target_link_libraries(ghub_pointer_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_pointer_domain)

# Camada de aplicação: casos de uso e ports; sem Qt nem I/O.
add_library(ghub_pointer_application STATIC)
target_sources(ghub_pointer_application PRIVATE
    application/ReadPointerConfiguration.cpp)
# Outros módulos só pela API pública (ADR-0006).
target_link_libraries(ghub_pointer_application PUBLIC
    ghub_pointer_domain ghub_device_inventory_api ghub_device_access_api)
ghub_set_warnings(ghub_pointer_application)

# Infraestrutura: cliente D-Bus do ratbagd, somente leitura (ADR-0011); só com Qt (o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus GLOBAL)
    add_library(ghub_pointer_infrastructure STATIC)
    target_sources(ghub_pointer_infrastructure PRIVATE
        infrastructure/RatbagPointerBackend.hpp
        infrastructure/RatbagPointerBackend.cpp)
    target_link_libraries(ghub_pointer_infrastructure PUBLIC ghub_pointer_application Qt6::Core Qt6::DBus)
    ghub_set_warnings(ghub_pointer_infrastructure)

    # Leitura em thread de trabalho (QtConcurrent), sem D-Bus: só depende do caso de uso.
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Concurrent GLOBAL)
    add_library(ghub_pointer_background STATIC)
    target_sources(ghub_pointer_background PRIVATE
        infrastructure/BackgroundPointerReader.hpp
        infrastructure/BackgroundPointerReader.cpp)
    set_target_properties(ghub_pointer_background PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_pointer_background PUBLIC ghub_pointer_application Qt6::Core PRIVATE Qt6::Concurrent)
    ghub_set_warnings(ghub_pointer_background)

    # Apresentação: ViewModel QObject sobre o port PointerReadRunner; nunca depende da infraestrutura.
    add_library(ghub_pointer_presentation STATIC)
    target_sources(ghub_pointer_presentation PRIVATE
        presentation/PointerViewModel.hpp
        presentation/PointerViewModel.cpp)
    set_target_properties(ghub_pointer_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_pointer_presentation PUBLIC ghub_pointer_application Qt6::Core)
    ghub_set_warnings(ghub_pointer_presentation)
endif()
