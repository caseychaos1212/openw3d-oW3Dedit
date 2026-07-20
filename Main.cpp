// main.cpp
#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <iostream>
#include "AppVersion.h"
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
    QCoreApplication::setApplicationVersion(QStringLiteral(OW3DEDIT_VERSION_SEMVER));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Westwood 3D asset editor and validator"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption verifyRoundTripOption(
        QStringLiteral("verify-roundtrip"),
        QStringLiteral("Validate all supported assets in <directory> without opening the editor UI."),
        QStringLiteral("directory"));
    const QCommandLineOption verifyOutputOption(
        QStringLiteral("verify-output"),
        QStringLiteral("Write the timestamped validation report beneath <directory>."),
        QStringLiteral("directory"));
    const QCommandLineOption verifyModeOption(
        QStringLiteral("verify-mode"),
        QStringLiteral("Serialization mode: both, structured, or hex (default: both)."),
        QStringLiteral("mode"),
        QStringLiteral("both"));
    parser.addOption(verifyRoundTripOption);
    parser.addOption(verifyOutputOption);
    parser.addOption(verifyModeOption);
    parser.process(app);

    const bool commandLineValidation = parser.isSet(verifyRoundTripOption);
    if (commandLineValidation && !parser.isSet(verifyOutputOption)) {
        std::cerr << "--verify-output is required with --verify-roundtrip.\n";
        return 2;
    }

    const QIcon windowIcon = LoadWindowIcon();
    if (!windowIcon.isNull()) {
        app.setWindowIcon(windowIcon);
    }

    MainWindow mainWindow;
    if (!windowIcon.isNull()) {
        mainWindow.setWindowIcon(windowIcon);
    }
    if (commandLineValidation) {
        return mainWindow.runCommandLineRoundTripValidation(
            parser.value(verifyRoundTripOption),
            parser.value(verifyOutputOption),
            parser.value(verifyModeOption));
    }

    mainWindow.show();
    return app.exec();
}
