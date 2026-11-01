/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "impedancereferencecurve.h"
#include "kfilterdoc.h"
#include "kfilterdriverio.h"

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
        std::cerr << "impedance reference smoketest failed: " << message << '\n';
        return false;
    }
    return true;
}

bool writeFile(const QString& fileName, const QString& content)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream(&file) << content;
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

    // Header and layout as written by tsp_impedance_measure.py --zma.
    const QString validFile = temporaryDir.filePath(QStringLiteral("driver.zma"));
    if (!require(writeFile(validFile,
                           QStringLiteral("* Impedance measurement, tsp_impedance_measure.py\n"
                                          "* Rs: 9.9 ohm\n"
                                          "* Freq[Hz]  |Z|[Ohm]  Phase[deg]\n"
                                          "300.0000 6.112400 -3.2100\n"
                                          "\n"
                                          "# other tools use hash comments\n"
                                          "400;6,2;-1,5\n"
                                          "500 6.3\n"
                                          "not a data row\n"
                                          "600 -1.0 2.0\n"
                                          "20000.0000 8.559093 28.8342\n")),
                 "valid ZMA fixture could not be written")) {
        return 1;
    }

    const KFilterImpedanceParseResult result = parseKFilterZmaFile(validFile);
    if (!require(result.isValid(), "valid ZMA file was rejected") ||
        !require(result.points.size() == 4, "unexpected ZMA point count") ||
        !require(result.ignoredLineCount == 2, "invalid ZMA rows were not counted") ||
        !require(nearlyEqual(result.points.at(0).frequencyHz, 300.0) &&
                     nearlyEqual(result.points.at(0).magnitudeOhm, 6.1124) &&
                     nearlyEqual(result.points.at(0).phaseDeg, -3.21),
                 "first ZMA row mismatch") ||
        !require(nearlyEqual(result.points.at(1).magnitudeOhm, 6.2) &&
                     nearlyEqual(result.points.at(1).phaseDeg, -1.5),
                 "semicolon/decimal-comma ZMA row mismatch") ||
        !require(std::isnan(result.points.at(2).phaseDeg), "missing phase is not NaN") ||
        !require(nearlyEqual(result.points.at(3).frequencyHz, 20000.0) &&
                     nearlyEqual(result.points.at(3).magnitudeOhm, 8.559093),
                 "last ZMA row mismatch")) {
        return 1;
    }

    const QString descendingFile = temporaryDir.filePath(QStringLiteral("descending.zma"));
    if (!require(writeFile(descendingFile, QStringLiteral("100 6 0\n90 6 0\n110 6 0\n")),
                 "descending ZMA fixture could not be written") ||
        !require(!parseKFilterZmaFile(descendingFile).isValid(),
                 "non-increasing ZMA frequencies were accepted")) {
        return 1;
    }

    const QString shortFile = temporaryDir.filePath(QStringLiteral("short.zma"));
    if (!require(writeFile(shortFile, QStringLiteral("* only one row\n100 6 0\n")),
                 "short ZMA fixture could not be written") ||
        !require(!parseKFilterZmaFile(shortFile).isValid(), "single-row ZMA was accepted") ||
        !require(!parseKFilterZmaFile(temporaryDir.filePath(QStringLiteral("missing.zma"))).isValid(),
                 "missing ZMA file was accepted")) {
        return 1;
    }

    // Document state: reference only, never part of the modified project state.
    KFilterDoc document;
    document.setModified(false);
    if (!require(document.setImpedanceReferenceCurve(1, result.points), "setting ZMA curve failed") ||
        !require(document.impedanceReferenceCurve(1).size() == 4, "ZMA curve not stored") ||
        !require(!document.isModified(), "ZMA import marked the project modified") ||
        !require(!document.setImpedanceReferenceHiddenForDriver(0, true) &&
                     !document.impedanceReferenceHiddenForDriver(0),
                 "driver without ZMA accepted a hidden state") ||
        !require(document.setImpedanceReferenceHiddenForDriver(1, true) &&
                     document.impedanceReferenceHiddenForDriver(1),
                 "ZMA hide state not applied")) {
        return 1;
    }

    // Re-importing shows the curve again.
    document.setImpedanceReferenceCurve(1, result.points);
    if (!require(!document.impedanceReferenceHiddenForDriver(1), "re-import kept the ZMA hidden")) {
        return 1;
    }

    document.setImpedanceReferenceHiddenForDriver(1, true);
    if (!require(document.clearImpedanceReferenceCurve(1), "clearing ZMA reported no change") ||
        !require(document.impedanceReferenceCurve(1).isEmpty() &&
                     !document.impedanceReferenceHiddenForDriver(1),
                 "clearing ZMA left curve or hide state behind")) {
        return 1;
    }

    document.setImpedanceReferenceCurve(2, result.points);
    document.setImpedanceReferenceCurve(3, result.points);
    KFilterDriverIo::DriverSlot slot;
    if (!require(KFilterDriverIo::applyDriverSlotToDocument(document, 2, slot), "driver slot apply failed") ||
        !require(document.impedanceReferenceCurve(2).isEmpty(), ".kfd import kept the old ZMA") ||
        !require(!document.impedanceReferenceCurve(3).isEmpty(), ".kfd import cleared another driver's ZMA")) {
        return 1;
    }

    document.newDocument();
    if (!require(document.impedanceReferenceCurve(3).isEmpty(), "New did not clear ZMA curves")) {
        return 1;
    }

    std::cout << "impedance reference smoketest passed\n";
    return 0;
}
