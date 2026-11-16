#include "windowlevelpanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

using dicomreader::ValueRange;
using dicomreader::WindowLevel;

namespace {

constexpr double kMaxMagnitude = 1.0e7;
constexpr int kDecimals = 2;

QDoubleSpinBox *createSpinBox(double minimum, QWidget *parent)
{
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setRange(minimum, kMaxMagnitude);
    spinBox->setDecimals(kDecimals);
    // Sem keyboardTracking o valor só é aplicado ao confirmar a digitação.
    spinBox->setKeyboardTracking(false);
    spinBox->setAccelerated(true);
    return spinBox;
}

QString formatNumber(double value)
{
    return QString::number(value, 'g', 6);
}

} // namespace

WindowLevelPanel::WindowLevelPanel(QWidget *parent)
    : QWidget(parent)
    , m_presets(new QComboBox(this))
    , m_center(createSpinBox(-kMaxMagnitude, this))
    , m_width(createSpinBox(1.0, this))
    , m_range(new QLabel(this))
    , m_reset(new QPushButton(QStringLiteral("Restaurar janela inicial"), this))
    , m_message(new QLabel(this))
{
    m_presets->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_presets->setMinimumContentsLength(16);
    m_range->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_message->setWordWrap(true);
    m_message->setForegroundRole(QPalette::PlaceholderText);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Predefinição:"), m_presets);
    form->addRow(QStringLiteral("Centro (WL):"), m_center);
    form->addRow(QStringLiteral("Largura (WW):"), m_width);
    form->addRow(QStringLiteral("Valores:"), m_range);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_reset);
    layout->addWidget(m_message);
    // Altura só a necessária: o restante da lateral fica para os metadados.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    connect(m_presets, &QComboBox::activated, this, &WindowLevelPanel::presetActivated);
    connect(m_center, &QDoubleSpinBox::valueChanged, this, &WindowLevelPanel::emitWindowLevel);
    connect(m_width, &QDoubleSpinBox::valueChanged, this, &WindowLevelPanel::emitWindowLevel);
    connect(m_reset, &QPushButton::clicked, this, &WindowLevelPanel::resetRequested);

    clear();
}

void WindowLevelPanel::setPresets(const QStringList &names)
{
    const QSignalBlocker blocker(m_presets);
    m_presets->clear();
    m_presets->addItems(names);
    setControlsEnabled(true);
    m_message->setText(QStringLiteral("Dica: arraste com o botão direito sobre a imagem. "
                                      "Na horizontal ajusta a largura; na vertical, o centro."));
}

void WindowLevelPanel::setActivePreset(int index)
{
    const QSignalBlocker blocker(m_presets);
    m_presets->setCurrentIndex(index);
}

void WindowLevelPanel::setWindowLevel(const WindowLevel &window)
{
    const QSignalBlocker centerBlocker(m_center);
    const QSignalBlocker widthBlocker(m_width);
    m_center->setValue(window.center);
    m_width->setValue(window.width);
}

void WindowLevelPanel::setValueRange(const std::optional<ValueRange> &range)
{
    if (!range) {
        m_range->clear();
        return;
    }
    m_range->setText(QStringLiteral("%1 a %2").arg(formatNumber(range->min), formatNumber(range->max)));

    // Passo proporcional à faixa: 1 para imagens inteiras típicas, menor para
    // faixas pequenas (ex.: valores fracionários após o rescale).
    const double step = std::clamp((range->max - range->min) / 256.0, 0.01, 1.0);
    m_center->setSingleStep(step);
    m_width->setSingleStep(step);
}

void WindowLevelPanel::setUnavailable(const QString &reason)
{
    clear();
    m_message->setText(reason);
}

void WindowLevelPanel::clear()
{
    {
        const QSignalBlocker blocker(m_presets);
        m_presets->clear();
    }
    setWindowLevel({});
    m_range->clear();
    m_message->clear();
    setControlsEnabled(false);
}

void WindowLevelPanel::emitWindowLevel()
{
    emit windowLevelEdited({m_center->value(), m_width->value()});
}

void WindowLevelPanel::setControlsEnabled(bool enabled)
{
    // "Personalizada" aparece quando nenhuma janela predefinida está em uso.
    m_presets->setPlaceholderText(enabled ? QStringLiteral("Personalizada") : QString());
    m_presets->setEnabled(enabled);
    m_center->setEnabled(enabled);
    m_width->setEnabled(enabled);
    m_reset->setEnabled(enabled);
}
