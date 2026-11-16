qt_add_executable(DICOMReader
    main.cpp
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
)

target_include_directories(DICOMReader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(DICOMReader PRIVATE Qt6::Widgets)
dicomreader_enable_warnings(DICOMReader)

set_target_properties(DICOMReader PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
