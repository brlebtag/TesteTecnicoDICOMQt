#ifndef DICOMREADER_CORE_DICOMLOADER_H
#define DICOMREADER_CORE_DICOMLOADER_H

#include "dicomimagedocument.h"
#include "loaderror.h"

#include <QString>

#include <memory>
#include <variant>

namespace dicomreader {

/// Resultado de DicomLoader::load(): ou o documento carregado, ou o erro.
class LoadResult
{
public:
    static LoadResult success(std::unique_ptr<DicomImageDocument> document);
    static LoadResult failure(LoadError error);

    bool isSuccess() const;

    /// Transfere a posse do documento. Pré-condição: isSuccess().
    std::unique_ptr<DicomImageDocument> takeDocument();

    /// Pré-condição: !isSuccess().
    const LoadError &error() const;

private:
    explicit LoadResult(std::variant<std::unique_ptr<DicomImageDocument>, LoadError> value);

    std::variant<std::unique_ptr<DicomImageDocument>, LoadError> m_value;
};

/// Abre arquivos DICOM com a DCMTK e os prepara para exibição.
class DicomLoader
{
public:
    /// Lê e valida o arquivo, descomprime a imagem quando necessário e extrai
    /// os metadados. Nunca lança exceções: falhas voltam como LoadError.
    static LoadResult load(const QString &filePath);

private:
    static LoadResult loadDocument(const QString &filePath);
};

} // namespace dicomreader

#endif // DICOMREADER_CORE_DICOMLOADER_H
