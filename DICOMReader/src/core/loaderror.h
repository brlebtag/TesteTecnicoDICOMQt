#ifndef DICOMREADER_CORE_LOADERROR_H
#define DICOMREADER_CORE_LOADERROR_H

#include <QString>

namespace dicomreader {

/// Motivos pelos quais um arquivo não pôde ser aberto como imagem DICOM.
enum class LoadErrorCode {
    FileNotFound,              ///< O caminho não existe.
    NotAFile,                  ///< O caminho existe, mas não é um arquivo (ex.: pasta).
    FileNotReadable,           ///< Sem permissão de leitura.
    NotDicom,                  ///< O conteúdo não é um arquivo DICOM válido.
    NoPixelData,               ///< É DICOM, mas não contém imagem (ex.: DICOMDIR, relatório).
    UnsupportedTransferSyntax, ///< Compressão não suportada (ex.: JPEG 2000).
    InvalidImage,              ///< Atributos de imagem ausentes, inválidos ou não suportados.
};

/// Erro de carregamento: mensagem para o usuário e detalhe técnico opcional.
struct LoadError
{
    LoadErrorCode code;
    QString message; ///< Texto em português, pronto para exibir na interface.
    QString details; ///< Detalhe técnico (ex.: mensagem da DCMTK); pode ser vazio.
};

/// Mensagem padrão, em português, para cada código de erro.
QString defaultMessage(LoadErrorCode code);

} // namespace dicomreader

#endif // DICOMREADER_CORE_LOADERROR_H
