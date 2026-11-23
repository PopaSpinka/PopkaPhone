# naomi-game — «приставка» стола (docs/GAMES.md): одна игра в ядре эмулятора libretro, окно — клиент Wayland оболочки
# (запускает её оболочка: app_launcher). Без Qt: libwayland-client, libpulse, libc. Протокол xdg-shell — из
# описания, что лежит в среде сборки с Qt Wayland; заголовок libretro.h — в third_party (README.md там).
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
set(XDG_SHELL_XML /usr/share/qt6/wayland/protocols/xdg-shell/xdg-shell.xml)
add_custom_command(OUTPUT xdg-shell-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_XML} xdg-shell-client-protocol.h
    DEPENDS ${XDG_SHELL_XML})
add_custom_command(OUTPUT xdg-shell-protocol.c
    COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_XML} xdg-shell-protocol.c
    DEPENDS ${XDG_SHELL_XML})
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)

add_executable(naomi-game main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h)
set_target_properties(naomi-game PROPERTIES AUTOMOC OFF)
target_include_directories(naomi-game PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(naomi-game PRIVATE PkgConfig::WAYLAND_CLIENT PkgConfig::PULSE ${CMAKE_DL_LIBS})
