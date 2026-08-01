/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "correctioncurveimport.h"
#include "measurementcurveparser.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>
#include <iostream>

namespace
{
bool nearlyEqual(double lhs, double rhs, double tolerance = 1e-9)
{
    return std::abs(lhs - rhs) <= tolerance;
}

bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "measurement import smoketest failed: " << message << '\n';
        return false;
    }
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir temporaryDir;
    if (!require(temporaryDir.isValid(), "temporary directory could not be created")) {
        return 1;
    }

    const QString fileName = temporaryDir.filePath(QStringLiteral("measurement.frd"));
    QFile file(fileName);
    if (!require(file.open(QIODevice::WriteOnly | QIODevice::Text),
                 "temporary measurement file could not be opened")) {
        return 1;
    }

    QTextStream stream(&file);
    stream << "# frequency level phase\n";
    stream << "100 80 -10\n";
    stream << "200;81;0\n";
    stream << "300,82.5,5\n";
    stream << "400,0;83,0;10,0\n";
    stream << "200 83 0\n";
    stream << "invalid row\n";
    file.close();

    const KFilterMeasurementParseResult parseResult = parseKFilterMeasurementFile(fileName);
    if (!require(parseResult.isValid(), "valid mixed-format input was rejected") ||
        !require(parseResult.points.size() == 4, "unexpected number of distinct parsed points") ||
        !require(parseResult.duplicateFrequencyCount == 1,
                 "duplicate frequency was not reported") ||
        !require(nearlyEqual(parseResult.points.at(1).frequencyHz, 200.0),
                 "parsed frequency order is wrong") ||
        !require(nearlyEqual(parseResult.points.at(1).levelDb, 82.0),
                 "duplicate levels were not combined using the median") ||
        !require(nearlyEqual(parseResult.points.at(2).levelDb, 82.5),
                 "comma-delimited level was parsed incorrectly") ||
        !require(nearlyEqual(parseResult.points.at(3).frequencyHz, 400.0),
                 "decimal-comma frequency was parsed incorrectly") ||
        !require(nearlyEqual(parseResult.points.at(3).levelDb, 83.0),
                 "decimal-comma level was parsed incorrectly")) {
        return 1;
    }

    const QVector<KFilterImportedMeasurementPoint> measurements{
        {100.0, 80.0},
        {125.0, 80.0},
        {150.0, 80.0},
        {200.0, 82.0},
        {300.0, 84.0},
        {400.0, 84.0},
        {800.0, 86.0},
        {1600.0, 74.0}
    };

    KFilterCorrectionImportSettings settings;
    settings.calibrationMinHz = 100.0;
    settings.calibrationMaxHz = 150.0;
    settings.manualOffsetDb = 0.0;
    settings.correctionMinHz = 200.0;
    settings.correctionMaxHz = 1600.0;
    settings.lowerFadeEnabled = true;
    settings.lowerFadeOctaves = 1.0;
    settings.upperFadeEnabled = false;

    KFilterCorrectionImportResult importResult =
        createKFilterCorrectionCurve(measurements, settings);
    if (!require(importResult.isValid(), "valid calibration/window settings were rejected") ||
        !require(nearlyEqual(importResult.referenceMedianDb, 80.0),
                 "calibration median is wrong") ||
        !require(nearlyEqual(importResult.automaticOffsetDb, -80.0),
                 "automatic offset is wrong") ||
        !require(importResult.correctionCurve.size() == 5,
                 "correction window did not retain expected points") ||
        !require(nearlyEqual(importResult.correctionCurve.points.at(0).frequencyHz, 200.0),
                 "lower boundary point is missing") ||
        !require(nearlyEqual(importResult.correctionCurve.points.at(0).value, 0.0),
                 "lower fade does not start neutrally") ||
        !require(importResult.correctionCurve.points.at(1).value > 0.0 &&
                     importResult.correctionCurve.points.at(1).value < 4.0,
                 "lower fade did not partially weight an interior point") ||
        !require(nearlyEqual(importResult.correctionCurve.points.at(2).value, 4.0),
                 "lower fade endpoint is not fully active") ||
        !require(nearlyEqual(importResult.correctionCurve.points.at(3).value, 6.0),
                 "normalized in-window correction is wrong") ||
        !require(nearlyEqual(importResult.correctionCurve.points.at(4).value, -6.0),
                 "upper in-window correction is wrong")) {
        return 1;
    }

    settings.manualOffsetDb = 1.5;
    settings.upperFadeEnabled = true;
    settings.upperFadeOctaves = 1.0;
    importResult = createKFilterCorrectionCurve(measurements, settings);
    if (!require(importResult.isValid(), "valid dual-fade settings were rejected") ||
        !require(nearlyEqual(importResult.effectiveOffsetDb, -78.5),
                 "manual offset was not added") ||
        !require(nearlyEqual(importResult.correctionCurve.points.constLast().value, 0.0),
                 "upper fade does not end neutrally")) {
        return 1;
    }

    settings.calibrationMinHz = 151.0;
    settings.calibrationMaxHz = 175.0;
    importResult = createKFilterCorrectionCurve(measurements, settings);
    if (!require(!importResult.isValid(),
                 "calibration range without a measurement point was accepted")) {
        return 1;
    }

    settings.calibrationMinHz = 100.0;
    settings.calibrationMaxHz = 150.0;
    settings.correctionMinHz = 151.0;
    settings.correctionMaxHz = 199.0;
    settings.lowerFadeEnabled = false;
    settings.upperFadeEnabled = false;
    importResult = createKFilterCorrectionCurve(measurements, settings);
    if (!require(!importResult.isValid(),
                 "correction window without two source points was accepted")) {
        return 1;
    }

    std::cout << "measurement import smoketest passed\n";
    return 0;
}
