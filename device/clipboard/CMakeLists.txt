# Проверка общего буфера обмена (clip_test.cpp, run.py). Сборка на телефоне, в среде сборки:
#   cmake -S ~/naomi/src/device/clipboard -B ~/naomi/cliptest -G Ninja && ninja -C ~/naomi/cliptest
cmake_minimum_required(VERSION 3.16)
project(clip_test CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick WaylandCompositor WaylandCompositorPrivate)
set(SHELL_APP ${CMAKE_CURRENT_SOURCE_DIR}/../../shell/app)
add_executable(clip_test clip_test.cpp ${SHELL_APP}/shell_compositor.cpp ${SHELL_APP}/shell_compositor.h)
target_include_directories(clip_test PRIVATE ${SHELL_APP})
target_link_libraries(clip_test PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick Qt6::WaylandCompositor Qt6::WaylandCompositorPrivate)
