#ifndef DICOMREADER_UI_WINDOWLEVELPANEL_H
#define DICOMREADER_UI_WINDOWLEVELPANEL_H

#include "core/dicomimagedocument.h"

#include <QWidget>

#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

/// Controles de janela VOI: janelas predefinidas (do arquivo e automáticas),
/// centro (WL) e largura (WW) e o botão para restaurar a janela inicial.
///
/// Só emite sinais em resposta a ações do usuário; as atualizações feitas
/// pelos métodos set*() não geram sinais.
class WindowLevelPanel : public QWidget
{
    Q_OBJECT

public:
    explicit WindowLevelPanel(QWidget *parent = nullptr);

    void setPresets(const QStringList &names);
    /// @param index janela predefinida em uso, ou -1 para "Personalizada".
    void setActivePreset(int index);
    void setWindowLevel(const dicomreader::WindowLevel &window);
    void setValueRange(const std::optional<dicomreader::ValueRange> &range);
    /// Desabilita os controles (ex.: imagem colorida), exibindo o motivo.
    void setUnavailable(const QString &reason);
    void clear();

signals:
    void presetActivated(int index);
    void windowLevelEdited(const dicomreader::WindowLevel &window);
    void resetRequested();

private:
    void emitWindowLevel();
    void setControlsEnabled(bool enabled);

    QComboBox *m_presets = nullptr;
    QDoubleSpinBox *m_center = nullptr;
    QDoubleSpinBox *m_width = nullptr;
    QLabel *m_range = nullptr;
    QPushButton *m_reset = nullptr;
    QLabel *m_message = nullptr;
};

#endif // DICOMREADER_UI_WINDOWLEVELPANEL_H
