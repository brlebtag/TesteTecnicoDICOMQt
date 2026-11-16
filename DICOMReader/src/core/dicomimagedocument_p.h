#ifndef DICOMREADER_CORE_DICOMIMAGEDOCUMENT_P_H
#define DICOMREADER_CORE_DICOMIMAGEDOCUMENT_P_H

// Detalhe de implementação: não faz parte da API do núcleo.

#include "dicomimagedocument.h"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmimgle/dcmimage.h>

#include <vector>

namespace dicomreader {

struct VoiPreset
{
    enum class Kind { FileWindow, FileVoiLut, MinMax, Histogram };

    Kind kind;
    unsigned long fileIndex; ///< Índice no arquivo (FileWindow/FileVoiLut).
    QString name;
};

struct DicomImageDocument::Private
{
    QString filePath;
    // A DicomImage referencia o dataset: fileFormat precisa ser declarado
    // antes para ser destruído depois dela.
    std::unique_ptr<DcmFileFormat> fileFormat;
    std::unique_ptr<DicomImage> image;

    QList<MetadataEntry> metadata;
    QString photometricInterpretation;
    QString transferSyntax;
    QString modality;
    int bitsStored = 0;

    std::vector<VoiPreset> voiPresets;
    int defaultVoiPreset = -1;
    int activeVoiPreset = -1;
};

} // namespace dicomreader

#endif // DICOMREADER_CORE_DICOMIMAGEDOCUMENT_P_H
