#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/dicomloader.h"
#include "imageview.h"
#include "metadatapanel.h"
#include "windowlevelpanel.h"

#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>

#include <algorithm>

using dicomreader::DicomLoader;
using dicomreader::LoadError;
using dicomreader::WindowLevel;

namespace {

constexpr int kStatusMessageTimeoutMs = 5000;
constexpr int kDockWidth = 440;
/// Quantos pixels de arraste percorrem toda a faixa de valores da imagem.
constexpr double kDragPixelsPerRange = 512.0;

const auto kGeometryKey = QStringLiteral("mainWindow/geometry");
const auto kStateKey = QStringLiteral("mainWindow/state");
const auto kLastDirectoryKey = QStringLiteral("lastDirectory");

QString formatNumber(double value)
{
    return QString::number(value, 'g', 6);
}

/// Cursor de espera enquanto o objeto existir.
class WaitCursor
{
public:
    WaitCursor() { QGuiApplication::setOverrideCursor(Qt::WaitCursor); }
    ~WaitCursor() { QGuiApplication::restoreOverrideCursor(); }
    WaitCursor(const WaitCursor &) = delete;
    WaitCursor &operator=(const WaitCursor &) = delete;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
{
    ui->setupUi(this);
    setupImageView();
    setupDocks();
    setupFrameSelector();
    setupStatusBar();
    setupActions();
    restoreSettings();
    updateDocumentActions();
}

MainWindow::~MainWindow() = default;

// --- Montagem da interface ---------------------------------------------------

void MainWindow::setupImageView()
{
    m_imageView = new ImageView(this);
    setCentralWidget(m_imageView);

    connect(m_imageView, &ImageView::windowDragged, this, &MainWindow::adjustWindowByDrag);
    connect(m_imageView, &ImageView::zoomChanged, this, &MainWindow::updateZoomLabel);
    connect(m_imageView, &ImageView::fileDropped, this, &MainWindow::openFile);
}

void MainWindow::setupDocks()
{
    m_windowPanel = new WindowLevelPanel;
    m_windowDock = new QDockWidget(QStringLiteral("Janela (WL/WW)"), this);
    m_windowDock->setObjectName(QStringLiteral("windowLevelDock"));
    m_windowDock->setWidget(m_windowPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_windowDock);

    m_metadataPanel = new MetadataPanel;
    m_metadataDock = new QDockWidget(QStringLiteral("Metadados DICOM"), this);
    m_metadataDock->setObjectName(QStringLiteral("metadataDock"));
    m_metadataDock->setWidget(m_metadataPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_metadataDock);

    resizeDocks({m_windowDock, m_metadataDock}, {kDockWidth, kDockWidth}, Qt::Horizontal);

    m_windowDock->toggleViewAction()->setText(QStringLiteral("Painel de janela (WL/WW)"));
    m_metadataDock->toggleViewAction()->setText(QStringLiteral("Painel de metadados"));
    ui->menuView->addAction(m_windowDock->toggleViewAction());
    ui->menuView->addAction(m_metadataDock->toggleViewAction());

    connect(m_windowPanel, &WindowLevelPanel::presetActivated, this, &MainWindow::applyVoiPreset);
    connect(m_windowPanel, &WindowLevelPanel::windowLevelEdited, this, &MainWindow::applyWindowLevel);
    connect(m_windowPanel, &WindowLevelPanel::resetRequested, this, &MainWindow::resetWindow);
}

void MainWindow::setupFrameSelector()
{
    auto *selector = new QWidget(this);
    auto *layout = new QHBoxLayout(selector);
    layout->setContentsMargins(8, 0, 8, 0);
    m_frameSpinBox = new QSpinBox(selector);
    m_frameSpinBox->setObjectName(QStringLiteral("frameSpinBox"));
    m_frameSpinBox->setAccelerated(true);
    m_frameCountLabel = new QLabel(selector);
    layout->addWidget(new QLabel(QStringLiteral("Frame:"), selector));
    layout->addWidget(m_frameSpinBox);
    layout->addWidget(m_frameCountLabel);

    ui->mainToolBar->addSeparator();
    m_frameSelectorAction = ui->mainToolBar->addWidget(selector);
    m_frameSelectorAction->setObjectName(QStringLiteral("frameSelectorAction"));
    m_frameSelectorAction->setVisible(false);

    connect(m_frameSpinBox, &QSpinBox::valueChanged, this, &MainWindow::showFrame);
}

void MainWindow::setupStatusBar()
{
    m_imageInfoLabel = new QLabel(this);
    m_imageInfoLabel->setObjectName(QStringLiteral("imageInfoLabel"));
    m_windowLabel = new QLabel(this);
    m_windowLabel->setObjectName(QStringLiteral("windowLabel"));
    m_zoomLabel = new QLabel(this);
    m_zoomLabel->setObjectName(QStringLiteral("zoomLabel"));
    statusBar()->addPermanentWidget(m_imageInfoLabel);
    statusBar()->addPermanentWidget(m_windowLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::setupActions()
{
    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    ui->actionOpen->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    ui->actionQuit->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
    ui->actionFitToWindow->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-best")));
    ui->actionActualSize->setIcon(QIcon::fromTheme(QStringLiteral("zoom-original")));
    ui->actionZoomIn->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    ui->actionZoomOut->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    ui->actionResetWindow->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    ui->actionAbout->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));

    // Textos curtos para a barra de ferramentas.
    ui->actionOpen->setIconText(QStringLiteral("Abrir"));
    ui->actionFitToWindow->setIconText(QStringLiteral("Ajustar"));
    ui->actionActualSize->setIconText(QStringLiteral("1:1"));
    ui->actionResetWindow->setIconText(QStringLiteral("Restaurar WL/WW"));

    // Ctrl+= também amplia (teclados em que "+" exige Shift).
    ui->actionZoomIn->setShortcuts({QKeySequence(QKeySequence::ZoomIn), QKeySequence(Qt::CTRL | Qt::Key_Equal)});

    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFileDialog);
    connect(ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionFitToWindow, &QAction::triggered, m_imageView, &ImageView::fitToWindow);
    connect(ui->actionActualSize, &QAction::triggered, m_imageView, &ImageView::zoomToActualSize);
    connect(ui->actionZoomIn, &QAction::triggered, m_imageView, &ImageView::zoomIn);
    connect(ui->actionZoomOut, &QAction::triggered, m_imageView, &ImageView::zoomOut);
    connect(ui->actionResetWindow, &QAction::triggered, this, &MainWindow::resetWindow);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
}

// --- Abertura de arquivos ----------------------------------------------------

void MainWindow::openFileDialog()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("Abrir arquivo DICOM"), m_lastDirectory,
        QStringLiteral("Arquivos DICOM (*.dcm *.dicom *.dic);;Todos os arquivos (*)"));
    if (!filePath.isEmpty())
        openFile(filePath);
}

bool MainWindow::openFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    m_lastDirectory = info.absolutePath();

    dicomreader::LoadResult result = [&filePath] {
        const WaitCursor waitCursor;
        return DicomLoader::load(filePath);
    }();

    if (!result.isSuccess()) {
        showLoadError(filePath, result.error());
        return false;
    }

    m_document = result.takeDocument();
    m_currentFrame = 0;
    showDocument();
    statusBar()->showMessage(QStringLiteral("Arquivo aberto: %1").arg(info.fileName()),
                             kStatusMessageTimeoutMs);
    return true;
}

void MainWindow::showLoadError(const QString &filePath, const LoadError &error)
{
    statusBar()->showMessage(
        QStringLiteral("Falha ao abrir %1: %2").arg(QFileInfo(filePath).fileName(), error.message),
        kStatusMessageTimeoutMs);

    QString details = QStringLiteral("Caminho: %1").arg(QDir::toNativeSeparators(filePath));
    if (!error.details.isEmpty())
        details += QStringLiteral("\nDetalhe: %1").arg(error.details);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(QStringLiteral("Não foi possível abrir o arquivo"));
    box.setText(error.message);
    box.setInformativeText(QStringLiteral("Arquivo: %1").arg(QFileInfo(filePath).fileName()));
    box.setDetailedText(details);
    box.exec();
}

// --- Exibição do documento ---------------------------------------------------

void MainWindow::showDocument()
{
    const dicomreader::DicomImageDocument &document = *m_document;

    setWindowTitle(QStringLiteral("%1 — DICOMReader").arg(QFileInfo(document.filePath()).fileName()));

    m_imageView->setImage(document.renderFrame(m_currentFrame), document.pixelAspectRatio());
    m_metadataPanel->setEntries(document.metadata());

    const int frameCount = document.frameCount();
    {
        const QSignalBlocker blocker(m_frameSpinBox);
        m_frameSpinBox->setRange(1, frameCount);
        m_frameSpinBox->setValue(m_currentFrame + 1);
    }
    m_frameCountLabel->setText(QStringLiteral("de %1").arg(frameCount));
    m_frameSelectorAction->setVisible(frameCount > 1);

    QStringList info{QStringLiteral("%1 × %2 px").arg(document.width()).arg(document.height()),
                     document.photometricInterpretation()};
    if (document.bitsStored() > 0)
        info.append(QStringLiteral("%1 bits").arg(document.bitsStored()));
    if (!document.modality().isEmpty())
        info.append(document.modality());
    info.removeAll(QString());
    m_imageInfoLabel->setText(info.join(QStringLiteral(" · ")));
    m_imageInfoLabel->setToolTip(QStringLiteral("Sintaxe de transferência: %1").arg(document.transferSyntax()));

    if (document.isMonochrome()) {
        m_windowPanel->setPresets(document.voiPresetNames());
        m_windowPanel->setValueRange(document.valueRange());
    } else {
        m_windowPanel->setUnavailable(
            QStringLiteral("A janela (WL/WW) se aplica apenas a imagens monocromáticas."));
    }
    updateWindowControls();
    updateDocumentActions();
}

void MainWindow::renderCurrentFrame()
{
    const QImage image = m_document->renderFrame(m_currentFrame);
    if (image.isNull()) {
        statusBar()->showMessage(QStringLiteral("Não foi possível renderizar o frame %1.").arg(m_currentFrame + 1),
                                 kStatusMessageTimeoutMs);
        return;
    }
    m_imageView->updateImage(image);
}

void MainWindow::updateWindowControls()
{
    if (!m_document || !m_document->isMonochrome()) {
        m_windowLabel->clear();
        return;
    }

    m_windowPanel->setActivePreset(m_document->activeVoiPreset());
    m_windowPanel->setWindowLevel(currentWindowLevel());

    if (const auto window = m_document->windowLevel()) {
        m_windowLabel->setText(QStringLiteral("Centro %1 · Largura %2")
                                   .arg(formatNumber(window->center), formatNumber(window->width)));
    } else {
        m_windowLabel->setText(QStringLiteral("VOI LUT do arquivo"));
    }
}

void MainWindow::updateDocumentActions()
{
    const bool hasDocument = m_document != nullptr;
    ui->actionFitToWindow->setEnabled(hasDocument);
    ui->actionActualSize->setEnabled(hasDocument);
    ui->actionZoomIn->setEnabled(hasDocument);
    ui->actionZoomOut->setEnabled(hasDocument);
    ui->actionResetWindow->setEnabled(hasDocument && m_document->isMonochrome());
}

// --- Janela (WL/WW) ----------------------------------------------------------

void MainWindow::applyVoiPreset(int index)
{
    if (m_document && m_document->applyVoiPreset(index))
        renderCurrentFrame();
    updateWindowControls();
}

void MainWindow::applyWindowLevel(const WindowLevel &window)
{
    if (m_document && m_document->setWindowLevel(window))
        renderCurrentFrame();
    updateWindowControls();
}

void MainWindow::resetWindow()
{
    if (m_document)
        applyVoiPreset(m_document->defaultVoiPreset());
}

void MainWindow::adjustWindowByDrag(int dx, int dy)
{
    if (!m_document || !m_document->isMonochrome())
        return;

    // Horizontal altera a largura (contraste); vertical, o centro (brilho).
    // O passo é proporcional à faixa de valores, para que o arraste tenha a
    // mesma sensibilidade em imagens de 8 ou 16 bits.
    const auto range = m_document->valueRange();
    const double span = range ? range->max - range->min : 256.0;
    const double step = std::max(span / kDragPixelsPerRange, 0.01);

    WindowLevel window = currentWindowLevel();
    window.width = std::max(1.0, window.width + dx * step);
    window.center += dy * step;
    applyWindowLevel(window);
}

WindowLevel MainWindow::currentWindowLevel() const
{
    if (const auto window = m_document->windowLevel())
        return *window;

    // VOI LUT em uso: parte da janela que cobre toda a faixa de valores.
    if (const auto range = m_document->valueRange())
        return {(range->min + range->max) / 2.0, std::max(1.0, range->max - range->min)};
    return {};
}

// --- Frames, zoom e diálogos -------------------------------------------------

void MainWindow::showFrame(int frameNumber)
{
    if (!m_document)
        return;
    m_currentFrame = std::clamp(frameNumber - 1, 0, m_document->frameCount() - 1);
    renderCurrentFrame();
}

void MainWindow::updateZoomLabel(double factor)
{
    m_zoomLabel->setText(QStringLiteral("Zoom %1%").arg(qRound(factor * 100.0)));
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, QStringLiteral("Sobre o DICOMReader"),
        QStringLiteral("<h3>DICOMReader %1</h3>"
                       "<p>Visualizador de imagens DICOM.</p>"
                       "<p>Qt %2<br>DCMTK %3</p>")
            .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion()),
                 DicomLoader::dcmtkVersion()));
}

// --- Configurações -----------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::restoreSettings()
{
    const QSettings settings;
    restoreGeometry(settings.value(kGeometryKey).toByteArray());
    restoreState(settings.value(kStateKey).toByteArray());
    m_lastDirectory = settings
                          .value(kLastDirectoryKey,
                                 QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                          .toString();
}

void MainWindow::saveSettings() const
{
    QSettings settings;
    settings.setValue(kGeometryKey, saveGeometry());
    settings.setValue(kStateKey, saveState());
    settings.setValue(kLastDirectoryKey, m_lastDirectory);
}
