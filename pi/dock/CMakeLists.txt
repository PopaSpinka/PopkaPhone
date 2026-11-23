# НаомиФон — приставка на Raspberry Pi 5 (docs/PI.md): рабочий стол телефона на мониторе.
# Собирается на самом Pi (tools/pi build): ffmpeg Raspberry Pi OS (аппаратный HEVC — v4l2-request) и libdrm.
cmake_minimum_required(VERSION 3.16)
project(naomi-dock CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET libavcodec libavutil libdrm)

add_executable(naomi-dock main.cpp link.cpp decoder.cpp display.cpp input.cpp trackpad.cpp audio_out.cpp lock.cpp clock.cpp
               cursor.cpp brightness.cpp tablet.cpp wol.cpp)
# протокол — тот же файл, что у телефона
target_include_directories(naomi-dock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../shell/stream)
target_compile_options(naomi-dock PRIVATE -Wall -Wextra)
# звук — системная libasound.so.2, часы без дока и курсор — cairo и pango (clock.cpp, cursor.cpp), связь с планшетом —
# OpenSSL (tablet.cpp, ssl_min.h): по имени файла, заголовков (-dev) на Pi нет
target_link_libraries(naomi-dock PRIVATE PkgConfig::DEPS -l:libasound.so.2 -l:libcairo.so.2 -l:libpango-1.0.so.0
                      -l:libpangocairo-1.0.so.0 -l:libgobject-2.0.so.0 -l:libssl.so.3 -l:libcrypto.so.3 pthread)
