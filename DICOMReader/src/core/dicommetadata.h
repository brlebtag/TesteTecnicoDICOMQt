#ifndef DICOMREADER_CORE_DICOMMETADATA_H
#define DICOMREADER_CORE_DICOMMETADATA_H

#include <QList>
#include <QString>

class DcmFileFormat;

namespace dicomreader {

/// Um atributo DICOM pronto para exibição.
struct MetadataEntry
{
    QString tag;   ///< "(gggg,eeee)"; vazio para marcadores de item de sequência.
    QString vr;    ///< Value Representation (ex.: "US", "PN").
    QString name;  ///< Nome do atributo no dicionário DICOM.
    QString value; ///< Valor formatado; dados binários aparecem resumidos.
    int depth = 0; ///< Nível de aninhamento dentro de sequências (0 = raiz).
};

/// Extrai os atributos do cabeçalho (grupo 0002) e do dataset, na ordem do
/// arquivo, percorrendo sequências recursivamente.
QList<MetadataEntry> extractMetadata(DcmFileFormat &fileFormat);

} // namespace dicomreader

#endif // DICOMREADER_CORE_DICOMMETADATA_H
