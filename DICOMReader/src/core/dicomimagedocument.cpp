#include "dicomimagedocument.h"
#include "dicomimagedocument_p.h"

#include <dcmtk/dcmdata/dcdeftag.h>

#include <cmath>

namespace dicomreader {
namespace {

/// Porcentagem das extremidades do histograma ignorada na janela automática.
constexpr double kHistogramThreshold = 0.05;

QString formatNumber(double value)
{
    return QString::number(value, 'g', 6);
}

QString fileWindowName(DicomImage &image, DcmDataset &dataset, unsigned long index)
{
    OFString explanation;
    image.getVoiWindowExplanation(index, explanation);
    QString name = explanation.empty()
                       ? QStringLiteral("Janela %1 do arquivo").arg(index + 1)
                       : QStringLiteral("Arquivo: %1").arg(QString::fromLatin1(explanation.c_str()));

    Float64 center = 0.0;
    Float64 width = 0.0;
    if (dataset.findAndGetFloat64(DCM_WindowCenter, center, index).good()
        && dataset.findAndGetFloat64(DCM_WindowWidth, width, index).good()) {
        name += QStringLiteral(" (centro %1, largura %2)").arg(formatNumber(center), formatNumber(width));
    }
    return name;
}

QString fileVoiLutName(DicomImage &image, unsigned long index)
{
    OFString explanation;
    image.getVoiLutExplanation(index, explanation);
    return explanation.empty()
               ? QStringLiteral("VOI LUT %1 do arquivo").arg(index + 1)
               : QStringLiteral("Arquivo: %1").arg(QString::fromLatin1(explanation.c_str()));
}

} // namespace

DicomImageDocument::DicomImageDocument(std::unique_ptr<Private> priv)
    : d(std::move(priv))
{
    if (!isMonochrome())
        return;

    DicomImage &image = *d->image;
    DcmDataset &dataset = *d->fileFormat->getDataset();

    for (unsigned long i = 0; i < image.getWindowCount(); ++i)
        d->voiPresets.push_back({VoiPreset::Kind::FileWindow, i, fileWindowName(image, dataset, i)});
    for (unsigned long i = 0; i < image.getVoiLutCount(); ++i)
        d->voiPresets.push_back({VoiPreset::Kind::FileVoiLut, i, fileVoiLutName(image, i)});
    d->voiPresets.push_back({VoiPreset::Kind::MinMax, 0, QStringLiteral("Automática (mín–máx)")});
    d->voiPresets.push_back(
        {VoiPreset::Kind::Histogram, 0, QStringLiteral("Automática (histograma)")});

    // A janela inicial é a primeira do arquivo; sem nenhuma válida, a automática.
    for (int i = 0; i < static_cast<int>(d->voiPresets.size()); ++i) {
        if (applyVoiPreset(i)) {
            d->defaultVoiPreset = i;
            break;
        }
    }
}

DicomImageDocument::~DicomImageDocument() = default;

QString DicomImageDocument::filePath() const
{
    return d->filePath;
}

int DicomImageDocument::width() const
{
    return static_cast<int>(d->image->getWidth());
}

int DicomImageDocument::height() const
{
    return static_cast<int>(d->image->getHeight());
}

int DicomImageDocument::frameCount() const
{
    return static_cast<int>(d->image->getFrameCount());
}

bool DicomImageDocument::isMonochrome() const
{
    return d->image->isMonochrome() != 0;
}

double DicomImageDocument::pixelAspectRatio() const
{
    const double ratio = d->image->getWidthHeightRatio();
    return (std::isfinite(ratio) && ratio > 0.0) ? ratio : 1.0;
}

QString DicomImageDocument::photometricInterpretation() const
{
    return d->photometricInterpretation;
}

QString DicomImageDocument::transferSyntax() const
{
    return d->transferSyntax;
}

QString DicomImageDocument::modality() const
{
    return d->modality;
}

int DicomImageDocument::bitsStored() const
{
    return d->bitsStored;
}

QStringList DicomImageDocument::voiPresetNames() const
{
    QStringList names;
    names.reserve(static_cast<qsizetype>(d->voiPresets.size()));
    for (const VoiPreset &preset : d->voiPresets)
        names.append(preset.name);
    return names;
}

int DicomImageDocument::defaultVoiPreset() const
{
    return d->defaultVoiPreset;
}

int DicomImageDocument::activeVoiPreset() const
{
    return d->activeVoiPreset;
}

bool DicomImageDocument::applyVoiPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(d->voiPresets.size()))
        return false;

    const VoiPreset &preset = d->voiPresets[static_cast<std::size_t>(index)];
    DicomImage &image = *d->image;
    int result = 0;
    switch (preset.kind) {
    case VoiPreset::Kind::FileWindow:
        result = image.setWindow(preset.fileIndex);
        break;
    case VoiPreset::Kind::FileVoiLut:
        result = image.setVoiLut(preset.fileIndex);
        break;
    case VoiPreset::Kind::MinMax:
        result = image.setMinMaxWindow();
        break;
    case VoiPreset::Kind::Histogram:
        result = image.setHistogramWindow(kHistogramThreshold);
        break;
    }

    if (result == 0)
        return false;
    d->activeVoiPreset = index;
    return true;
}

std::optional<WindowLevel> DicomImageDocument::windowLevel() const
{
    if (!isMonochrome())
        return std::nullopt;

    WindowLevel window;
    if (d->image->getWindow(window.center, window.width) == 0)
        return std::nullopt;
    return window;
}

bool DicomImageDocument::setWindowLevel(const WindowLevel &window)
{
    if (!isMonochrome() || !std::isfinite(window.center) || !std::isfinite(window.width)
        || window.width < 1.0) {
        return false;
    }
    if (d->image->setWindow(window.center, window.width) == 0)
        return false;
    d->activeVoiPreset = -1;
    return true;
}

std::optional<ValueRange> DicomImageDocument::valueRange() const
{
    ValueRange range;
    if (d->image->getMinMaxValues(range.min, range.max) == 0)
        return std::nullopt;
    return range;
}

QImage DicomImageDocument::renderFrame(int frame) const
{
    if (frame < 0 || frame >= frameCount())
        return {};

    constexpr int kOutputBits = 8;
    const auto *pixels = static_cast<const uchar *>(
        d->image->getOutputData(kOutputBits, static_cast<unsigned long>(frame)));
    if (pixels == nullptr)
        return {};

    // A DCMTK entrega as linhas de cima para baixo, sem preenchimento:
    // 1 byte por pixel (monocromática) ou RGB intercalado (colorida).
    const bool monochrome = isMonochrome();
    const qsizetype bytesPerLine = static_cast<qsizetype>(width()) * (monochrome ? 1 : 3);
    const QImage::Format format = monochrome ? QImage::Format_Grayscale8 : QImage::Format_RGB888;

    // A QImage apenas referencia o buffer interno da DCMTK, que é reescrito a
    // cada renderização; copy() torna a imagem independente dele.
    return QImage(pixels, width(), height(), bytesPerLine, format).copy();
}

const QList<MetadataEntry> &DicomImageDocument::metadata() const
{
    return d->metadata;
}

} // namespace dicomreader
