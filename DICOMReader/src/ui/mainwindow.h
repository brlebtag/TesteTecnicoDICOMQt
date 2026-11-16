#ifndef DICOMREADER_UI_MAINWINDOW_H
#define DICOMREADER_UI_MAINWINDOW_H

#include "core/dicomimagedocument.h"

#include <QMainWindow>

#include <memory>

class ImageView;
class MetadataPanel;
class WindowLevelPanel;
class QAction;
class QDockWidget;
class QLabel;
class QSpinBox;

namespace dicomreader {
struct LoadError;
}

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Abre e exibe um arquivo DICOM. Em caso de erro, mostra a mensagem e
    /// mantém a imagem que estava aberta. Retorna true se o arquivo foi aberto.
    bool openFile(const QString &filePath);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupImageView();
    void setupDocks();
    void setupFrameSelector();
    void setupStatusBar();
    void setupActions();

    void openFileDialog();
    void showLoadError(const QString &filePath, const dicomreader::LoadError &error);
    void showDocument();
    void renderCurrentFrame();
    void updateWindowControls();
    void updateDocumentActions();

    void applyVoiPreset(int index);
    void applyWindowLevel(const dicomreader::WindowLevel &window);
    void resetWindow();
    void adjustWindowByDrag(int dx, int dy);
    dicomreader::WindowLevel currentWindowLevel() const;

    void showFrame(int frameNumber);
    void updateZoomLabel(double factor);
    void showAbout();

    void restoreSettings();
    void saveSettings() const;

    std::unique_ptr<Ui::MainWindow> ui;
    ImageView *m_imageView = nullptr;
    WindowLevelPanel *m_windowPanel = nullptr;
    MetadataPanel *m_metadataPanel = nullptr;
    QDockWidget *m_windowDock = nullptr;
    QDockWidget *m_metadataDock = nullptr;
    QSpinBox *m_frameSpinBox = nullptr;
    QLabel *m_frameCountLabel = nullptr;
    QAction *m_frameSelectorAction = nullptr;
    QLabel *m_imageInfoLabel = nullptr;
    QLabel *m_windowLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;

    std::unique_ptr<dicomreader::DicomImageDocument> m_document;
    int m_currentFrame = 0;
    QString m_lastDirectory;
};

#endif // DICOMREADER_UI_MAINWINDOW_H
