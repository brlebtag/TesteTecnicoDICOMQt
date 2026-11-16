add_subdirectory(core)
add_subdirectory(ui)

qt_add_executable(DICOMReader main.cpp)

target_compile_definitions(DICOMReader PRIVATE DICOMREADER_VERSION="${PROJECT_VERSION}")
target_link_libraries(DICOMReader PRIVATE dicomui)
dicomreader_enable_warnings(DICOMReader)

set_target_properties(DICOMReader PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
