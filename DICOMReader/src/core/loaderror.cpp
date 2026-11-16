#include "loaderror.h"

namespace dicomreader {

QString defaultMessage(LoadErrorCode code)
{
    switch (code) {
    case LoadErrorCode::FileNotFound:
        return QStringLiteral("O arquivo não foi encontrado.");
    case LoadErrorCode::NotAFile:
        return QStringLiteral("O caminho selecionado não é um arquivo.");
    case LoadErrorCode::FileNotReadable:
        return QStringLiteral("Sem permissão para ler o arquivo.");
    case LoadErrorCode::NotDicom:
        return QStringLiteral("O arquivo não é um DICOM válido ou está corrompido.");
    case LoadErrorCode::NoPixelData:
        return QStringLiteral("O arquivo DICOM não contém imagem "
                              "(por exemplo, DICOMDIR ou relatório estruturado).");
    case LoadErrorCode::UnsupportedTransferSyntax:
        return QStringLiteral("A compressão da imagem não é suportada.");
    case LoadErrorCode::InvalidImage:
        return QStringLiteral("Não foi possível processar a imagem DICOM.");
    }
    return QStringLiteral("Erro desconhecido ao abrir o arquivo.");
}

} // namespace dicomreader
