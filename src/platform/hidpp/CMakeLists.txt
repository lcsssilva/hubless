# ADR-0017 decisões 1 e 2: transporte HID++ 2.0 sobre hidraw compartilhado entre módulos. Só STL + shared_kernel;
# a política de comandos permitidos é injetada pelo módulo dono (HidppCommandPolicy).
add_library(ghub_platform_hidpp STATIC)
target_sources(ghub_platform_hidpp PRIVATE
    HidrawDevice.cpp
    HidppFeatureMap.cpp
    HidppChannel.cpp
    HidppCapabilities.cpp
    HidppNodeLocator.cpp)
target_link_libraries(ghub_platform_hidpp PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_platform_hidpp)
