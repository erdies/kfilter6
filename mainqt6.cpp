/***************************************************************************
                          mainqt6.cpp  -  Qt6 entry point
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "kfilterqt6app.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QUrl>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("KFilter"));
    QApplication::setOrganizationDomain(QStringLiteral("kfilter.local"));
    QApplication::setApplicationName(QStringLiteral("KFilter"));
    QApplication::setApplicationDisplayName(QStringLiteral("KFilter"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0-qt6-port"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("KFilter Qt6 port"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("KFilter .kfp project file to open."));
    parser.process(app);

    KFilterQt6App window;
    const QStringList positionalArguments = parser.positionalArguments();
    if (!positionalArguments.isEmpty()) {
        window.openDocumentFile(QUrl::fromLocalFile(QFileInfo(positionalArguments.constFirst()).absoluteFilePath()));
    }
    window.show();

    return app.exec();
}
