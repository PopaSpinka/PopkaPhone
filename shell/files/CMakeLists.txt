# naomi-files — «Файлы» рабочего стола (docs/FILES.md): модель места (папка, «Избранное», корзина), операции с
# отменой, избранное, закреплённые, корзина. Окно — QML стола (shell/app/qml/desktop/files/).
add_library(naomi-files STATIC
    files_model.cpp files_model.h
    files_ops.cpp files_ops.h
    files_service.cpp files_service.h)
target_include_directories(naomi-files PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(naomi-files PUBLIC Qt6::Core Qt6::Gui Qt6::Qml)
