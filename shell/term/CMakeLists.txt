# naomi-term — служба вкладок «Терминала» стола (docs/TERMINAL.md): псевдотерминалы, текст вкладок, сохранение на
# диск. Без Qt — только libc: живёт отдельно от оболочки (systemd-run --user) и весит меньше мегабайта.
add_executable(naomi-term main.cpp)
target_link_libraries(naomi-term PRIVATE util)
