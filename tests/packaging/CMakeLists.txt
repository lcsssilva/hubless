# Testes de empacotamento (INC-013 parte 2). Python puro (unittest), rodados com -I; caminhos chegam por argv.
# Ferramenta ausente (udevadm, desktop-file-validate, readelf, pacman) → unittest.skipTest: o caso aparece como
# "skipped" na saída do teste, mas o ctest conta o arquivo como aprovado (saída 0).
set(GHUB_PKG_SRC "${PROJECT_SOURCE_DIR}")

function(ghub_add_packaging_test name script)
    add_test(NAME packaging.${name}
        COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/${script}" ${ARGN})
    set_tests_properties(packaging.${name} PROPERTIES TIMEOUT 120)
endfunction()

ghub_add_packaging_test(install_tree test_install_tree.py "${PROJECT_BINARY_DIR}" "${GHUB_PKG_SRC}")
ghub_add_packaging_test(desktop_entry test_desktop_entry.py "${GHUB_PKG_SRC}")
ghub_add_packaging_test(udev_rules test_udev_rules.py "${GHUB_PKG_SRC}")
ghub_add_packaging_test(pkgbuild test_pkgbuild.py "${GHUB_PKG_SRC}" "$<TARGET_FILE:ghub-linux>")
