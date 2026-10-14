# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0004).
add_library(ghub_battery_domain STATIC)
target_sources(ghub_battery_domain PRIVATE
    domain/BatteryLevel.cpp
    domain/BatteryReading.cpp)
target_link_libraries(ghub_battery_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_battery_domain)

# Camada de aplicação: casos de uso e ports; sem Qt nem I/O.
add_library(ghub_battery_application STATIC)
target_sources(ghub_battery_application PRIVATE
    application/GetBatteryStatus.cpp)
# Outros módulos só pela API pública (ADR-0006): nenhum alvo de aplicação/infraestrutura de device_inventory.
target_link_libraries(ghub_battery_application PUBLIC ghub_battery_domain ghub_device_inventory_api)
ghub_set_warnings(ghub_battery_application)

# Infraestrutura: adaptadores dos ports (filesystem permitido aqui).
add_library(ghub_battery_infrastructure STATIC)
target_sources(ghub_battery_infrastructure PRIVATE
    infrastructure/SysfsBatterySource.cpp
    infrastructure/KernelUevent.cpp)
target_link_libraries(ghub_battery_infrastructure PUBLIC ghub_battery_application)
ghub_set_warnings(ghub_battery_infrastructure)

# Apresentação: ViewModels QObject expostos ao QML (só com Qt; o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
    add_library(ghub_battery_presentation STATIC)
    target_sources(ghub_battery_presentation PRIVATE
        presentation/BatteryViewModel.hpp
        presentation/BatteryViewModel.cpp)
    set_target_properties(ghub_battery_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_battery_presentation PUBLIC ghub_battery_application Qt6::Core)
    ghub_set_warnings(ghub_battery_presentation)

    # Notificação por uevent do kernel (ADR-0005): adaptador com Qt (QSocketNotifier).
    add_library(ghub_battery_uevent STATIC)
    target_sources(ghub_battery_uevent PRIVATE
        infrastructure/UeventBatteryMonitor.hpp
        infrastructure/UeventBatteryMonitor.cpp)
    set_target_properties(ghub_battery_uevent PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_battery_uevent PUBLIC ghub_battery_infrastructure Qt6::Core)
    ghub_set_warnings(ghub_battery_uevent)

    # Bateria do G733 via HeadsetControl CLI (ADR-0007): QProcess assíncrono + parser do JSON.
    add_library(ghub_battery_headsetcontrol STATIC)
    target_sources(ghub_battery_headsetcontrol PRIVATE
        infrastructure/HeadsetControlJson.hpp
        infrastructure/HeadsetControlJson.cpp
        infrastructure/HeadsetControlBatterySource.hpp
        infrastructure/HeadsetControlBatterySource.cpp)
    set_target_properties(ghub_battery_headsetcontrol PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_battery_headsetcontrol PUBLIC
        ghub_battery_application ghub_device_access_api Qt6::Core)
    ghub_set_warnings(ghub_battery_headsetcontrol)
endif()
