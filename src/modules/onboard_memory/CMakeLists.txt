# Módulo onboard_memory (ADR-0027): memória onboard HID++ (feature 0x8100) — setor com CRC, diretório, gravação do
# setor ativo com cópia, releitura e restauração. A interpretação dos bytes é dos módulos donos dos dispositivos.

# Domínio: somente STL + shared_kernel.
add_library(ghub_onboard_memory_domain STATIC)
target_sources(ghub_onboard_memory_domain PRIVATE
    domain/OnboardSector.cpp
    domain/OnboardDirectory.cpp)
target_link_libraries(ghub_onboard_memory_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_onboard_memory_domain)

# Aplicação: ports e caso de uso genérico; sem Qt nem I/O. Outros módulos só pela API pública (ADR-0006).
add_library(ghub_onboard_memory_application STATIC)
target_sources(ghub_onboard_memory_application PRIVATE
    application/ActiveSectorEditor.cpp)
target_link_libraries(ghub_onboard_memory_application PUBLIC ghub_onboard_memory_domain ghub_device_access_api)
ghub_set_warnings(ghub_onboard_memory_application)

# API pública: consumida pelos módulos donos dos dispositivos (pointer, lighting).
add_library(ghub_onboard_memory_api INTERFACE)
target_include_directories(ghub_onboard_memory_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(ghub_onboard_memory_api INTERFACE ghub_onboard_memory_application)

# Infraestrutura: 0x8100 sobre platform/hidpp e cópia em arquivo; sem Qt.
add_library(ghub_onboard_memory_infrastructure STATIC)
target_sources(ghub_onboard_memory_infrastructure PRIVATE
    infrastructure/HidppOnboardMemory.hpp
    infrastructure/HidppOnboardMemory.cpp
    infrastructure/FileOnboardBackupStore.hpp
    infrastructure/FileOnboardBackupStore.cpp)
target_link_libraries(ghub_onboard_memory_infrastructure PUBLIC ghub_onboard_memory_api ghub_platform_hidpp)
ghub_set_warnings(ghub_onboard_memory_infrastructure)
