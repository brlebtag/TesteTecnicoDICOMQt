// Testes da interface. Rodam com a plataforma "offscreen" do Qt: nenhuma
// janela aparece na tela.

#include "core/loaderror.h"
#include "dicomtestfiles.h"
#include "ui/imageview.h"
#include "ui/mainwindow.h"
#include "ui/metadatapanel.h"
#include "ui/windowlevelpanel.h"

#include <dcmtk/dcmdata/dcdeftag.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeView>

using dicomreader::LoadErrorCode;
using dicomreader::MetadataEntry;
using dicomreader::WindowLevel;
using testfiles::MonochromeImage;

namespace {

MonochromeImage gradientImage(unsigned short rows, unsigned short columns, unsigned short frames = 1)
{
    MonochromeImage image;
    image.rows = rows;
    image.columns = columns;
    image.frames = frames;
    const int count = rows * columns * frames;
    for (int i = 0; i < count; ++i)
        image.pixels.push_back(static_cast<std::uint16_t>(i * 4095 / (count - 1)));
    return image;
}

void sendMouse(QWidget *widget, QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons,
               QPoint position)
{
    QMouseEvent event(type, QPointF(position), widget->mapToGlobal(QPointF(position)), button, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

/// Arrasta com o botão direito de `from` até `to` sobre o widget.
void rightDrag(QWidget *widget, QPoint from, QPoint to)
{
    sendMouse(widget, QEvent::MouseButtonPress, Qt::RightButton, Qt::RightButton, from);
    sendMouse(widget, QEvent::MouseMove, Qt::NoButton, Qt::RightButton, to);
    sendMouse(widget, QEvent::MouseButtonRelease, Qt::RightButton, Qt::NoButton, to);
}

/// Fecha o próximo diálogo modal que abrir, guardando o texto dele.
void closeNextModalDialog(QString *text)
{
    QTimer::singleShot(0, [text] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            *text = box->text();
            box->accept();
        }
    });
}

} // namespace

class TestUi : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // ImageView
    void imageViewStartsEmpty();
    void imageViewFitsImageToWindow();
    void imageViewZooms();
    void imageViewAppliesPixelAspectRatio();
    void imageViewEmitsWindowDragWithRightButton();

    // WindowLevelPanel
    void windowPanelDoesNotEmitOnProgrammaticChanges();
    void windowPanelEmitsUserEdits();
    void windowPanelCanBeDisabled();

    // MetadataPanel
    void metadataPanelBuildsTree();
    void metadataPanelFiltersKeepingParents();
    void metadataPanelCopiesSelection();

    // MainWindow
    void mainWindowOpensFile();
    void mainWindowShowsErrorAndKeepsCurrentImage();
    void mainWindowAdjustsWindowByDragging();
    void mainWindowAppliesPresetsAndReset();
    void mainWindowShowsFrameSelectorForMultiFrame();
    void mainWindowDisablesWindowControlsForColor();

private:
    QString writeFile(const QString &name, DcmFileFormat &fileFormat);
    QString writeMonochrome(const QString &name, const MonochromeImage &image);

    QTemporaryDir m_dir;
};

void TestUi::initTestCase()
{
    QVERIFY(m_dir.isValid());
    // Configurações dos testes isoladas das do usuário.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.filePath(QStringLiteral("settings")));
}

QString TestUi::writeFile(const QString &name, DcmFileFormat &fileFormat)
{
    const QString path = m_dir.filePath(name);
    return testfiles::save(fileFormat, path) ? path : QString();
}

QString TestUi::writeMonochrome(const QString &name, const MonochromeImage &image)
{
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(image);
    return writeFile(name, fileFormat);
}

// --- ImageView --------------------------------------------------------------

void TestUi::imageViewStartsEmpty()
{
    ImageView view;
    QVERIFY(!view.hasImage());
    view.zoomIn();
    QCOMPARE(view.zoomFactor(), 1.0);
}

void TestUi::imageViewFitsImageToWindow()
{
    ImageView view;
    view.resize(400, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QSignalSpy zoomSpy(&view, &ImageView::zoomChanged);
    QImage image(100, 50, QImage::Format_Grayscale8);
    image.fill(128);
    view.setImage(image, 1.0);

    QVERIFY(view.hasImage());
    QVERIFY(zoomSpy.count() >= 1);
    // 100x50 numa área de 400x300: limitado pela largura, ~4x.
    QVERIFY2(view.zoomFactor() > 3.5 && view.zoomFactor() <= 4.0, qPrintable(QString::number(view.zoomFactor())));
}

void TestUi::imageViewZooms()
{
    ImageView view;
    view.resize(400, 300);
    QImage image(100, 100, QImage::Format_Grayscale8);
    image.fill(0);
    view.setImage(image, 1.0);

    view.zoomToActualSize();
    QCOMPARE(view.zoomFactor(), 1.0);
    view.zoomIn();
    QCOMPARE(view.zoomFactor(), 1.25);
    view.zoomOut();
    QCOMPARE(view.zoomFactor(), 1.0);

    view.clear();
    QVERIFY(!view.hasImage());
}

void TestUi::imageViewAppliesPixelAspectRatio()
{
    ImageView view;
    QImage image(100, 100, QImage::Format_Grayscale8);
    image.fill(0);
    view.setImage(image, 2.0);
    QCOMPARE(view.scene()->sceneRect().size(), QSizeF(200.0, 100.0));
}

void TestUi::imageViewEmitsWindowDragWithRightButton()
{
    ImageView view;
    view.resize(200, 200);
    QImage image(10, 10, QImage::Format_Grayscale8);
    image.fill(0);
    view.setImage(image, 1.0);

    QSignalSpy spy(&view, &ImageView::windowDragged);
    rightDrag(view.viewport(), QPoint(50, 50), QPoint(60, 45));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toInt(), 10);
    QCOMPARE(spy.first().at(1).toInt(), -5);
}

// --- WindowLevelPanel -------------------------------------------------------

void TestUi::windowPanelDoesNotEmitOnProgrammaticChanges()
{
    WindowLevelPanel panel;
    QSignalSpy presetSpy(&panel, &WindowLevelPanel::presetActivated);
    QSignalSpy windowSpy(&panel, &WindowLevelPanel::windowLevelEdited);

    panel.setPresets({QStringLiteral("A"), QStringLiteral("B")});
    panel.setActivePreset(1);
    panel.setWindowLevel({40.0, 400.0});
    panel.setValueRange(dicomreader::ValueRange{-1000.0, 3000.0});

    QCOMPARE(presetSpy.count(), 0);
    QCOMPARE(windowSpy.count(), 0);
    QCOMPARE(panel.findChild<QDoubleSpinBox *>(QStringLiteral("centerSpinBox"))->value(), 40.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox *>(QStringLiteral("widthSpinBox"))->value(), 400.0);
}

void TestUi::windowPanelEmitsUserEdits()
{
    WindowLevelPanel panel;
    panel.setPresets({QStringLiteral("A"), QStringLiteral("B")});
    panel.setActivePreset(0);
    panel.setWindowLevel({40.0, 400.0});

    QSignalSpy windowSpy(&panel, &WindowLevelPanel::windowLevelEdited);
    panel.findChild<QDoubleSpinBox *>(QStringLiteral("centerSpinBox"))->setValue(100.0);
    QCOMPARE(windowSpy.count(), 1);
    const auto window = windowSpy.first().first().value<WindowLevel>();
    QCOMPARE(window.center, 100.0);
    QCOMPARE(window.width, 400.0);

    QSignalSpy presetSpy(&panel, &WindowLevelPanel::presetActivated);
    QTest::keyClick(panel.findChild<QComboBox *>(QStringLiteral("presetComboBox")), Qt::Key_Down);
    QCOMPARE(presetSpy.count(), 1);
    QCOMPARE(presetSpy.first().first().toInt(), 1);

    QSignalSpy resetSpy(&panel, &WindowLevelPanel::resetRequested);
    auto *reset = panel.findChild<QPushButton *>();
    QVERIFY(reset);
    reset->click();
    QCOMPARE(resetSpy.count(), 1);
}

void TestUi::windowPanelCanBeDisabled()
{
    WindowLevelPanel panel;
    panel.setPresets({QStringLiteral("A")});
    panel.setUnavailable(QStringLiteral("Indisponível"));
    QVERIFY(!panel.findChild<QComboBox *>(QStringLiteral("presetComboBox"))->isEnabled());
    QVERIFY(!panel.findChild<QDoubleSpinBox *>(QStringLiteral("centerSpinBox"))->isEnabled());
    QVERIFY(!panel.findChild<QDoubleSpinBox *>(QStringLiteral("widthSpinBox"))->isEnabled());
}

// --- MetadataPanel ----------------------------------------------------------

namespace {

QList<MetadataEntry> sampleEntries()
{
    return {
        {QStringLiteral("(0008,0060)"), QStringLiteral("CS"), QStringLiteral("Modality"), QStringLiteral("CT"), 0},
        {QStringLiteral("(0008,1140)"), QStringLiteral("SQ"), QStringLiteral("ReferencedImageSequence"),
         QStringLiteral("1 item"), 0},
        {QString(), QString(), QStringLiteral("Item 1"), QString(), 1},
        {QStringLiteral("(0008,1155)"), QStringLiteral("UI"), QStringLiteral("ReferencedSOPInstanceUID"),
         QStringLiteral("1.2.3.4"), 2},
        {QStringLiteral("(0010,0010)"), QStringLiteral("PN"), QStringLiteral("PatientName"),
         QStringLiteral("Teste^Paciente"), 0},
    };
}

} // namespace

void TestUi::metadataPanelBuildsTree()
{
    MetadataPanel panel;
    panel.setEntries(sampleEntries());
    const QAbstractItemModel *model = panel.findChild<QTreeView *>()->model();

    QCOMPARE(model->rowCount(), 3);
    const QModelIndex sequence = model->index(1, 0);
    QCOMPARE(sequence.data().toString(), QStringLiteral("(0008,1140)"));
    QCOMPARE(model->rowCount(sequence), 1);
    const QModelIndex item = model->index(0, 0, sequence);
    QCOMPARE(model->index(0, 2, sequence).data().toString(), QStringLiteral("Item 1"));
    QCOMPARE(model->rowCount(item), 1);
    QCOMPARE(model->index(0, 3, item).data().toString(), QStringLiteral("1.2.3.4"));

    // Novos dados substituem os anteriores.
    panel.setEntries(sampleEntries().mid(0, 1));
    QCOMPARE(model->rowCount(), 1);
}

void TestUi::metadataPanelFiltersKeepingParents()
{
    MetadataPanel panel;
    panel.setEntries(sampleEntries());
    const QAbstractItemModel *model = panel.findChild<QTreeView *>()->model();

    panel.findChild<QLineEdit *>()->setText(QStringLiteral("1.2.3"));
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("(0008,1140)"));

    panel.findChild<QLineEdit *>()->setText(QStringLiteral("patientname"));
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->index(0, 3).data().toString(), QStringLiteral("Teste^Paciente"));

    panel.findChild<QLineEdit *>()->clear();
    QCOMPARE(model->rowCount(), 3);
}

void TestUi::metadataPanelCopiesSelection()
{
    MetadataPanel panel;
    panel.setEntries(sampleEntries());
    auto *view = panel.findChild<QTreeView *>();
    view->selectionModel()->select(view->model()->index(0, 0),
                                   QItemSelectionModel::Select | QItemSelectionModel::Rows);

    QApplication::clipboard()->clear();
    QAction *copy = view->actions().value(0);
    QVERIFY(copy);
    copy->trigger();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("(0008,0060)\tCS\tModality\tCT"));
}

// --- MainWindow -------------------------------------------------------------

void TestUi::mainWindowOpensFile()
{
    const QString file = writeMonochrome(QStringLiteral("janela-principal.dcm"), gradientImage(8, 8));
    QVERIFY(!file.isEmpty());

    MainWindow window;
    QVERIFY(window.openFile(file));
    QVERIFY(window.windowTitle().contains(QStringLiteral("janela-principal.dcm")));
    QVERIFY(window.findChild<ImageView *>()->hasImage());
    QVERIFY(window.findChild<MetadataPanel *>()->findChild<QTreeView *>()->model()->rowCount() > 0);
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("imageInfoLabel"))->text().contains(QStringLiteral("8 × 8 px")));
}

void TestUi::mainWindowShowsErrorAndKeepsCurrentImage()
{
    const QString valid = writeMonochrome(QStringLiteral("valido.dcm"), gradientImage(4, 4));
    const QString invalid = m_dir.filePath(QStringLiteral("invalido.dcm"));
    {
        QFile file(invalid);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("não é DICOM");
    }

    MainWindow window;
    QVERIFY(window.openFile(valid));

    QString dialogText;
    closeNextModalDialog(&dialogText);
    QVERIFY(!window.openFile(invalid));

    QCOMPARE(dialogText, dicomreader::defaultMessage(LoadErrorCode::NotDicom));
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Falha ao abrir")));
    // A imagem anterior continua aberta.
    QVERIFY(window.windowTitle().contains(QStringLiteral("valido.dcm")));
    QVERIFY(window.findChild<ImageView *>()->hasImage());
}

void TestUi::mainWindowAdjustsWindowByDragging()
{
    MonochromeImage source = gradientImage(16, 16);
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(source);
    fileFormat.getDataset()->putAndInsertString(DCM_WindowCenter, "2000");
    fileFormat.getDataset()->putAndInsertString(DCM_WindowWidth, "1000");
    const QString file = writeFile(QStringLiteral("arraste.dcm"), fileFormat);

    MainWindow window;
    window.resize(1000, 700);
    QVERIFY(window.openFile(file));
    auto *center = window.findChild<QDoubleSpinBox *>(QStringLiteral("centerSpinBox"));
    auto *width = window.findChild<QDoubleSpinBox *>(QStringLiteral("widthSpinBox"));
    auto *presets = window.findChild<QComboBox *>(QStringLiteral("presetComboBox"));
    QCOMPARE(center->value(), 2000.0);
    QCOMPARE(width->value(), 1000.0);
    QCOMPARE(presets->currentIndex(), 0);

    // Direita aumenta a largura; para baixo aumenta o centro.
    rightDrag(window.findChild<ImageView *>()->viewport(), QPoint(100, 100), QPoint(140, 120));
    QVERIFY(width->value() > 1000.0);
    QVERIFY(center->value() > 2000.0);
    QCOMPARE(presets->currentIndex(), -1); // janela personalizada
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("windowLabel"))->text().contains(QStringLiteral("Centro")));
}

void TestUi::mainWindowAppliesPresetsAndReset()
{
    MonochromeImage source = gradientImage(16, 16);
    DcmFileFormat fileFormat = testfiles::makeMonochromeFile(source);
    fileFormat.getDataset()->putAndInsertString(DCM_WindowCenter, "2000");
    fileFormat.getDataset()->putAndInsertString(DCM_WindowWidth, "1000");
    const QString file = writeFile(QStringLiteral("predefinicoes.dcm"), fileFormat);

    MainWindow window;
    QVERIFY(window.openFile(file));
    auto *center = window.findChild<QDoubleSpinBox *>(QStringLiteral("centerSpinBox"));
    auto *presets = window.findChild<QComboBox *>(QStringLiteral("presetComboBox"));

    // Janela do arquivo + mín–máx + histograma.
    QCOMPARE(presets->count(), 3);
    QTest::keyClick(presets, Qt::Key_Down); // mín–máx
    QCOMPARE(presets->currentIndex(), 1);
    QVERIFY(center->value() != 2000.0);

    center->setValue(123.0);
    QCOMPARE(presets->currentIndex(), -1);

    auto *reset = window.findChild<QAction *>(QStringLiteral("actionResetWindow"));
    QVERIFY(reset && reset->isEnabled());
    reset->trigger();
    QCOMPARE(presets->currentIndex(), 0);
    QCOMPARE(center->value(), 2000.0);
}

void TestUi::mainWindowShowsFrameSelectorForMultiFrame()
{
    const QString single = writeMonochrome(QStringLiteral("um-frame.dcm"), gradientImage(4, 4));
    const QString multi = writeMonochrome(QStringLiteral("tres-frames.dcm"), gradientImage(4, 4, 3));

    MainWindow window;
    auto *selector = window.findChild<QAction *>(QStringLiteral("frameSelectorAction"));
    auto *frames = window.findChild<QSpinBox *>(QStringLiteral("frameSpinBox"));
    QVERIFY(selector && frames);

    QVERIFY(window.openFile(single));
    QVERIFY(!selector->isVisible());

    QVERIFY(window.openFile(multi));
    QVERIFY(selector->isVisible());
    QCOMPARE(frames->minimum(), 1);
    QCOMPARE(frames->maximum(), 3);
    frames->setValue(3);
    QCOMPARE(frames->value(), 3);

    // Um novo arquivo volta ao primeiro frame.
    QVERIFY(window.openFile(multi));
    QCOMPARE(frames->value(), 1);
}

void TestUi::mainWindowDisablesWindowControlsForColor()
{
    DcmFileFormat fileFormat;
    testfiles::addIdentification(*fileFormat.getDataset());
    testfiles::setColorPixels(*fileFormat.getDataset(), 2, 2, "RGB", std::vector<std::uint8_t>(12, 200));
    const QString file = writeFile(QStringLiteral("colorida.dcm"), fileFormat);

    MainWindow window;
    QVERIFY(window.openFile(file));
    QVERIFY(!window.findChild<QDoubleSpinBox *>(QStringLiteral("centerSpinBox"))->isEnabled());
    QVERIFY(!window.findChild<QAction *>(QStringLiteral("actionResetWindow"))->isEnabled());
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("windowLabel"))->text().isEmpty());

    // Arrastar com o botão direito não tem efeito em imagens coloridas.
    rightDrag(window.findChild<ImageView *>()->viewport(), QPoint(10, 10), QPoint(50, 50));
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("windowLabel"))->text().isEmpty());
}

int main(int argc, char *argv[])
{
    // Sem janelas na tela, inclusive quando executado fora do ctest.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DICOMReaderTests"));
    QApplication::setOrganizationName(QStringLiteral("DICOMReaderTests"));
    TestUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_ui.moc"
