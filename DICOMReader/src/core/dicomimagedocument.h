#ifndef DICOMREADER_CORE_DICOMIMAGEDOCUMENT_H
#define DICOMREADER_CORE_DICOMIMAGEDOCUMENT_H

#include "dicommetadata.h"

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace dicomreader {

/// Janela VOI (Window Center / Window Width), nas unidades após a
/// transformação de modalidade (ex.: Hounsfield em CT).
struct WindowLevel
{
    double center = 0.0;
    double width = 1.0;
};

/// Faixa de valores de pixel presentes na imagem, após a transformação de modalidade.
struct ValueRange
{
    double min = 0.0;
    double max = 0.0;
};

/// Imagem DICOM carregada e pronta para exibição.
///
/// Encapsula o arquivo lido pela DCMTK e o pipeline de renderização dela
/// (Modality LUT, VOI LUT/janela, Presentation LUT, MONOCHROME1, cor).
/// Instâncias são criadas por DicomLoader::load().
class DicomImageDocument
{
public:
    ~DicomImageDocument();

    DicomImageDocument(const DicomImageDocument &) = delete;
    DicomImageDocument &operator=(const DicomImageDocument &) = delete;

    QString filePath() const;

    int width() const;
    int height() const;
    int frameCount() const;
    bool isMonochrome() const;

    /// Proporção largura/altura de um pixel (1.0 = pixel quadrado), obtida de
    /// Pixel Spacing, Imager Pixel Spacing ou Pixel Aspect Ratio.
    double pixelAspectRatio() const;

    QString photometricInterpretation() const;
    QString transferSyntax() const;
    QString modality() const;
    int bitsStored() const;

    /// Janelas disponíveis: as gravadas no arquivo (Window Center/Width e
    /// VOI LUT) seguidas das automáticas (mín–máx e histograma).
    /// Lista vazia para imagens coloridas.
    QStringList voiPresetNames() const;
    int defaultVoiPreset() const;
    /// Índice da janela predefinida em uso, ou -1 para janela personalizada.
    int activeVoiPreset() const;
    bool applyVoiPreset(int index);

    /// Janela atual; vazio para imagens coloridas ou quando uma VOI LUT do
    /// arquivo está em uso.
    std::optional<WindowLevel> windowLevel() const;
    /// Aplica uma janela personalizada. Exige imagem monocromática e largura >= 1.
    bool setWindowLevel(const WindowLevel &window);

    /// Valores mínimo e máximo dos pixels (imagens monocromáticas).
    std::optional<ValueRange> valueRange() const;

    /// Renderiza um frame em 8 bits: Format_Grayscale8 (monocromática) ou
    /// Format_RGB888 (colorida). Retorna uma QImage nula em caso de falha.
    QImage renderFrame(int frame = 0) const;

    const QList<MetadataEntry> &metadata() const;

private:
    friend class DicomLoader;
    struct Private;

    explicit DicomImageDocument(std::unique_ptr<Private> d);

    std::unique_ptr<Private> d;
};

} // namespace dicomreader

#endif // DICOMREADER_CORE_DICOMIMAGEDOCUMENT_H
