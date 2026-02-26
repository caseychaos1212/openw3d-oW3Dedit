// main.cpp
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include "MainWindow.h"

static QIcon LoadWindowIcon() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath("resources/ow3dedit_logo_v2.ico"),
        QDir(appDir).absoluteFilePath("../resources/ow3dedit_logo_v2.ico"),
        QDir(appDir).absoluteFilePath("../../resources/ow3dedit_logo_v2.ico")
    };

    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            QIcon icon(path);
            if (!icon.isNull()) {
                return icon;
            }
        }
    }

    return QIcon();
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("openw3d"));
    QCoreApplication::setApplicationName(QStringLiteral("oW3DEdit"));

    const QIcon windowIcon = LoadWindowIcon();
    if (!windowIcon.isNull()) {
        app.setWindowIcon(windowIcon);
    }

    MainWindow mainWindow;
    if (!windowIcon.isNull()) {
        mainWindow.setWindowIcon(windowIcon);
    }
    mainWindow.show();
    return app.exec();
}
