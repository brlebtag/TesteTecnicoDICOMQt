add_subdirectory(core)

qt_add_executable(DICOMReader
    main.cpp
    ui/imageview.cpp
    ui/imageview.h
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
    ui/metadatapanel.cpp
    ui/metadatapanel.h
    ui/windowlevelpanel.cpp
    ui/windowlevelpanel.h
)

target_include_directories(DICOMReader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(DICOMReader PRIVATE DICOMREADER_VERSION="${PROJECT_VERSION}")
target_link_libraries(DICOMReader PRIVATE dicomcore Qt6::Widgets)
dicomreader_enable_warnings(DICOMReader)

set_target_properties(DICOMReader PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
