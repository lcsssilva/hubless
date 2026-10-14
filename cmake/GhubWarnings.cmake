# Aplica o conjunto de avisos obrigatório (ENGINEERING_RULES.md §4) a um alvo.
function(ghub_set_warnings target)
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        set(_scope INTERFACE)
    else()
        set(_scope PRIVATE)
    endif()
    target_compile_options(${target} ${_scope}
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Woverloaded-virtual)
endfunction()
