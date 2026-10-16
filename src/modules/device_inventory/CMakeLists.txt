# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0006).
add_library(ghub_device_inventory_domain STATIC)
target_sources(ghub_device_inventory_domain PRIVATE
    domain/DeviceCatalog.cpp
    domain/DeviceType.cpp
    domain/FirmwareVersion.cpp
    domain/DeviceIdentity.cpp
    domain/DeviceNickname.cpp)
target_link_libraries(ghub_device_inventory_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_device_inventory_domain)

# API pública: único alvo que outros módulos linkam (ADR-0006).
add_library(ghub_device_inventory_api INTERFACE)
target_link_libraries(ghub_device_inventory_api INTERFACE ghub_device_inventory_domain)

# Camada de aplicação: casos de uso e ports; sem Qt nem I/O.
add_library(ghub_device_inventory_application STATIC)
target_sources(ghub_device_inventory_application PRIVATE
    application/ListDetectedDevices.cpp
    application/ReadDeviceIdentity.cpp)
target_link_libraries(ghub_device_inventory_application PUBLIC ghub_device_inventory_domain ghub_device_access_api)
ghub_set_warnings(ghub_device_inventory_application)

# Infraestrutura: adaptadores dos ports (filesystem permitido aqui).
add_library(ghub_device_inventory_infrastructure STATIC)
target_sources(ghub_device_inventory_infrastructure PRIVATE
    infrastructure/SysfsHidDiscovery.cpp
    infrastructure/SysfsVideoDiscovery.cpp
    infrastructure/InventoryIdentityPolicy.cpp
    infrastructure/HidppIdentitySource.cpp)
target_link_libraries(ghub_device_inventory_infrastructure PUBLIC ghub_device_inventory_application ghub_platform_hidpp)
ghub_set_warnings(ghub_device_inventory_infrastructure)

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Concurrent)
    add_library(ghub_device_inventory_presentation STATIC)
    target_sources(ghub_device_inventory_presentation PRIVATE
        presentation/DeviceInventoryViewModel.hpp
        presentation/DeviceInventoryViewModel.cpp
        presentation/DeviceIdentityViewModel.hpp
        presentation/DeviceIdentityViewModel.cpp)
    set_target_properties(ghub_device_inventory_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_device_inventory_presentation PUBLIC ghub_device_inventory_application Qt6::Core
                                                              PRIVATE Qt6::Concurrent)
    ghub_set_warnings(ghub_device_inventory_presentation)
endif()
