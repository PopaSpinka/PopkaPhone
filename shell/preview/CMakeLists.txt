# naomi-preview — «Просмотр» рабочего стола (docs/DESKTOP.md, «Просмотр»): что за файл, папка по имени, видео
# (GStreamer: playbin, кадры NV12 — текстурами для шейдера стола). Окно — QML стола (shell/app/qml/desktop/preview/).
# GStreamer — системный (1.24), заголовки — libgstreamer1.0-dev в среде сборки (device/devroot.sh)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)
add_library(naomi-preview STATIC
    preview_service.cpp preview_service.h
    preview_video.cpp preview_video.h
    video_plane.cpp video_plane.h)
target_include_directories(naomi-preview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(naomi-preview PUBLIC Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Quick PkgConfig::GST)
