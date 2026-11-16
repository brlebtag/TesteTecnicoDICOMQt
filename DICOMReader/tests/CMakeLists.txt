find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(tst_dicomcore
    dicomtestfiles.cpp
    dicomtestfiles.h
    tst_dicomcore.cpp
)
target_link_libraries(tst_dicomcore PRIVATE dicomcore dicomreader::dcmtk Qt6::Test)
dicomreader_enable_warnings(tst_dicomcore)

add_test(NAME tst_dicomcore COMMAND tst_dicomcore)

# No Windows as DLLs do Qt precisam estar no PATH para o teste rodar via ctest.
if(WIN32)
    set_tests_properties(tst_dicomcore PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}"
    )
endif()
