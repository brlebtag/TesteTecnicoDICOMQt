#ifndef DICOMREADER_TESTS_DICOMTESTFILES_H
#define DICOMREADER_TESTS_DICOMTESTFILES_H

// Gera arquivos DICOM sintéticos com a DCMTK, para que os testes não
// dependam de amostras externas.

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcxfer.h>

#include <QString>

#include <cstdint>
#include <vector>

namespace testfiles {

/// Parâmetros de uma imagem monocromática de 16 bits alocados.
struct MonochromeImage
{
    unsigned short rows = 0;
    unsigned short columns = 0;
    unsigned short frames = 1;
    unsigned short bitsStored = 12;
    bool isSigned = false;
    const char *photometric = "MONOCHROME2";
    std::vector<std::uint16_t> pixels; ///< rows * columns * frames, linha a linha.
};

/// Dataset mínimo de uma Secondary Capture com os atributos de paciente.
void addIdentification(DcmDataset &dataset);

/// Preenche o módulo Image Pixel com uma imagem monocromática.
void setMonochromePixels(DcmDataset &dataset, const MonochromeImage &image);

/// Preenche o módulo Image Pixel com uma imagem colorida de 8 bits por amostra
/// (intercalada: R1G1B1R2G2B2...).
void setColorPixels(DcmDataset &dataset, unsigned short rows, unsigned short columns,
                    const char *photometric, const std::vector<std::uint8_t> &samples);

/// Imagem monocromática completa (identificação + pixels).
DcmFileFormat makeMonochromeFile(const MonochromeImage &image);

/// Grava em formato DICOM Part 10 (preâmbulo + cabeçalho), comprimindo os
/// pixels quando a sintaxe de transferência pede. Retorna false em falha.
bool save(DcmFileFormat &fileFormat, const QString &path,
          E_TransferSyntax xfer = EXS_LittleEndianExplicit);

/// Grava só o dataset, sem preâmbulo nem cabeçalho (formato antigo/ACR-NEMA).
bool saveDatasetOnly(DcmDataset &dataset, const QString &path);

/// Grava um dataset cujos pixels estão encapsulados com a sintaxe indicada
/// (conteúdo fictício), para simular compressões sem decodificador.
bool saveWithFakeEncapsulatedPixels(const QString &path, E_TransferSyntax xfer);

} // namespace testfiles

#endif // DICOMREADER_TESTS_DICOMTESTFILES_H
