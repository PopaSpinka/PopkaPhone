# eglfs_naomi — драйвер экрана Qt для Pixel 5: HWC2 через libhybris, 90 Гц, учёт кадров.
# Подключается так: QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_INTEGRATION=eglfs_naomi,
# QT_PLUGIN_PATH=<сборка>/plugins (tools/naomi run делает это сам).
add_library(eglfs_naomi MODULE
    naomi_hwc.cpp naomi_hwc.h
    present_timer.cpp present_timer.h
    naomi_display.h
    eglfs_naomi.json)
target_link_libraries(eglfs_naomi PRIVATE
    Qt6::EglFSDeviceIntegrationPrivate Qt6::GuiPrivate PkgConfig::HYBRIS
    gralloc)   # hybris_gralloc_lock — залить чёрный кадр (naomi_hwc.cpp, blankFrame)
# eglfs в Qt собран с QT_EGL_NO_X11: без него qt_egl_p.h включает USE_X11, EGLNativeDisplayType
# становится _XDisplay*, и подписи виртуальных функций QEglFSDeviceIntegration не совпадут с
# библиотекой (плагин не загрузится: undefined symbol …createDisplayEP9_XDisplay).
target_compile_definitions(eglfs_naomi PRIVATE QT_EGL_NO_X11)
set_target_properties(eglfs_naomi PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/egldeviceintegrations)
