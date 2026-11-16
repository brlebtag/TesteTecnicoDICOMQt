#include "ui/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DICOMReader"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    MainWindow window;
    window.show();
    return QApplication::exec();
}
