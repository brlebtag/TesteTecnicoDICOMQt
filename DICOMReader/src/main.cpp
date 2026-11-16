#include "ui/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DICOMReader"));
    QApplication::setOrganizationName(QStringLiteral("DICOMReader"));
    QApplication::setApplicationVersion(QStringLiteral(DICOMREADER_VERSION));

    // A interface é em português. Os textos do próprio Qt (botões e diálogos
    // padrão) usam a tradução pt_BR que acompanha o Qt, quando disponível.
    const QLocale brazilianPortuguese(QLocale::Portuguese, QLocale::Brazil);
    QLocale::setDefault(brazilianPortuguese);
    QTranslator qtTranslator;
    if (qtTranslator.load(brazilianPortuguese, QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtTranslator);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Visualizador de imagens DICOM."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("arquivo"),
                                 QStringLiteral("Arquivo DICOM a abrir (opcional)."));
    parser.process(app);

    MainWindow window;
    window.show();

    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty())
        window.openFile(files.first());

    return QApplication::exec();
}
