// main.cpp
#include <QApplication>
#include <QCoreApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("openw3d"));
    QCoreApplication::setApplicationName(QStringLiteral("oW3DEdit"));
    MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}
