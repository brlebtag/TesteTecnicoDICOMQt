#include "dicomloader.h"
#include "dicomimagedocument_p.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/dcmimage/diregist.h> // habilita imagens coloridas na DicomImage
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpls/djdecode.h>
#include <dcmtk/oflog/oflog.h>

#include <QFile>
#include <QFileInfo>

#include <exception>
#include <new>
#include <string>

namespace dicomreader {
namespace {

/// Configuração global da DCMTK, feita uma única vez: registra os
/// decodificadores RLE, JPEG e JPEG-LS e reduz o log ao nível de erro (as
/// falhas chegam ao usuário pelos LoadError).
class DcmtkSetup
{
public:
    DcmtkSetup()
    {
        OFLog::configure(OFLogger::ERROR_LOG_LEVEL);
        DcmRLEDecoderRegistration::registerCodecs();
        DJDecoderRegistration::registerCodecs();
        DJLSDecoderRegistration::registerCodecs();
    }

    ~DcmtkSetup()
    {
        DJLSDecoderRegistration::cleanup();
        DJDecoderRegistration::cleanup();
        DcmRLEDecoderRegistration::cleanup();
    }

    DcmtkSetup(const DcmtkSetup &) = delete;
    DcmtkSetup &operator=(const DcmtkSetup &) = delete;
};

void ensureDcmtkInitialized()
{
    static const DcmtkSetup setup;
}

OFFilename toDcmtkFilename(const QString &path)
{
#if defined(_WIN32) && defined(WIDE_CHAR_FILE_IO_FUNCTIONS)
    // Caminhos com acentos no Windows exigem a API de arquivos em UTF-16.
    const std::wstring widePath = path.toStdWString();
    return OFFilename(widePath.c_str(), OFTrue);
#elif defined(_WIN32)
    return OFFilename(path.toLocal8Bit().constData(), OFTrue);
#else
    return OFFilename(QFile::encodeName(path).constData(), OFTrue);
#endif
}

LoadResult fail(LoadErrorCode code, const QString &details = {})
{
    return LoadResult::failure({code, defaultMessage(code), details});
}

QString conditionText(const OFCondition &condition)
{
    return QString::fromLatin1(condition.text());
}

QString findString(DcmItem &item, const DcmTagKey &tag)
{
    OFString value;
    item.findAndGetOFString(tag, value);
    return QString::fromLatin1(value.c_str()).trimmed();
}

/// Arquivos sem preâmbulo/cabeçalho DICOM são aceitos (a DCMTK lê o dataset
/// "cru"), mas só quando trazem atributos que um DICOM de imagem teria. Isso
/// evita interpretar um arquivo qualquer como dataset.
bool looksLikeDicom(DcmFileFormat &fileFormat)
{
    DcmMetaInfo *metaInfo = fileFormat.getMetaInfo();
    if (metaInfo != nullptr && metaInfo->tagExistsWithValue(DCM_TransferSyntaxUID))
        return true;

    DcmDataset *dataset = fileFormat.getDataset();
    return dataset != nullptr
           && (dataset->tagExistsWithValue(DCM_SOPClassUID)
               || (dataset->tagExistsWithValue(DCM_Rows) && dataset->tagExistsWithValue(DCM_Columns)));
}

} // namespace

LoadResult DicomLoader::loadDocument(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists())
        return fail(LoadErrorCode::FileNotFound);
    if (!info.isFile())
        return fail(LoadErrorCode::NotAFile);
    if (!info.isReadable())
        return fail(LoadErrorCode::FileNotReadable);

    auto d = std::make_unique<DicomImageDocument::Private>();
    d->filePath = info.absoluteFilePath();
    d->fileFormat = std::make_unique<DcmFileFormat>();

    const OFCondition readStatus = d->fileFormat->loadFile(toDcmtkFilename(filePath));
    if (readStatus == EC_UnsupportedEncoding)
        return fail(LoadErrorCode::UnsupportedTransferSyntax, conditionText(readStatus));
    if (readStatus.bad())
        return fail(LoadErrorCode::NotDicom, conditionText(readStatus));
    if (!looksLikeDicom(*d->fileFormat))
        return fail(LoadErrorCode::NotDicom);

    DcmDataset &dataset = *d->fileFormat->getDataset();
    if (!dataset.tagExistsWithValue(DCM_PixelData)) {
        if (dataset.tagExists(DCM_FloatPixelData) || dataset.tagExists(DCM_DoubleFloatPixelData)) {
            return fail(LoadErrorCode::InvalidImage,
                        QStringLiteral("Pixels em ponto flutuante não são suportados."));
        }
        return fail(LoadErrorCode::NoPixelData);
    }

    const DcmXfer originalXfer(dataset.getOriginalXfer());
    d->transferSyntax = QString::fromLatin1(originalXfer.getXferName());
    d->photometricInterpretation = findString(dataset, DCM_PhotometricInterpretation);
    d->modality = findString(dataset, DCM_Modality);
    Uint16 bitsStored = 0;
    dataset.findAndGetUint16(DCM_BitsStored, bitsStored);
    d->bitsStored = bitsStored;

    // Metadados extraídos antes da descompressão, para refletirem o arquivo
    // como está no disco. Textos convertidos para UTF-8 conforme o Specific
    // Character Set; se a conversão falhar, os valores originais são mantidos.
    d->fileFormat->convertToUTF8();
    d->metadata = extractMetadata(*d->fileFormat);

    if (originalXfer.isPixelDataCompressed()) {
        const OFCondition decodeStatus = dataset.chooseRepresentation(EXS_LittleEndianExplicit, nullptr);
        if (decodeStatus.bad() || !dataset.canWriteXfer(EXS_LittleEndianExplicit)) {
            LoadError error{LoadErrorCode::UnsupportedTransferSyntax,
                            QStringLiteral("A compressão da imagem (%1) não é suportada.")
                                .arg(d->transferSyntax),
                            conditionText(decodeStatus)};
            return LoadResult::failure(std::move(error));
        }
    }

    // A DicomImage converte os pixels para a sua representação interna; com
    // CIF_MayDetachPixelData ela libera a cópia que ficaria no dataset.
    d->image = std::make_unique<DicomImage>(d->fileFormat.get(), dataset.getCurrentXfer(),
                                            CIF_MayDetachPixelData);
    const EI_Status imageStatus = d->image->getStatus();
    if (imageStatus != EIS_Normal)
        return fail(LoadErrorCode::InvalidImage, QString::fromLatin1(DicomImage::getString(imageStatus)));
    if (d->image->getFrameCount() == 0)
        return fail(LoadErrorCode::InvalidImage, QStringLiteral("A imagem não possui frames."));

    return LoadResult::success(std::unique_ptr<DicomImageDocument>(new DicomImageDocument(std::move(d))));
}

LoadResult LoadResult::success(std::unique_ptr<DicomImageDocument> document)
{
    return LoadResult(std::move(document));
}

LoadResult LoadResult::failure(LoadError error)
{
    return LoadResult(std::move(error));
}

LoadResult::LoadResult(std::variant<std::unique_ptr<DicomImageDocument>, LoadError> value)
    : m_value(std::move(value))
{
}

bool LoadResult::isSuccess() const
{
    return std::holds_alternative<std::unique_ptr<DicomImageDocument>>(m_value);
}

std::unique_ptr<DicomImageDocument> LoadResult::takeDocument()
{
    return std::move(std::get<std::unique_ptr<DicomImageDocument>>(m_value));
}

const LoadError &LoadResult::error() const
{
    return std::get<LoadError>(m_value);
}

QString DicomLoader::dcmtkVersion()
{
    return QStringLiteral(OFFIS_DCMTK_VERSION_STRING);
}

LoadResult DicomLoader::load(const QString &filePath)
{
    // A DCMTK não lança exceções, mas alocações grandes podem lançar
    // std::bad_alloc; nada escapa daqui para a interface.
    try {
        ensureDcmtkInitialized();
        return loadDocument(filePath);
    } catch (const std::bad_alloc &) {
        return fail(LoadErrorCode::InvalidImage,
                    QStringLiteral("Memória insuficiente para carregar a imagem."));
    } catch (const std::exception &e) {
        return fail(LoadErrorCode::InvalidImage, QString::fromLocal8Bit(e.what()));
    }
}

} // namespace dicomreader
