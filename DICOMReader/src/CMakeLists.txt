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

# "cmake --install" gera uma pasta pronta para executar, com as bibliotecas e
# plugins do Qt copiados por windeployqt/macdeployqt (ou equivalente no Linux).
include(GNUInstallDirs)
install(TARGETS DICOMReader
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
qt_generate_deploy_app_script(
    TARGET DICOMReader
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT "${deploy_script}")
