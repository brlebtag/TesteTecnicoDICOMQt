# Disponibiliza a DCMTK (https://dicom.offis.de/dcmtk) para o projeto.
#
# Por padrão, a versão fixada abaixo é baixada e compilada junto com o projeto
# (FetchContent), apenas com os módulos usados pelo DICOMReader. Para usar uma
# DCMTK já instalada, configure com -DDICOMREADER_USE_SYSTEM_DCMTK=ON (e, se
# preciso, -DDCMTK_DIR=<prefixo>/lib/cmake/dcmtk).
#
# Resultado: o alvo de interface dicomreader::dcmtk.

option(DICOMREADER_USE_SYSTEM_DCMTK "Usa uma DCMTK já instalada em vez de baixá-la." OFF)

set(DICOMREADER_DCMTK_VERSION "3.7.1")
set(DICOMREADER_DCMTK_SHA256 "38ac8fceebe201921f6c101493e9611ba12fb5c597db7311df26e95d6d6bf39a")

# Bibliotecas usadas: leitura do arquivo (dcmdata), renderização de imagens
# monocromáticas (dcmimgle) e coloridas (dcmimage) e os decodificadores
# JPEG (dcmjpeg) e JPEG-LS (dcmjpls). O decodificador RLE já vem no dcmdata.
set(_dicomreader_dcmtk_libraries dcmdata dcmimgle dcmimage dcmjpeg dcmjpls)

# As opções ficam dentro de uma função para não vazar para o restante do projeto.
function(_dicomreader_fetch_dcmtk)
    include(FetchContent)

    set(DCMTK_MODULES ofstd oflog oficonv dcmdata dcmimgle dcmimage dcmjpeg dcmjpls)
    set(BUILD_APPS OFF)
    set(BUILD_SHARED_LIBS OFF)
    set(DCMTK_WITH_TIFF OFF)
    set(DCMTK_WITH_PNG OFF)
    set(DCMTK_WITH_XML OFF)
    set(DCMTK_WITH_ZLIB OFF)
    set(DCMTK_WITH_OPENSSL OFF)
    set(DCMTK_WITH_SNDFILE OFF)
    set(DCMTK_WITH_ICONV OFF)
    set(DCMTK_WITH_ICU OFF)
    set(DCMTK_WITH_WRAP OFF)
    set(DCMTK_WITH_OPENJPEG OFF)
    set(DCMTK_WITH_DOXYGEN OFF)
    set(DCMTK_ENABLE_MANPAGES OFF)
    # Dicionário DICOM e tabelas de conversão de caracteres embutidos no
    # executável: a aplicação não depende de arquivos externos em tempo de execução.
    set(DCMTK_DEFAULT_DICT builtin)
    set(DCMTK_ENABLE_BUILTIN_OFICONV_DATA ON)
    # Caminhos com caracteres não ASCII no Windows.
    set(DCMTK_WIDE_CHAR_FILE_IO_FUNCTIONS ON)
    # Mesmo runtime do Qt no MSVC (/MD).
    set(DCMTK_COMPILE_WIN32_MULTITHREADED_DLL ON)

    FetchContent_Declare(dcmtk
        URL "https://github.com/DCMTK/dcmtk/archive/refs/tags/DCMTK-${DICOMREADER_DCMTK_VERSION}.tar.gz"
        URL_HASH "SHA256=${DICOMREADER_DCMTK_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        # Headers da DCMTK como "system": os avisos rigorosos do projeto não
        # se aplicam a eles.
        SYSTEM
        # Só as bibliotecas usadas são compiladas, e nada da DCMTK é instalado
        # junto com a aplicação (ela é ligada estaticamente).
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(dcmtk)
endfunction()

if(DICOMREADER_USE_SYSTEM_DCMTK)
    find_package(DCMTK CONFIG REQUIRED)
    list(TRANSFORM _dicomreader_dcmtk_libraries PREPEND "DCMTK::")
else()
    _dicomreader_fetch_dcmtk()
endif()

add_library(dicomreader_dcmtk INTERFACE)
add_library(dicomreader::dcmtk ALIAS dicomreader_dcmtk)
target_link_libraries(dicomreader_dcmtk INTERFACE ${_dicomreader_dcmtk_libraries})
