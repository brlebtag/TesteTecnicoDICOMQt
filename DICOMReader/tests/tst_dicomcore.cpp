#include "core/dicomloader.h"
#include "dicomtestfiles.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcrleerg.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmjpeg/djencode.h>
#include <dcmtk/dcmjpls/djencode.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

using dicomreader::DicomImageDocument;
using dicomreader::DicomLoader;
using dicomreader::LoadErrorCode;
using dicomreader::WindowLevel;
using testfiles::MonochromeImage;

Q_DECLARE_METATYPE(E_TransferSyntax)

namespace {

/// Valor (0..255) de um pixel de uma imagem Format_Grayscale8.
int grayAt(const QImage &image, int x, int y)
{
    return image.constScanLine(y)[x];
}

/// Imagem rows x columns com valores distintos e crescentes na ordem de
/// leitura (linha a linha), de 0 a 4095.
MonochromeImage gradientImage(unsigned short rows, unsigned short columns)
{
    MonochromeImage image;
    image.rows = rows;
    image.columns = columns;
    const int count = rows * columns;
    for (int i = 0; i < count; ++i)
        image.pixels.push_back(static_cast<std::uint16_t>(i * 4095 / (count - 1)));
    return image;
}

std::unique_ptr<DicomImageDocument> loadOrFail(const QString &path)
{
    dicomreader::LoadResult result = DicomLoader::load(path);
    if (!result.isSuccess()) {
        qWarning() << "Falha ao carregar" << path << result.error().message << result.error().details;
        return nullptr;
    }
    return result.takeDocument();
}

} // namespace

class TestDicomCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Carregamento e atributos básicos
    void loadsMonochromeImage();
    void loadsDatasetWithoutFileMetaHeader();
    void loadsPathWithNonAsciiCharacters();

    // Exibição correta (orientação, escala de cinza, proporção)
    void keepsRowOrderAndOrientation();
    void handlesWidthNotMultipleOfFour();
    void usesWindowStoredInFile();
    void fallsBackToMinMaxWindowWithoutFileWindow();
    void appliesCustomWindow();
    void rejectsInvalidWindow();
    void invertsMonochrome1();
    void appliesRescaleSlopeAndIntercept();
    void supportsSignedPixels();
    void reportsPixelAspectRatio();
    void rendersEachFrameOfMultiFrameImage();

    // Imagens coloridas
    void rendersRgbImage();
    void convertsYbrFullToRgb();

    // Compressão
    void decodesCompressedTransferSyntaxes_data();
    void decodesCompressedTransferSyntaxes();
    void rejectsUnsupportedCompression();

    // Erros
    void reportsMissingFile();
    void reportsDirectory();
    void rejectsNonDicomFiles_data();
    void rejectsNonDicomFiles();
    void reportsDicomWithoutPixelData();
    void reportsMissingImageAttributes();
    void reportsTruncatedFile();

    // Metadados
    void extractsMetadata();
    void extractsNestedSequences();
    void convertsSpecificCharacterSet();

private:
    QString path(const QString &fileName) const { return m_dir.filePath(fileName); }
    QString writeMonochrome(const QString &fileName, const MonochromeImage &image);

    QTemporaryDir m_dir;
};

void TestDicomCore::initTestCase()
{
    QVERIFY2(m_dir.isValid(), "Não foi possível criar a pasta temporária.");
    // Codificadores usados só para gerar arquivos comprimidos de teste; os
    // decodificadores são registrados pelo próprio DicomLoader.
    DcmRLEEncoderRegistration::registerCodecs();
    DJEncoderRegistration::registerCodecs();
    DJLSEncoderRegistration::registerCodecs();
}

void TestDicomCore::cleanupTestCase()
{
    DJLSEncoderRegistration::cleanup();
    DJEncoderRegistration::cleanup();
    DcmRLEEncoderRegistration::cleanup();
}

QString TestDicomCore::writeMonochrome(const QString &fileName, const MonochromeImage &image)
{
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(image);
    const QString filePath = path(fileName);
    if (!testfiles::save(fileFormat, filePath))
        return {};
    return filePath;
}

// --- Carregamento e atributos básicos ---------------------------------------

void TestDicomCore::loadsMonochromeImage()
{
    const QString file = writeMonochrome(QStringLiteral("mono.dcm"), gradientImage(3, 4));
    QVERIFY(!file.isEmpty());

    const auto document = loadOrFail(file);
    QVERIFY(document);
    QCOMPARE(document->width(), 4);
    QCOMPARE(document->height(), 3);
    QCOMPARE(document->frameCount(), 1);
    QVERIFY(document->isMonochrome());
    QCOMPARE(document->photometricInterpretation(), QStringLiteral("MONOCHROME2"));
    QCOMPARE(document->bitsStored(), 12);
    QCOMPARE(document->modality(), QStringLiteral("OT"));
    QVERIFY(!document->transferSyntax().isEmpty());

    const QImage image = document->renderFrame();
    QCOMPARE(image.size(), QSize(4, 3));
    QCOMPARE(image.format(), QImage::Format_Grayscale8);
}

void TestDicomCore::loadsDatasetWithoutFileMetaHeader()
{
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(gradientImage(2, 2));
    const QString file = path(QStringLiteral("sem-cabecalho.dcm"));
    QVERIFY(testfiles::saveDatasetOnly(*fileFormat.getDataset(), file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    QCOMPARE(document->width(), 2);
}

void TestDicomCore::loadsPathWithNonAsciiCharacters()
{
    const QString folder = QStringLiteral("pasta com acentuação");
    QVERIFY(QDir(m_dir.path()).mkpath(folder));
    const QString file = writeMonochrome(folder + QStringLiteral("/imagem-ção.dcm"), gradientImage(2, 2));
    QVERIFY(!file.isEmpty());

    QVERIFY(loadOrFail(file));
}

// --- Exibição correta -------------------------------------------------------

void TestDicomCore::keepsRowOrderAndOrientation()
{
    // Valores crescem da esquerda para a direita e de cima para baixo: o
    // primeiro pixel do arquivo deve aparecer no canto superior esquerdo.
    const QString file = writeMonochrome(QStringLiteral("orientacao.dcm"), gradientImage(4, 4));
    const auto document = loadOrFail(file);
    QVERIFY(document);
    const auto minMax = document->voiPresetNames().indexOf(QStringLiteral("Automática (mín–máx)"));
    QVERIFY(document->applyVoiPreset(static_cast<int>(minMax)));

    const QImage image = document->renderFrame();
    QCOMPARE(grayAt(image, 0, 0), 0);
    QCOMPARE(grayAt(image, 3, 3), 255);
    int previous = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QVERIFY2(grayAt(image, x, y) > previous, "Os pixels não seguem a ordem do arquivo.");
            previous = grayAt(image, x, y);
        }
    }
}

void TestDicomCore::handlesWidthNotMultipleOfFour()
{
    // Largura 5: linhas de 5 bytes. Um stride errado "entorta" a imagem.
    MonochromeImage source;
    source.rows = 3;
    source.columns = 5;
    source.pixels = {0, 0, 0, 0, 4095,
                     0, 0, 0, 0, 4095,
                     0, 0, 0, 0, 4095};
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("largura5.dcm"), source));
    QVERIFY(document);

    const QImage image = document->renderFrame();
    for (int y = 0; y < 3; ++y) {
        QCOMPARE(grayAt(image, 0, y), 0);
        QCOMPARE(grayAt(image, 4, y), 255);
    }
}

void TestDicomCore::usesWindowStoredInFile()
{
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(gradientImage(4, 4));
    DcmDataset &dataset = *fileFormat.getDataset();
    dataset.putAndInsertString(DCM_WindowCenter, "2048");
    dataset.putAndInsertString(DCM_WindowWidth, "1000");
    dataset.putAndInsertString(DCM_WindowCenterWidthExplanation, "TESTE");
    const QString file = path(QStringLiteral("janela.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    QCOMPARE(document->defaultVoiPreset(), 0);
    QCOMPARE(document->activeVoiPreset(), 0);
    QVERIFY(document->voiPresetNames().first().contains(QStringLiteral("TESTE")));

    const auto window = document->windowLevel();
    QVERIFY(window.has_value());
    QCOMPARE(window->center, 2048.0);
    QCOMPARE(window->width, 1000.0);

    // Abaixo de centro - largura/2 fica preto; acima de centro + largura/2, branco.
    const QImage image = document->renderFrame();
    QCOMPARE(grayAt(image, 0, 0), 0);
    QCOMPARE(grayAt(image, 3, 3), 255);
}

void TestDicomCore::fallsBackToMinMaxWindowWithoutFileWindow()
{
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("sem-janela.dcm"), gradientImage(2, 2)));
    QVERIFY(document);
    QCOMPARE(document->voiPresetNames().at(document->activeVoiPreset()),
             QStringLiteral("Automática (mín–máx)"));

    const QImage image = document->renderFrame();
    QCOMPARE(grayAt(image, 0, 0), 0);
    QCOMPARE(grayAt(image, 1, 1), 255);
}

void TestDicomCore::appliesCustomWindow()
{
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("janela-manual.dcm"), gradientImage(4, 4)));
    QVERIFY(document);

    QVERIFY(document->setWindowLevel({100.0, 50.0}));
    QCOMPARE(document->activeVoiPreset(), -1);
    const auto window = document->windowLevel();
    QVERIFY(window.has_value());
    QCOMPARE(window->center, 100.0);
    QCOMPARE(window->width, 50.0);

    // Quase todos os pixels ficam acima da janela, logo brancos.
    const QImage image = document->renderFrame();
    QCOMPARE(grayAt(image, 0, 0), 0);
    QCOMPARE(grayAt(image, 2, 2), 255);

    QVERIFY(document->applyVoiPreset(document->defaultVoiPreset()));
    QCOMPARE(document->activeVoiPreset(), document->defaultVoiPreset());
}

void TestDicomCore::rejectsInvalidWindow()
{
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("janela-invalida.dcm"), gradientImage(2, 2)));
    QVERIFY(document);
    QVERIFY(!document->setWindowLevel({100.0, 0.0}));
    QVERIFY(!document->setWindowLevel({qQNaN(), 10.0}));
    QVERIFY(!document->applyVoiPreset(-1));
    QVERIFY(!document->applyVoiPreset(static_cast<int>(document->voiPresetNames().size())));
}

void TestDicomCore::invertsMonochrome1()
{
    // Em MONOCHROME1 o valor mínimo é exibido como branco.
    MonochromeImage source = gradientImage(2, 2);
    source.photometric = "MONOCHROME1";
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("mono1.dcm"), source));
    QVERIFY(document);

    const QImage image = document->renderFrame();
    QCOMPARE(grayAt(image, 0, 0), 255);
    QCOMPARE(grayAt(image, 1, 1), 0);
}

void TestDicomCore::appliesRescaleSlopeAndIntercept()
{
    // Valores armazenados 0, 1024, 2048 e 3072 viram -1024, 0, 1024 e 2048 HU.
    MonochromeImage source;
    source.rows = 2;
    source.columns = 2;
    source.pixels = {0, 1024, 2048, 3072};
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(source);
    fileFormat.getDataset()->putAndInsertString(DCM_RescaleIntercept, "-1024");
    fileFormat.getDataset()->putAndInsertString(DCM_RescaleSlope, "1");
    const QString file = path(QStringLiteral("rescale.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    const auto range = document->valueRange();
    QVERIFY(range.has_value());
    QCOMPARE(range->min, -1024.0);
    QCOMPARE(range->max, 2048.0);

    // Janela em HU: 0 HU fica no meio da escala de cinza.
    QVERIFY(document->setWindowLevel({0.0, 2048.0}));
    const QImage image = document->renderFrame();
    QCOMPARE(grayAt(image, 0, 0), 0);
    QVERIFY(qAbs(grayAt(image, 1, 0) - 128) <= 1);
    QCOMPARE(grayAt(image, 0, 1), 255);
}

void TestDicomCore::supportsSignedPixels()
{
    MonochromeImage source;
    source.rows = 1;
    source.columns = 3;
    source.isSigned = true;
    source.bitsStored = 16;
    source.pixels = {static_cast<std::uint16_t>(-500), 0, 500};
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("com-sinal.dcm"), source));
    QVERIFY(document);

    const auto range = document->valueRange();
    QVERIFY(range.has_value());
    QCOMPARE(range->min, -500.0);
    QCOMPARE(range->max, 500.0);

    const QImage image = document->renderFrame();
    QCOMPARE(grayAt(image, 0, 0), 0);
    QCOMPARE(grayAt(image, 2, 0), 255);
}

void TestDicomCore::reportsPixelAspectRatio()
{
    // Pixel Spacing = espaçamento entre linhas \ entre colunas: pixel com
    // 1,0 mm de largura e 0,5 mm de altura.
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(gradientImage(2, 2));
    fileFormat.getDataset()->putAndInsertString(DCM_PixelSpacing, "0.5\\1.0");
    const QString file = path(QStringLiteral("aspecto.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    QCOMPARE(document->pixelAspectRatio(), 2.0);

    const auto square = loadOrFail(writeMonochrome(QStringLiteral("quadrado.dcm"), gradientImage(2, 2)));
    QVERIFY(square);
    QCOMPARE(square->pixelAspectRatio(), 1.0);
}

void TestDicomCore::rendersEachFrameOfMultiFrameImage()
{
    MonochromeImage source;
    source.rows = 2;
    source.columns = 2;
    source.frames = 3;
    source.pixels = {0, 0, 0, 0, 2000, 2000, 2000, 2000, 4000, 4000, 4000, 4000};
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("multiframe.dcm"), source));
    QVERIFY(document);
    QCOMPARE(document->frameCount(), 3);

    QCOMPARE(grayAt(document->renderFrame(0), 1, 1), 0);
    QCOMPARE(grayAt(document->renderFrame(2), 1, 1), 255);
    const int middle = grayAt(document->renderFrame(1), 1, 1);
    QVERIFY(middle > 0 && middle < 255);
    QVERIFY(document->renderFrame(3).isNull());
    QVERIFY(document->renderFrame(-1).isNull());
}

// --- Imagens coloridas ------------------------------------------------------

void TestDicomCore::rendersRgbImage()
{
    DcmFileFormat fileFormat;
    testfiles::addIdentification(*fileFormat.getDataset());
    testfiles::setColorPixels(*fileFormat.getDataset(), 2, 2, "RGB",
                              {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255});
    const QString file = path(QStringLiteral("rgb.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    QVERIFY(!document->isMonochrome());
    QVERIFY(document->voiPresetNames().isEmpty());
    QVERIFY(!document->windowLevel().has_value());
    QVERIFY(!document->setWindowLevel({128.0, 256.0}));

    const QImage image = document->renderFrame();
    QCOMPARE(image.format(), QImage::Format_RGB888);
    QCOMPARE(image.pixel(0, 0), qRgb(255, 0, 0));
    QCOMPARE(image.pixel(1, 0), qRgb(0, 255, 0));
    QCOMPARE(image.pixel(0, 1), qRgb(0, 0, 255));
    QCOMPARE(image.pixel(1, 1), qRgb(255, 255, 255));
}

void TestDicomCore::convertsYbrFullToRgb()
{
    // Vermelho puro em YCbCr (ITU-R BT.601, faixa completa): Y=76, Cb=85, Cr=255.
    DcmFileFormat fileFormat;
    testfiles::addIdentification(*fileFormat.getDataset());
    testfiles::setColorPixels(*fileFormat.getDataset(), 1, 1, "YBR_FULL", {76, 85, 255});
    const QString file = path(QStringLiteral("ybr.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    const QRgb color = document->renderFrame().pixel(0, 0);
    QVERIFY2(qRed(color) >= 250 && qGreen(color) <= 5 && qBlue(color) <= 5,
             qPrintable(QStringLiteral("cor obtida: %1,%2,%3")
                            .arg(qRed(color)).arg(qGreen(color)).arg(qBlue(color))));
}

// --- Compressão -------------------------------------------------------------

void TestDicomCore::decodesCompressedTransferSyntaxes_data()
{
    QTest::addColumn<E_TransferSyntax>("xfer");
    QTest::newRow("RLE") << EXS_RLELossless;
    QTest::newRow("JPEG sem perdas") << EXS_JPEGProcess14SV1;
    QTest::newRow("JPEG-LS sem perdas") << EXS_JPEGLSLossless;
}

void TestDicomCore::decodesCompressedTransferSyntaxes()
{
    QFETCH(E_TransferSyntax, xfer);

    const MonochromeImage source = gradientImage(8, 8);
    const auto reference = loadOrFail(writeMonochrome(QStringLiteral("referencia.dcm"), source));
    QVERIFY(reference);

    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(source);
    const QString file = path(QStringLiteral("comprimido-%1.dcm").arg(static_cast<int>(xfer)));
    QVERIFY2(testfiles::save(fileFormat, file, xfer), "Falha ao gerar o arquivo comprimido.");

    const auto document = loadOrFail(file);
    QVERIFY(document);
    QCOMPARE(document->transferSyntax(), QString::fromLatin1(DcmXfer(xfer).getXferName()));
    // Compressão sem perdas: a imagem renderizada é idêntica à original.
    QCOMPARE(document->renderFrame(), reference->renderFrame());
}

void TestDicomCore::rejectsUnsupportedCompression()
{
    const QString file = path(QStringLiteral("jpeg2000.dcm"));
    QVERIFY(testfiles::saveWithFakeEncapsulatedPixels(file, EXS_JPEG2000LosslessOnly));

    dicomreader::LoadResult result = DicomLoader::load(file);
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.error().code, LoadErrorCode::UnsupportedTransferSyntax);
    QVERIFY(result.error().message.contains(QStringLiteral("JPEG 2000")));
}

// --- Erros ------------------------------------------------------------------

void TestDicomCore::reportsMissingFile()
{
    dicomreader::LoadResult result = DicomLoader::load(path(QStringLiteral("nao-existe.dcm")));
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.error().code, LoadErrorCode::FileNotFound);
    QVERIFY(!result.error().message.isEmpty());
}

void TestDicomCore::reportsDirectory()
{
    dicomreader::LoadResult result = DicomLoader::load(m_dir.path());
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.error().code, LoadErrorCode::NotAFile);
}

void TestDicomCore::rejectsNonDicomFiles_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("vazio") << QByteArray();
    QTest::newRow("texto") << QByteArray("Isto não é um arquivo DICOM.\nSegunda linha.\n");
    QTest::newRow("binário") << QByteArray(4096, '\xAB');
    QTest::newRow("PNG") << QByteArray("\x89PNG\r\n\x1a\n\0\0\0\rIHDR", 16);
}

void TestDicomCore::rejectsNonDicomFiles()
{
    QFETCH(QByteArray, content);

    QFile file(path(QStringLiteral("invalido-%1.dcm").arg(QTest::currentDataTag())));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();

    dicomreader::LoadResult result = DicomLoader::load(file.fileName());
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.error().code, LoadErrorCode::NotDicom);
}

void TestDicomCore::reportsDicomWithoutPixelData()
{
    DcmFileFormat fileFormat;
    testfiles::addIdentification(*fileFormat.getDataset());
    const QString file = path(QStringLiteral("sem-imagem.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    dicomreader::LoadResult result = DicomLoader::load(file);
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.error().code, LoadErrorCode::NoPixelData);
}

void TestDicomCore::reportsMissingImageAttributes()
{
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(gradientImage(2, 2));
    delete fileFormat.getDataset()->remove(DCM_Columns);
    const QString file = path(QStringLiteral("sem-colunas.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    dicomreader::LoadResult result = DicomLoader::load(file);
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.error().code, LoadErrorCode::InvalidImage);
    QVERIFY(!result.error().details.isEmpty());
}

void TestDicomCore::reportsTruncatedFile()
{
    const QString file = writeMonochrome(QStringLiteral("truncado.dcm"), gradientImage(64, 64));
    QVERIFY(!file.isEmpty());
    QFile handle(file);
    QVERIFY(handle.resize(handle.size() / 2));

    dicomreader::LoadResult result = DicomLoader::load(file);
    QVERIFY(!result.isSuccess());
    QVERIFY(result.error().code == LoadErrorCode::NotDicom
            || result.error().code == LoadErrorCode::InvalidImage);
}

// --- Metadados --------------------------------------------------------------

namespace {

const dicomreader::MetadataEntry *findEntry(const QList<dicomreader::MetadataEntry> &entries,
                                            const QString &tag)
{
    const auto it = std::find_if(entries.cbegin(), entries.cend(),
                                 [&tag](const dicomreader::MetadataEntry &e) { return e.tag == tag; });
    return it == entries.cend() ? nullptr : &*it;
}

} // namespace

void TestDicomCore::extractsMetadata()
{
    const auto document = loadOrFail(writeMonochrome(QStringLiteral("metadados.dcm"), gradientImage(3, 4)));
    QVERIFY(document);
    const auto &entries = document->metadata();

    const auto *transferSyntax = findEntry(entries, QStringLiteral("(0002,0010)"));
    QVERIFY2(transferSyntax, "Cabeçalho (grupo 0002) ausente.");

    const auto *patientName = findEntry(entries, QStringLiteral("(0010,0010)"));
    QVERIFY(patientName);
    QCOMPARE(patientName->vr, QStringLiteral("PN"));
    QCOMPARE(patientName->name, QStringLiteral("PatientName"));
    QCOMPARE(patientName->value, QStringLiteral("Teste^Paciente"));

    const auto *rows = findEntry(entries, QStringLiteral("(0028,0010)"));
    QVERIFY(rows);
    QCOMPARE(rows->value, QStringLiteral("3"));

    const auto *pixelData = findEntry(entries, QStringLiteral("(7fe0,0010)"));
    QVERIFY(pixelData);
    QVERIFY2(pixelData->value.contains(QStringLiteral("bytes")), qPrintable(pixelData->value));
}

void TestDicomCore::extractsNestedSequences()
{
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(gradientImage(2, 2));
    DcmItem *item = nullptr;
    QVERIFY(fileFormat.getDataset()
                ->findOrCreateSequenceItem(DCM_ReferencedImageSequence, item, -2)
                .good());
    item->putAndInsertString(DCM_ReferencedSOPInstanceUID, "1.2.3.4");
    const QString file = path(QStringLiteral("sequencia.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    const auto &entries = document->metadata();

    const auto *sequence = findEntry(entries, QStringLiteral("(0008,1140)"));
    QVERIFY(sequence);
    QCOMPARE(sequence->vr, QStringLiteral("SQ"));
    QCOMPARE(sequence->value, QStringLiteral("1 item"));

    const auto *nested = findEntry(entries, QStringLiteral("(0008,1155)"));
    QVERIFY(nested);
    QCOMPARE(nested->depth, sequence->depth + 2);
    QCOMPARE(nested->value, QStringLiteral("1.2.3.4"));
}

void TestDicomCore::convertsSpecificCharacterSet()
{
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(gradientImage(2, 2));
    DcmDataset &dataset = *fileFormat.getDataset();
    dataset.putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 100");
    dataset.putAndInsertString(DCM_PatientName, "Jos\xE9^Concei\xE7\xE3o"); // Latin-1
    const QString file = path(QStringLiteral("latin1.dcm"));
    QVERIFY(testfiles::save(fileFormat, file));

    const auto document = loadOrFail(file);
    QVERIFY(document);
    const auto *patientName = findEntry(document->metadata(), QStringLiteral("(0010,0010)"));
    QVERIFY(patientName);
    QCOMPARE(patientName->value, QStringLiteral("José^Conceição"));
}

QTEST_GUILESS_MAIN(TestDicomCore)
#include "tst_dicomcore.moc"
