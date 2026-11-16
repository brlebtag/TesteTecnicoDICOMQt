#include "dicomtestfiles.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmdata/dcuid.h>

#include <QFile>

#include <string>

namespace testfiles {
namespace {

OFFilename toDcmtkFilename(const QString &path)
{
#if defined(_WIN32) && defined(WIDE_CHAR_FILE_IO_FUNCTIONS)
    const std::wstring widePath = path.toStdWString();
    return OFFilename(widePath.c_str(), OFTrue);
#else
    return OFFilename(QFile::encodeName(path).constData(), OFTrue);
#endif
}

void setPixelModule(DcmDataset &dataset, unsigned short rows, unsigned short columns,
                    unsigned short samplesPerPixel, const char *photometric,
                    unsigned short bitsAllocated, unsigned short bitsStored, bool isSigned)
{
    dataset.putAndInsertUint16(DCM_SamplesPerPixel, samplesPerPixel);
    dataset.putAndInsertString(DCM_PhotometricInterpretation, photometric);
    dataset.putAndInsertUint16(DCM_Rows, rows);
    dataset.putAndInsertUint16(DCM_Columns, columns);
    dataset.putAndInsertUint16(DCM_BitsAllocated, bitsAllocated);
    dataset.putAndInsertUint16(DCM_BitsStored, bitsStored);
    dataset.putAndInsertUint16(DCM_HighBit, static_cast<Uint16>(bitsStored - 1));
    dataset.putAndInsertUint16(DCM_PixelRepresentation, isSigned ? 1 : 0);
}

} // namespace

void addIdentification(DcmDataset &dataset)
{
    char uid[100];
    dataset.putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
    dataset.putAndInsertString(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT));
    dataset.putAndInsertString(DCM_Modality, "OT");
    dataset.putAndInsertString(DCM_PatientName, "Teste^Paciente");
    dataset.putAndInsertString(DCM_PatientID, "12345");
}

void setMonochromePixels(DcmDataset &dataset, const MonochromeImage &image)
{
    setPixelModule(dataset, image.rows, image.columns, 1, image.photometric, 16, image.bitsStored,
                   image.isSigned);
    if (image.frames > 1)
        dataset.putAndInsertString(DCM_NumberOfFrames, std::to_string(image.frames).c_str());
    dataset.putAndInsertUint16Array(DCM_PixelData, image.pixels.data(),
                                    static_cast<unsigned long>(image.pixels.size()));
}

void setColorPixels(DcmDataset &dataset, unsigned short rows, unsigned short columns,
                    const char *photometric, const std::vector<std::uint8_t> &samples)
{
    setPixelModule(dataset, rows, columns, 3, photometric, 8, 8, false);
    dataset.putAndInsertUint16(DCM_PlanarConfiguration, 0);
    dataset.putAndInsertUint8Array(DCM_PixelData, samples.data(),
                                   static_cast<unsigned long>(samples.size()));
}

DcmFileFormat makeMonochromeFile(const MonochromeImage &image)
{
    DcmFileFormat fileFormat;
    addIdentification(*fileFormat.getDataset());
    setMonochromePixels(*fileFormat.getDataset(), image);
    return fileFormat;
}

bool save(DcmFileFormat &fileFormat, const QString &path, E_TransferSyntax xfer)
{
    DcmDataset &dataset = *fileFormat.getDataset();
    if (dataset.chooseRepresentation(xfer, nullptr).bad() || !dataset.canWriteXfer(xfer))
        return false;
    return fileFormat.saveFile(toDcmtkFilename(path), xfer).good();
}

bool saveDatasetOnly(DcmDataset &dataset, const QString &path)
{
    return dataset.saveFile(toDcmtkFilename(path), EXS_LittleEndianImplicit).good();
}

bool saveWithFakeEncapsulatedPixels(const QString &path, E_TransferSyntax xfer)
{
    DcmFileFormat fileFormat;
    DcmDataset &dataset = *fileFormat.getDataset();
    addIdentification(dataset);
    dataset.putAndInsertUint16(DCM_SamplesPerPixel, 1);
    dataset.putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
    dataset.putAndInsertUint16(DCM_Rows, 2);
    dataset.putAndInsertUint16(DCM_Columns, 2);
    dataset.putAndInsertUint16(DCM_BitsAllocated, 8);
    dataset.putAndInsertUint16(DCM_BitsStored, 8);
    dataset.putAndInsertUint16(DCM_HighBit, 7);
    dataset.putAndInsertUint16(DCM_PixelRepresentation, 0);

    auto *sequence = new DcmPixelSequence(DCM_PixelSequenceTag);
    sequence->insert(new DcmPixelItem(DCM_PixelItemTag)); // tabela de offsets vazia
    auto *fragment = new DcmPixelItem(DCM_PixelItemTag);
    const Uint8 bytes[] = {0xFF, 0x4F, 0xFF, 0x51, 0x00, 0x00, 0x00, 0x00};
    fragment->putUint8Array(bytes, sizeof(bytes));
    sequence->insert(fragment);

    auto *pixelData = new DcmPixelData(DCM_PixelData);
    pixelData->putOriginalRepresentation(xfer, nullptr, sequence);
    dataset.insert(pixelData);

    return fileFormat.saveFile(toDcmtkFilename(path), xfer).good();
}

} // namespace testfiles
