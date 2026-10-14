# Instalação do que não é código: entrada de menu, ícone e regra udev (INC-013 parte 2, ADR-0021).
install(FILES ghub-linux.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ghub-linux.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(GHUB_INSTALL_UDEV_RULES)
    # Caminho do udev é fixo em lib/ (não depende de CMAKE_INSTALL_LIBDIR, que pode ser lib64).
    install(FILES 70-ghub-linux.rules DESTINATION lib/udev/rules.d)
endif()
