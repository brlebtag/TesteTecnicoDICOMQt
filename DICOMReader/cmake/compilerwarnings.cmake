# Ativa um conjunto rigoroso de avisos do compilador num alvo do projeto.
# Aplicado só aos alvos do DICOMReader, nunca às dependências.
function(dicomreader_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
        )
    endif()
endfunction()
