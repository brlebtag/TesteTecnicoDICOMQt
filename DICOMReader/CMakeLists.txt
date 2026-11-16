cmake_minimum_required(VERSION 3.25)

project(DICOMReader
    VERSION 1.0.0
    DESCRIPTION "Visualizador de imagens DICOM em Qt"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DICOMREADER_BUILD_TESTS "Compila os testes automatizados." ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compilerwarnings)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

# A DCMTK é incluída antes de qt_standard_project_setup() para que os alvos
# dela não recebam AUTOMOC/AUTOUIC.
include(dcmtk)

qt_standard_project_setup()

add_subdirectory(src)
