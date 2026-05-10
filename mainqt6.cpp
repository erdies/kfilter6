/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterqt6app.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QSize>
#include <QUrl>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("KFilter"));
    QApplication::setOrganizationDomain(QStringLiteral("kfilter.local"));
    QApplication::setApplicationName(QStringLiteral("KFilter6"));
    QApplication::setApplicationDisplayName(QStringLiteral("KFilter6"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0-qt6-port"));

    QIcon applicationIcon;
    applicationIcon.addFile(QStringLiteral(":/icons/kfilter6-16.png"), QSize(16, 16));
    applicationIcon.addFile(QStringLiteral(":/icons/kfilter6-24.png"), QSize(24, 24));
    applicationIcon.addFile(QStringLiteral(":/icons/kfilter6-32.png"), QSize(32, 32));
    applicationIcon.addFile(QStringLiteral(":/icons/kfilter6-48.png"), QSize(48, 48));
    applicationIcon.addFile(QStringLiteral(":/icons/kfilter6-64.png"), QSize(64, 64));
    applicationIcon.addFile(QStringLiteral(":/icons/kfilter6-128.png"), QSize(128, 128));
    applicationIcon.addFile(QStringLiteral(":/icons/kfilter6-256.png"), QSize(256, 256));
    QApplication::setWindowIcon(applicationIcon);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("KFilter6 loudspeaker design and crossover modelling tool"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("KFilter6 .kfp project file to open."));
    parser.process(app);

    KFilterQt6App window;
    const QStringList positionalArguments = parser.positionalArguments();
    if (!positionalArguments.isEmpty()) {
        window.openDocumentFile(QUrl::fromLocalFile(QFileInfo(positionalArguments.constFirst()).absoluteFilePath()));
    }
    window.show();

    return app.exec();
}
