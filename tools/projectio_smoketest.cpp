/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterprojectio.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QString>
#include <QTextStream>

#include <algorithm>
#include <cstddef>

namespace
{
bool fuzzyEqual(double left, double right)
{
    const double diff = left - right;
    return diff > -0.000001 && diff < 0.000001;
}

void populateDrivers(driver (&drivers)[KFilterProjectIo::DriverCount], bool includeFullCircuit)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& currentDriver = drivers[driverIndex];
        currentDriver.SetTitle(QStringLiteral("Driver %1 äöü").arg(driverIndex + 1));
        currentDriver.setRdc(5.0 + driverIndex);
        currentDriver.setLsp(0.00075 + driverIndex * 0.0001);
        currentDriver.setF0(40.0 + driverIndex);
        currentDriver.setQtc(0.7 + driverIndex);
        currentDriver.setQes(0.8 + driverIndex);
        currentDriver.setQms(4.0 + driverIndex);
        currentDriver.setVas(55.0 + driverIndex);
        currentDriver.setDm(18.0 + driverIndex);
        currentDriver.Vb = 20.0 + driverIndex;
        currentDriver.setQl(6.5 + driverIndex);
        currentDriver.Fb = 42.0 + driverIndex;
        currentDriver.V2 = 12.0 + driverIndex;
        currentDriver.GTypProposal = driverIndex + 1;
        currentDriver.gain = 1.5 + driverIndex;
        currentDriver.PressureisActive = (driverIndex % 2) == 0;
        currentDriver.ImpedanzisActive = true;
        currentDriver.SummaryisActive = false;
        currentDriver.ScalarSummaryisActive = true;
        currentDriver.ImpedanzSummaryisActive = false;
        currentDriver.InvertPhase = (driverIndex % 2) != 0;
        currentDriver.setFullCircuit(includeFullCircuit && (driverIndex % 2) == 0);

        for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
            currentDriver.setUnit(unitIndex, driverIndex * 100.0 + unitIndex / 10.0);
        }
    }
}

bool compareDrivers(driver (&expected)[KFilterProjectIo::DriverCount],
                    driver (&actual)[KFilterProjectIo::DriverCount],
                    bool compareFullCircuit,
                    QString& error)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& expectedDriver = expected[driverIndex];
        driver& actualDriver = actual[driverIndex];

        if (expectedDriver.GetTitle() != actualDriver.GetTitle() ||
            !fuzzyEqual(expectedDriver.getRdc(), actualDriver.getRdc()) ||
            !fuzzyEqual(expectedDriver.getLsp(), actualDriver.getLsp()) ||
            !fuzzyEqual(expectedDriver.getF0(), actualDriver.getF0()) ||
            !fuzzyEqual(expectedDriver.getQtc(), actualDriver.getQtc()) ||
            !fuzzyEqual(expectedDriver.getQes(), actualDriver.getQes()) ||
            !fuzzyEqual(expectedDriver.getQms(), actualDriver.getQms()) ||
            !fuzzyEqual(expectedDriver.getVas(), actualDriver.getVas()) ||
            !fuzzyEqual(expectedDriver.getDm(), actualDriver.getDm()) ||
            !fuzzyEqual(expectedDriver.Vb, actualDriver.Vb) ||
            !fuzzyEqual(expectedDriver.getQl(), actualDriver.getQl()) ||
            !fuzzyEqual(expectedDriver.Fb, actualDriver.Fb) ||
            !fuzzyEqual(expectedDriver.V2, actualDriver.V2) ||
            expectedDriver.GTypProposal != actualDriver.GTypProposal ||
            !fuzzyEqual(expectedDriver.gain, actualDriver.gain) ||
            expectedDriver.PressureisActive != actualDriver.PressureisActive ||
            expectedDriver.ImpedanzisActive != actualDriver.ImpedanzisActive ||
            expectedDriver.SummaryisActive != actualDriver.SummaryisActive ||
            expectedDriver.ScalarSummaryisActive != actualDriver.ScalarSummaryisActive ||
            expectedDriver.ImpedanzSummaryisActive != actualDriver.ImpedanzSummaryisActive ||
            expectedDriver.InvertPhase != actualDriver.InvertPhase ||
            (compareFullCircuit && expectedDriver.getFullCircuit() != actualDriver.getFullCircuit())) {
            error = QStringLiteral("Parameter round-trip mismatch for driver %1").arg(driverIndex + 1);
            return false;
        }

        for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
            if (!fuzzyEqual(expectedDriver.getUnit(unitIndex), actualDriver.getUnit(unitIndex))) {
                error = QStringLiteral("Network unit mismatch for driver %1, unit %2")
                            .arg(driverIndex + 1)
                            .arg(unitIndex);
                return false;
            }
        }
    }

    return true;
}


void populateMeasurements(KFilterProjectIo::MeasurementCurves& curves)
{
    curves[0].appendPoint(100.0, -2.5);
    curves[0].appendPoint(1000.0, 1.25);
    curves[2].appendPoint(80.0, 0.0);
    curves[2].appendPoint(400.0, -3.0);
    curves[2].appendPoint(5000.0, 2.0);
}

bool compareMeasurements(const KFilterProjectIo::MeasurementCurves& expected,
                         const KFilterProjectIo::MeasurementCurves& actual,
                         QString& error)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        const KFilterMeasurementCurve& expectedCurve = expected[static_cast<std::size_t>(driverIndex)];
        const KFilterMeasurementCurve& actualCurve = actual[static_cast<std::size_t>(driverIndex)];
        if (expectedCurve.size() != actualCurve.size()) {
            error = QStringLiteral("Measurement point count mismatch for driver %1").arg(driverIndex + 1);
            return false;
        }

        for (qsizetype pointIndex = 0; pointIndex < expectedCurve.size(); ++pointIndex) {
            const KFilterMeasurementPoint& expectedPoint = expectedCurve.points().at(pointIndex);
            const KFilterMeasurementPoint& actualPoint = actualCurve.points().at(pointIndex);
            if (!fuzzyEqual(expectedPoint.frequencyHz, actualPoint.frequencyHz) ||
                !fuzzyEqual(expectedPoint.value, actualPoint.value)) {
                error = QStringLiteral("Measurement point mismatch for driver %1, point %2")
                            .arg(driverIndex + 1)
                            .arg(pointIndex + 1);
                return false;
            }
        }
    }

    return true;
}

bool compareMeasurementHiddenStates(
    const KFilterProjectIo::MeasurementHiddenStates& expected,
    const KFilterProjectIo::MeasurementHiddenStates& actual,
    QString& error)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        const std::size_t index = static_cast<std::size_t>(driverIndex);
        if (expected[index] != actual[index]) {
            error = QStringLiteral("Measurement hidden-state mismatch for driver %1")
                        .arg(driverIndex + 1);
            return false;
        }
    }
    return true;
}

QString createLegacyProject(driver (&drivers)[KFilterProjectIo::DriverCount], bool includeQlSection)
{
    QString content;
    QTextStream stream(&content);
    stream.setLocale(QLocale::c());
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(9);

    stream << "# KFilter datafile\n[Network values]";
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        stream << "\n# Driver " << (driverIndex + 1);
        for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
            stream << '\n' << drivers[driverIndex].getUnit(unitIndex);
        }
    }

    stream.setRealNumberPrecision(6);
    stream << "\n[Driver parameters]";
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& currentDriver = drivers[driverIndex];
        stream << "\n# Driver " << (driverIndex + 1)
               << "\nRdc=" << currentDriver.getRdc()
               << "\nLsp=" << currentDriver.getLsp()
               << "\nF0=" << currentDriver.getF0()
               << "\nQts=" << currentDriver.getQtc()
               << "\nQe=" << currentDriver.getQes()
               << "\nQms=" << currentDriver.getQms()
               << "\nVas=" << currentDriver.getVas()
               << "\nDm=" << currentDriver.getDm()
               << "\nVb=" << currentDriver.Vb
               << "\nFb=" << currentDriver.Fb
               << "\nV2=" << currentDriver.V2
               << "\nGTypProposal=" << currentDriver.GTypProposal
               << "\nGain=" << currentDriver.gain
               << "\nPressure=" << (currentDriver.PressureisActive ? 1 : 0)
               << "\nImpedanz=" << (currentDriver.ImpedanzisActive ? 1 : 0)
               << "\nSummary=" << (currentDriver.SummaryisActive ? 1 : 0)
               << "\nScalarSummary=" << (currentDriver.ScalarSummaryisActive ? 1 : 0)
               << "\nImpedanzSummary=" << (currentDriver.ImpedanzSummaryisActive ? 1 : 0)
               << "\nInvertPhase=" << (currentDriver.InvertPhase ? 1 : 0)
               << "\nTitle=" << currentDriver.GetTitle();
    }

    if (includeQlSection) {
        stream << "\n[Driver enclosure losses]";
        for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
            stream << "\n# Driver " << (driverIndex + 1)
                   << "\nQl=" << drivers[driverIndex].getQl();
        }
    }

    stream << '\n';
    return content;
}

bool writeTextFile(const QString& filePath, const QString& content, QString& error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        error = QStringLiteral("Could not create '%1': %2").arg(filePath, file.errorString());
        return false;
    }

    if (file.write(content.toUtf8()) < 0) {
        error = QStringLiteral("Could not write '%1': %2").arg(filePath, file.errorString());
        return false;
    }

    return true;
}

bool readJsonRoot(const QString& filePath, QJsonObject& root, QString& error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Could not read '%1': %2").arg(filePath, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Saved project is not a valid JSON object: %1").arg(parseError.errorString());
        return false;
    }

    root = document.object();
    return true;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    (void)app;

    const QString jsonFilePath = QDir::temp().filePath(QStringLiteral("kfilter_projectio_json_smoketest.kfp"));
    const QString legacyFilePath = QDir::temp().filePath(QStringLiteral("kfilter_projectio_legacy_smoketest.kfp"));
    const QString oldLegacyFilePath = QDir::temp().filePath(QStringLiteral("kfilter_projectio_old_legacy_smoketest.kfp"));
    const QString invalidFilePath = QDir::temp().filePath(QStringLiteral("kfilter_projectio_invalid_smoketest.kfp"));

    QString errorMessage;
    driver original[KFilterProjectIo::DriverCount];
    populateDrivers(original, true);
    KFilterProjectIo::MeasurementCurves originalMeasurements;
    populateMeasurements(originalMeasurements);
    KFilterProjectIo::MeasurementHiddenStates originalHiddenStates{};
    originalHiddenStates[0] = true;

    if (!KFilterProjectIo::saveToFile(jsonFilePath,
                                      original,
                                      originalMeasurements,
                                      true,
                                      originalHiddenStates,
                                      &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    QJsonObject validRoot;
    if (!readJsonRoot(jsonFilePath, validRoot, errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    const QJsonObject project = validRoot.value(QStringLiteral("project")).toObject();
    const QJsonArray jsonDrivers = project.value(QStringLiteral("drivers")).toArray();
    const QJsonObject measurementSettings =
        project.value(QStringLiteral("measurementSettings")).toObject();
    const QJsonObject firstDriverMeasurements =
        jsonDrivers.at(0).toObject().value(QStringLiteral("measurements")).toObject();
    const QJsonObject firstCorrection =
        firstDriverMeasurements.value(QStringLiteral("splCorrection")).toObject();
    const QJsonObject thirdCorrection =
        jsonDrivers.at(2).toObject()
            .value(QStringLiteral("measurements")).toObject()
            .value(QStringLiteral("splCorrection")).toObject();
    if (validRoot.value(QStringLiteral("format")).toString() != QStringLiteral("KFilter project") ||
        validRoot.value(QStringLiteral("formatVersion")).toInt(-1) != KFilterProjectIo::JsonFormatVersion ||
        jsonDrivers.size() != KFilterProjectIo::DriverCount ||
        !measurementSettings.value(QStringLiteral("mergeCorrectionCurves")).toBool(false) ||
        measurementSettings.contains(QStringLiteral("hideMeasurements")) ||
        firstCorrection.value(QStringLiteral("hidden")).toBool(false) != true ||
        thirdCorrection.value(QStringLiteral("hidden")).toBool(true) != false) {
        QTextStream(stderr) << "Saved JSON project metadata, measurements or driver count is invalid\n";
        return 1;
    }

    driver jsonLoaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves jsonLoadedMeasurements;
    bool jsonLoadedMergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates jsonLoadedHiddenStates{};
    if (!KFilterProjectIo::loadFromFile(jsonFilePath,
                                        jsonLoaded,
                                        jsonLoadedMeasurements,
                                        jsonLoadedMergeEnabled,
                                        jsonLoadedHiddenStates,
                                        &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    if (!compareDrivers(original, jsonLoaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, jsonLoadedMeasurements, errorMessage) ||
        !compareMeasurementHiddenStates(originalHiddenStates, jsonLoadedHiddenStates, errorMessage) ||
        !jsonLoadedMergeEnabled) {
        QTextStream(stderr) << (errorMessage.isEmpty()
                                    ? QStringLiteral("Measurement merge/per-driver hide state was not restored")
                                    : errorMessage)
                            << '\n';
        return 1;
    }

    driver legacyOriginal[KFilterProjectIo::DriverCount];
    populateDrivers(legacyOriginal, false);
    if (!writeTextFile(legacyFilePath, createLegacyProject(legacyOriginal, true), errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver legacyLoaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves legacyMeasurements;
    legacyMeasurements[0].appendPoint(50.0, 9.0);
    bool legacyMergeEnabled = true;
    KFilterProjectIo::MeasurementHiddenStates legacyHiddenStates{};
    legacyHiddenStates.fill(true);
    if (!KFilterProjectIo::loadFromFile(legacyFilePath,
                                        legacyLoaded,
                                        legacyMeasurements,
                                        legacyMergeEnabled,
                                        legacyHiddenStates,
                                        &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    if (!compareDrivers(legacyOriginal, legacyLoaded, false, errorMessage) ||
        !std::all_of(legacyMeasurements.cbegin(),
                     legacyMeasurements.cend(),
                     [](const KFilterMeasurementCurve& curve) { return curve.isEmpty(); }) ||
        legacyMergeEnabled ||
        std::any_of(legacyHiddenStates.cbegin(), legacyHiddenStates.cend(),
                    [](bool hidden) { return hidden; })) {
        QTextStream(stderr) << (errorMessage.isEmpty()
                                    ? QStringLiteral("Legacy load did not reset measurement state")
                                    : errorMessage)
                            << '\n';
        return 1;
    }

    if (!KFilterProjectIo::saveToFile(legacyFilePath,
                                      legacyLoaded,
                                      legacyMeasurements,
                                      legacyMergeEnabled,
                                      legacyHiddenStates,
                                      &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    QJsonObject migratedLegacyRoot;
    if (!readJsonRoot(legacyFilePath, migratedLegacyRoot, errorMessage) ||
        migratedLegacyRoot.value(QStringLiteral("formatVersion")).toInt(-1) !=
            KFilterProjectIo::JsonFormatVersion) {
        QTextStream(stderr) << "Legacy project was not migrated to current JSON format: "
                            << errorMessage << '\n';
        return 1;
    }

    if (!writeTextFile(oldLegacyFilePath, createLegacyProject(legacyOriginal, false), errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver oldLegacyLoaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves oldLegacyMeasurements;
    bool oldLegacyMergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates oldLegacyHiddenStates{};
    if (!KFilterProjectIo::loadFromFile(oldLegacyFilePath,
                                        oldLegacyLoaded,
                                        oldLegacyMeasurements,
                                        oldLegacyMergeEnabled,
                                        oldLegacyHiddenStates,
                                        &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        if (!fuzzyEqual(oldLegacyLoaded[driverIndex].getQl(), 10.0)) {
            QTextStream(stderr) << "Old legacy Ql default mismatch for driver "
                                << (driverIndex + 1) << '\n';
            return 1;
        }
    }

    // Patch 155 JSON files contain neither measurementSettings nor per-driver measurements.
    QJsonObject patch155Root = validRoot;
    patch155Root.insert(QStringLiteral("formatVersion"), KFilterProjectIo::LegacyJsonFormatVersion);
    QJsonObject patch155Project = patch155Root.value(QStringLiteral("project")).toObject();
    patch155Project.remove(QStringLiteral("measurementSettings"));
    QJsonArray patch155Drivers = patch155Project.value(QStringLiteral("drivers")).toArray();
    for (int index = 0; index < patch155Drivers.size(); ++index) {
        QJsonObject driverObject = patch155Drivers.at(index).toObject();
        driverObject.remove(QStringLiteral("measurements"));
        patch155Drivers[index] = driverObject;
    }
    patch155Project.insert(QStringLiteral("drivers"), patch155Drivers);
    patch155Root.insert(QStringLiteral("project"), patch155Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(patch155Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver patch155Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves patch155Measurements;
    bool patch155MergeEnabled = true;
    KFilterProjectIo::MeasurementHiddenStates patch155HiddenStates{};
    patch155HiddenStates.fill(true);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        patch155Loaded,
                                        patch155Measurements,
                                        patch155MergeEnabled,
                                        patch155HiddenStates,
                                        &errorMessage) ||
        !compareDrivers(original, patch155Loaded, true, errorMessage) ||
        !std::all_of(patch155Measurements.cbegin(),
                     patch155Measurements.cend(),
                     [](const KFilterMeasurementCurve& curve) { return curve.isEmpty(); }) ||
        patch155MergeEnabled ||
        std::any_of(patch155HiddenStates.cbegin(), patch155HiddenStates.cend(),
                    [](bool hidden) { return hidden; })) {
        QTextStream(stderr) << "Patch 155 JSON compatibility failed: " << errorMessage << '\n';
        return 1;
    }

    // Format version 3 stores one global hide flag. It migrates to all
    // drivers that actually contain a measurement curve.
    QJsonObject version3Root = validRoot;
    version3Root.insert(QStringLiteral("formatVersion"), 3);
    QJsonObject version3Project = version3Root.value(QStringLiteral("project")).toObject();
    QJsonObject version3Settings =
        version3Project.value(QStringLiteral("measurementSettings")).toObject();
    version3Settings.insert(QStringLiteral("hideMeasurements"), true);
    version3Project.insert(QStringLiteral("measurementSettings"), version3Settings);
    version3Root.insert(QStringLiteral("project"), version3Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version3Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version3Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version3Measurements;
    bool version3MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version3HiddenStates{};
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version3Loaded,
                                        version3Measurements,
                                        version3MergeEnabled,
                                        version3HiddenStates,
                                        &errorMessage) ||
        !compareDrivers(original, version3Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version3Measurements, errorMessage) ||
        !version3MergeEnabled ||
        !version3HiddenStates[0] || version3HiddenStates[1] ||
        !version3HiddenStates[2] || version3HiddenStates[3]) {
        QTextStream(stderr) << "Format version 3 global-hide migration failed: "
                            << errorMessage << '\n';
        return 1;
    }

    // Format version 2 persists merge state but predates every hide state.
    QJsonObject version2Root = validRoot;
    version2Root.insert(QStringLiteral("formatVersion"), 2);
    QJsonObject version2Project = version2Root.value(QStringLiteral("project")).toObject();
    QJsonObject version2Settings =
        version2Project.value(QStringLiteral("measurementSettings")).toObject();
    version2Settings.remove(QStringLiteral("hideMeasurements"));
    version2Project.insert(QStringLiteral("measurementSettings"), version2Settings);
    version2Root.insert(QStringLiteral("project"), version2Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version2Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version2Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version2Measurements;
    bool version2MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version2HiddenStates{};
    version2HiddenStates.fill(true);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version2Loaded,
                                        version2Measurements,
                                        version2MergeEnabled,
                                        version2HiddenStates,
                                        &errorMessage) ||
        !compareDrivers(original, version2Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version2Measurements, errorMessage) ||
        !version2MergeEnabled ||
        std::any_of(version2HiddenStates.cbegin(), version2HiddenStates.cend(),
                    [](bool hidden) { return hidden; })) {
        QTextStream(stderr) << "Format version 2 compatibility failed: " << errorMessage << '\n';
        return 1;
    }

    // Invalid measurement point ordering must fail transactionally.
    QJsonObject invalidMeasurementRoot = validRoot;
    QJsonObject invalidMeasurementProject =
        invalidMeasurementRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidMeasurementDrivers =
        invalidMeasurementProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject firstDriver = invalidMeasurementDrivers.at(0).toObject();
    QJsonObject firstMeasurements = firstDriver.value(QStringLiteral("measurements")).toObject();
    QJsonObject invalidFirstCorrection =
        firstMeasurements.value(QStringLiteral("splCorrection")).toObject();
    QJsonArray invalidPoints = invalidFirstCorrection.value(QStringLiteral("points")).toArray();
    QJsonObject secondPoint = invalidPoints.at(1).toObject();
    secondPoint.insert(QStringLiteral("frequencyHz"), 50.0);
    invalidPoints[1] = secondPoint;
    invalidFirstCorrection.insert(QStringLiteral("points"), invalidPoints);
    firstMeasurements.insert(QStringLiteral("splCorrection"), invalidFirstCorrection);
    firstDriver.insert(QStringLiteral("measurements"), firstMeasurements);
    invalidMeasurementDrivers[0] = firstDriver;
    invalidMeasurementProject.insert(QStringLiteral("drivers"), invalidMeasurementDrivers);
    invalidMeasurementRoot.insert(QStringLiteral("project"), invalidMeasurementProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidMeasurementRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver unchanged[KFilterProjectIo::DriverCount];
    unchanged[0].SetTitle(QStringLiteral("unchanged sentinel"));
    KFilterProjectIo::MeasurementCurves unchangedMeasurements;
    unchangedMeasurements[0].appendPoint(123.0, 4.5);
    bool unchangedMergeEnabled = true;
    KFilterProjectIo::MeasurementHiddenStates unchangedHiddenStates{};
    unchangedHiddenStates.fill(true);
    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                       unchanged,
                                       unchangedMeasurements,
                                       unchangedMergeEnabled,
                                       unchangedHiddenStates,
                                       &errorMessage)) {
        QTextStream(stderr) << "Invalid measurement point ordering was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 ||
        !fuzzyEqual(unchangedMeasurements[0].points().constFirst().frequencyHz, 123.0) ||
        !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; })) {
        QTextStream(stderr) << "Failed measurement load modified the destination project state\n";
        return 1;
    }

    // Invalid per-driver hide state must fail transactionally.
    QJsonObject invalidHideRoot = validRoot;
    QJsonObject invalidHideProject = invalidHideRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidHideDrivers =
        invalidHideProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidHideDriver = invalidHideDrivers.at(0).toObject();
    QJsonObject invalidHideMeasurements =
        invalidHideDriver.value(QStringLiteral("measurements")).toObject();
    QJsonObject invalidHideCorrection =
        invalidHideMeasurements.value(QStringLiteral("splCorrection")).toObject();
    invalidHideCorrection.insert(QStringLiteral("hidden"), QStringLiteral("yes"));
    invalidHideMeasurements.insert(QStringLiteral("splCorrection"), invalidHideCorrection);
    invalidHideDriver.insert(QStringLiteral("measurements"), invalidHideMeasurements);
    invalidHideDrivers[0] = invalidHideDriver;
    invalidHideProject.insert(QStringLiteral("drivers"), invalidHideDrivers);
    invalidHideRoot.insert(QStringLiteral("project"), invalidHideProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidHideRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                       unchanged,
                                       unchangedMeasurements,
                                       unchangedMergeEnabled,
                                       unchangedHiddenStates,
                                       &errorMessage)) {
        QTextStream(stderr) << "Invalid per-driver hidden value was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 ||
        !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; })) {
        QTextStream(stderr) << "Failed hide-state load modified the destination project state\n";
        return 1;
    }

    QJsonObject unsupportedVersionRoot = validRoot;
    unsupportedVersionRoot.insert(QStringLiteral("formatVersion"), 999);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(unsupportedVersionRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                       unchanged,
                                       unchangedMeasurements,
                                       unchangedMergeEnabled,
                                       unchangedHiddenStates,
                                       &errorMessage)) {
        QTextStream(stderr) << "Unsupported JSON project version was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 ||
        !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; })) {
        QTextStream(stderr) << "Failed JSON load modified the destination project state\n";
        return 1;
    }

    QFile::remove(invalidFilePath);
    QFile::remove(oldLegacyFilePath);
    QFile::remove(legacyFilePath);
    QFile::remove(jsonFilePath);
    QTextStream(stdout) << "KFilterProjectIo JSON measurement and legacy compatibility smoke test passed\n";
    return 0;
}
