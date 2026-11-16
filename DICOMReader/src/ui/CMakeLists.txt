# Interface gráfica (Qt Widgets). Biblioteca separada do executável para que
# os testes de interface possam usá-la.
add_library(dicomui STATIC
    imageview.cpp
    imageview.h
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    metadatapanel.cpp
    metadatapanel.h
    windowlevelpanel.cpp
    windowlevelpanel.h
)

target_include_directories(dicomui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(dicomui PUBLIC dicomcore Qt6::Widgets)
dicomreader_enable_warnings(dicomui)
