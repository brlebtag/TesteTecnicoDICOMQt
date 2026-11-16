# Núcleo DICOM: leitura, validação, renderização e metadados.
# Não depende de Qt Widgets, o que permite testá-lo isoladamente.
add_library(dicomcore STATIC
    dicomimagedocument.cpp
    dicomimagedocument.h
    dicomimagedocument_p.h
    dicomloader.cpp
    dicomloader.h
    dicommetadata.cpp
    dicommetadata.h
    loaderror.cpp
    loaderror.h
)

target_include_directories(dicomcore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(dicomcore
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE dicomreader::dcmtk
)
dicomreader_enable_warnings(dicomcore)
