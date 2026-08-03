/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "correctioncurveexport.h"

#include <QCoreApplication>
#include <QFile>
#include <QLocale>
#include <QTemporaryDir>

#include <iostream>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "measurement export smoketest failed: " << message << '\n';
        return false;
    }
    return true;
}

QString readUtf8File(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir temporaryDir;
    if (!require(temporaryDir.isValid(), "temporary directory could not be created")) {
        return 1;
    }

    const QString fileName = temporaryDir.filePath(QStringLiteral("driver-correction.frd"));

    KFilterMeasurementCurve curve;
    curve.points.append(KFilterMeasurementPoint{2000.0, -3.0});
    curve.points.append(KFilterMeasurementPoint{1000.0, 6.0});

    const QLocale previousLocale = QLocale();
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    const KFilterCorrectionCurveExportResult exportResult =
        exportKFilterCorrectionCurveAsFrd(fileName,
                                          curve,
                                          QStringLiteral("Driver 2: Midrange\ninvalid-header-line"),
                                          QStringLiteral("163"));
    QLocale::setDefault(previousLocale);

    if (!require(exportResult.isValid(), "valid correction curve was rejected") ||
        !require(exportResult.exportedPointCount == 2, "unexpected exported point count")) {
        return 1;
    }

    const QString contents = readUtf8File(fileName);
    if (!require(!contents.isEmpty(), "exported FRD file could not be read") ||
        !require(contents.contains(QStringLiteral("* KFilter-Data-Type: magnitude-correction\n")),
                 "data type metadata is missing") ||
        !require(contents.contains(QStringLiteral("* KFilter-Phase-Meaning: none\n")),
                 "phase metadata is missing") ||
        !require(contents.contains(QStringLiteral("* Levels are relative corrections, not absolute SPL\n")),
                 "relative correction warning is missing") ||
        !require(contents.contains(QStringLiteral("* Driver: Driver 2: Midrange invalid-header-line\n")),
                 "driver metadata was not sanitized") ||
        !require(contents.contains(QStringLiteral("* Patch level: 163\n")),
                 "patch metadata is missing") ||
        !require(contents.contains(QStringLiteral("1000.000000    6.000000    0.000\n")),
                 "first correction point is missing or locale-dependent") ||
        !require(contents.contains(QStringLiteral("2000.000000    -3.000000    0.000\n")),
                 "second correction point is missing") ||
        !require(contents.indexOf(QStringLiteral("1000.000000")) <
                     contents.indexOf(QStringLiteral("2000.000000")),
                 "exported frequencies are not sorted") ||
        !require(!contents.contains(QStringLiteral("1000,000000")),
                 "system locale changed the decimal separator")) {
        return 1;
    }

    KFilterMeasurementCurve emptyCurve;
    const KFilterCorrectionCurveExportResult emptyResult =
        exportKFilterCorrectionCurveAsFrd(temporaryDir.filePath(QStringLiteral("empty.frd")),
                                          emptyCurve,
                                          QStringLiteral("Driver 1"),
                                          QStringLiteral("163"));
    if (!require(!emptyResult.isValid(), "empty correction curve was accepted")) {
        return 1;
    }

    KFilterMeasurementCurve duplicateCurve;
    duplicateCurve.points.append(KFilterMeasurementPoint{1000.0, 1.0});
    duplicateCurve.points.append(KFilterMeasurementPoint{1000.0, 2.0});
    const KFilterCorrectionCurveExportResult duplicateResult =
        exportKFilterCorrectionCurveAsFrd(temporaryDir.filePath(QStringLiteral("duplicate.frd")),
                                          duplicateCurve,
                                          QStringLiteral("Driver 1"),
                                          QStringLiteral("163"));
    if (!require(!duplicateResult.isValid(), "duplicate frequencies were accepted")) {
        return 1;
    }

    std::cout << "measurement export smoketest passed\n";
    return 0;
}
