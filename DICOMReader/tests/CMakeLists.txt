find_package(Qt6 REQUIRED COMPONENTS Test)

# Gerador de arquivos DICOM sintéticos, compartilhado pelos testes.
add_library(dicomtestfiles STATIC
    dicomtestfiles.cpp
    dicomtestfiles.h
)
target_include_directories(dicomtestfiles PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(dicomtestfiles PUBLIC dicomreader::dcmtk Qt6::Core)
dicomreader_enable_warnings(dicomtestfiles)

# Registra um executável de teste no ctest.
function(dicomreader_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE dicomtestfiles Qt6::Test ${ARGN})
    dicomreader_enable_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})

    # No Windows as DLLs do Qt precisam estar no PATH para o teste rodar via ctest.
    if(WIN32)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}"
        )
    endif()
endfunction()

dicomreader_add_test(tst_dicomcore dicomcore)
dicomreader_add_test(tst_ui dicomui)
set_tests_properties(tst_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
