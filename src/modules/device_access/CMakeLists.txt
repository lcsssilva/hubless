# Camada de domínio: somente STL + shared_kernel (ADR-0002).
add_library(ghub_device_access_domain STATIC)
target_sources(ghub_device_access_domain PRIVATE
    domain/AccessRegistry.cpp
    domain/PhysicalInterfaceId.cpp
    domain/LeaseRequest.cpp
    domain/ExternalOwner.cpp)
target_link_libraries(ghub_device_access_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_device_access_domain)

# API pública: ports consumidos por outros módulos.
add_library(ghub_device_access_api INTERFACE)
target_include_directories(ghub_device_access_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(ghub_device_access_api INTERFACE ghub_device_access_domain)

# Infraestrutura: adaptadores dos ports (filesystem permitido aqui; sem Qt).
add_library(ghub_device_access_infrastructure STATIC)
target_sources(ghub_device_access_infrastructure PRIVATE
    infrastructure/ProcExternalProcessProbe.cpp
    infrastructure/SteadyClock.cpp)
target_link_libraries(ghub_device_access_infrastructure PUBLIC ghub_device_access_api)
ghub_set_warnings(ghub_device_access_infrastructure)

# Monitor de suspensão via logind (ADR-0014): D-Bus do sistema, só com Qt (o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus GLOBAL)
    add_library(ghub_device_access_logind STATIC)
    target_sources(ghub_device_access_logind PRIVATE
        infrastructure/LogindSleepMonitor.hpp
        infrastructure/LogindSleepMonitor.cpp)
    set_target_properties(ghub_device_access_logind PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_device_access_logind PUBLIC ghub_device_access_api Qt6::Core Qt6::DBus)
    ghub_set_warnings(ghub_device_access_logind)
endif()
