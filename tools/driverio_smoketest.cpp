/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterdriverio.h"
#include "kfilterdoc.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>
#include <variant>

namespace
{
bool fuzzyEqual(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

bool compareDriver(const driver& expected, const driver& actual, QString& error)
{
    if (expected.GetTitle() != actual.GetTitle() ||
        !fuzzyEqual(expected.getRdc(), actual.getRdc()) ||
        !fuzzyEqual(expected.getLsp(), actual.getLsp()) ||
        !fuzzyEqual(expected.getF0(), actual.getF0()) ||
        !fuzzyEqual(expected.getQtc(), actual.getQtc()) ||
        !fuzzyEqual(expected.getQes(), actual.getQes()) ||
        !fuzzyEqual(expected.getQms(), actual.getQms()) ||
        !fuzzyEqual(expected.getVas(), actual.getVas()) ||
        !fuzzyEqual(expected.getDm(), actual.getDm()) ||
        !fuzzyEqual(expected.Vb, actual.Vb) ||
        !fuzzyEqual(expected.getQl(), actual.getQl()) ||
        !fuzzyEqual(expected.Fb, actual.Fb) ||
        !fuzzyEqual(expected.V2, actual.V2) ||
        expected.GTypProposal != actual.GTypProposal ||
        !fuzzyEqual(expected.gain, actual.gain) ||
        expected.PressureisActive != actual.PressureisActive ||
        expected.ImpedanzisActive != actual.ImpedanzisActive ||
        expected.SummaryisActive != actual.SummaryisActive ||
        expected.ScalarSummaryisActive != actual.ScalarSummaryisActive ||
        expected.ImpedanzSummaryisActive != actual.ImpedanzSummaryisActive ||
        expected.InvertPhase != actual.InvertPhase ||
        expected.getFullCircuit() != actual.getFullCircuit()) {
        error = QStringLiteral("Driver parameter round-trip mismatch.");
        return false;
    }

    for (int unitIndex = 1; unitIndex <= KFilterDriverIo::NetworkUnitCount; ++unitIndex) {
        if (!fuzzyEqual(expected.getUnit(unitIndex), actual.getUnit(unitIndex))) {
            error = QStringLiteral("Network round-trip mismatch at unit %1.").arg(unitIndex);
            return false;
        }
    }
    return true;
}

bool compareMeasurement(const KFilterMeasurementCurve& expected,
                        const KFilterMeasurementCurve& actual,
                        QString& error)
{
    if (expected.size() != actual.size()) {
        error = QStringLiteral("Measurement point count mismatch.");
        return false;
    }
    for (qsizetype index = 0; index < expected.size(); ++index) {
        const KFilterMeasurementPoint& left = expected.points().at(index);
        const KFilterMeasurementPoint& right = actual.points().at(index);
        if (!fuzzyEqual(left.frequencyHz, right.frequencyHz) ||
            !fuzzyEqual(left.value, right.value)) {
            error = QStringLiteral("Measurement point mismatch at index %1.").arg(index);
            return false;
        }
    }
    return true;
}

bool compareActiveFilter(const ActiveFilterChain& expected,
                         const ActiveFilterChain& actual,
                         QString& error)
{
    if (expected.enabled() != actual.enabled() ||
        expected.showResponseInPlot() != actual.showResponseInPlot() ||
        expected.sectionCount() != actual.sectionCount()) {
        error = QStringLiteral("Active-filter chain metadata mismatch.");
        return false;
    }

    if (expected.sectionCount() != 2) {
        error = QStringLiteral("Unexpected active-filter test setup.");
        return false;
    }

    const ActiveFilterSection& expectedLowPass = expected.section(0);
    const ActiveFilterSection& actualLowPass = actual.section(0);
    if (expectedLowPass.enabled() != actualLowPass.enabled() ||
        expectedLowPass.type() != actualLowPass.type()) {
        error = QStringLiteral("Low-pass section metadata mismatch.");
        return false;
    }
    const auto& expectedLowPassParameters =
        std::get<ActiveFilterLowPassParameters>(expectedLowPass.parameters());
    const auto& actualLowPassParameters =
        std::get<ActiveFilterLowPassParameters>(actualLowPass.parameters());
    if (expectedLowPassParameters.characteristic != actualLowPassParameters.characteristic ||
        expectedLowPassParameters.order != actualLowPassParameters.order ||
        !fuzzyEqual(expectedLowPassParameters.frequencyHz, actualLowPassParameters.frequencyHz) ||
        !fuzzyEqual(expectedLowPassParameters.q, actualLowPassParameters.q)) {
        error = QStringLiteral("Low-pass section parameter mismatch.");
        return false;
    }

    const ActiveFilterSection& expectedGain = expected.section(1);
    const ActiveFilterSection& actualGain = actual.section(1);
    if (expectedGain.enabled() != actualGain.enabled() ||
        expectedGain.type() != actualGain.type() ||
        !fuzzyEqual(std::get<ActiveFilterGainParameters>(expectedGain.parameters()).gainDb,
                    std::get<ActiveFilterGainParameters>(actualGain.parameters()).gainDb)) {
        error = QStringLiteral("Gain section mismatch.");
        return false;
    }
    return true;
}

bool compareBaffle(const BaffleSettings& expected,
                   const BaffleSettings& actual,
                   QString& error)
{
    if (expected.enabled != actual.enabled ||
        expected.model != actual.model ||
        !fuzzyEqual(expected.widthMm, actual.widthMm) ||
        !fuzzyEqual(expected.heightMm, actual.heightMm) ||
        !fuzzyEqual(expected.driverXmm, actual.driverXmm) ||
        !fuzzyEqual(expected.driverYmm, actual.driverYmm) ||
        expected.boundaryCondition != actual.boundaryCondition ||
        expected.showResponseInPlot != actual.showResponseInPlot ||
        expected.edgeSourceCount != actual.edgeSourceCount ||
        expected.leftEdgeTreatment != actual.leftEdgeTreatment ||
        !fuzzyEqual(expected.leftChamferSetbackMm, actual.leftChamferSetbackMm) ||
        expected.rightEdgeTreatment != actual.rightEdgeTreatment ||
        !fuzzyEqual(expected.rightChamferSetbackMm, actual.rightChamferSetbackMm)) {
        error = QStringLiteral("Baffle settings round-trip mismatch.");
        return false;
    }
    return true;
}

bool compareFloor(const FloorReflectionSettings& expected,
                  const FloorReflectionSettings& actual,
                  QString& error)
{
    if (expected.enabled != actual.enabled ||
        !fuzzyEqual(expected.cabinetBottomAboveFloorMm, actual.cabinetBottomAboveFloorMm) ||
        !fuzzyEqual(expected.listenerHeightAboveFloorMm, actual.listenerHeightAboveFloorMm) ||
        !fuzzyEqual(expected.horizontalDistanceMm, actual.horizontalDistanceMm) ||
        expected.surfacePreset != actual.surfacePreset) {
        error = QStringLiteral("Floor-reflection settings round-trip mismatch.");
        return false;
    }
    return true;
}

void populateSlot(KFilterDriverIo::DriverSlot& slot)
{
    slot.driverData.SetTitle(QStringLiteral("Complete KFD Driver äöü"));
    slot.driverData.setRdc(5.85);
    slot.driverData.setLsp(0.00073);
    slot.driverData.setF0(41.5);
    slot.driverData.setQtc(0.39);
    slot.driverData.setQes(0.42);
    slot.driverData.setQms(5.6);
    slot.driverData.setVas(47.2);
    slot.driverData.setDm(16.8);
    slot.driverData.Vb = 28.0;
    slot.driverData.setQl(7.0);
    slot.driverData.Fb = 36.0;
    slot.driverData.V2 = 3.5;
    slot.driverData.GTypProposal = 2;
    slot.driverData.gain = 1.125;
    slot.driverData.PressureisActive = true;
    slot.driverData.ImpedanzisActive = false;
    slot.driverData.SummaryisActive = true;
    slot.driverData.ScalarSummaryisActive = false;
    slot.driverData.ImpedanzSummaryisActive = true;
    slot.driverData.InvertPhase = true;
    slot.driverData.setFullCircuit(true);
    for (int unitIndex = 1; unitIndex <= KFilterDriverIo::NetworkUnitCount; ++unitIndex) {
        slot.driverData.setUnit(unitIndex, 0.25 * unitIndex);
    }

    slot.measurementCurve.appendPoint(80.0, -2.25);
    slot.measurementCurve.appendPoint(800.0, 1.75);
    slot.measurementCurve.appendPoint(8000.0, -0.5);
    slot.measurementHidden = true;
    slot.mergeMeasurementsEnabled = true;

    slot.activeFilterChain.setEnabled(true);
    slot.activeFilterChain.setShowResponseInPlot(true);
    slot.activeFilterChain.addSection(ActiveFilterType::LowPass);
    auto& lowPass =
        std::get<ActiveFilterLowPassParameters>(slot.activeFilterChain.section(0).parameters());
    lowPass.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
    lowPass.order = 4;
    lowPass.frequencyHz = 2250.0;
    lowPass.q = 0.71;
    slot.activeFilterChain.addSection(ActiveFilterType::Gain);
    slot.activeFilterChain.section(1).setEnabled(false);
    std::get<ActiveFilterGainParameters>(slot.activeFilterChain.section(1).parameters()).gainDb = -1.75;

    slot.baffleSettings.enabled = true;
    slot.baffleSettings.model = BaffleModel::RectangularEdgeDiffraction;
    slot.baffleSettings.widthMm = 300.0;
    slot.baffleSettings.heightMm = 720.0;
    slot.baffleSettings.driverXmm = 132.0;
    slot.baffleSettings.driverYmm = 285.0;
    slot.baffleSettings.boundaryCondition = BaffleBoundaryCondition::FreeField;
    slot.baffleSettings.showResponseInPlot = true;
    slot.baffleSettings.edgeSourceCount = 73;
    slot.baffleSettings.leftEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    slot.baffleSettings.leftChamferSetbackMm = 18.0;
    slot.baffleSettings.rightEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    slot.baffleSettings.rightChamferSetbackMm = 22.0;

    slot.floorReflectionSettings.enabled = true;
    slot.floorReflectionSettings.cabinetBottomAboveFloorMm = 45.0;
    slot.floorReflectionSettings.listenerHeightAboveFloorMm = 1075.0;
    slot.floorReflectionSettings.horizontalDistanceMm = 2450.0;
    slot.floorReflectionSettings.surfacePreset = FloorSurfacePreset::MikiReference10mm100k;

    slot.hasTubeDiameterCm = true;
    slot.tubeDiameterCm = 6.8;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream errorStream(stderr);

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        errorStream << "Cannot create temporary directory.\n";
        return 1;
    }

    KFilterDriverIo::DriverSlot expected;
    populateSlot(expected);
    const QString filePath = temporaryDirectory.filePath(QStringLiteral("complete.kfd"));
    QString error;
    if (!KFilterDriverIo::saveDriverSlotToFile(filePath, expected, &error)) {
        errorStream << "Save failed: " << error << '\n';
        return 2;
    }

    QFile jsonFile(filePath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        errorStream << "Cannot inspect saved KFD.\n";
        return 3;
    }
    const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll());
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("formatVersion")).toInt() != 2 ||
        !root.value(QStringLiteral("measurements")).isObject() ||
        !root.value(QStringLiteral("activeFilter")).isObject() ||
        !root.value(QStringLiteral("baffle")).isObject() ||
        !root.value(QStringLiteral("floorReflection")).isObject() ||
        !root.value(QStringLiteral("measurementSettings")).isObject()) {
        errorStream << "Saved KFD v2 does not contain the complete driver-slot state.\n";
        return 4;
    }

    KFilterDriverIo::DriverSlot actual;
    if (!KFilterDriverIo::loadDriverSlotFromFile(filePath, actual, &error)) {
        errorStream << "Load failed: " << error << '\n';
        return 5;
    }

    if (!compareDriver(expected.driverData, actual.driverData, error) ||
        !compareMeasurement(expected.measurementCurve, actual.measurementCurve, error) ||
        expected.measurementHidden != actual.measurementHidden ||
        expected.mergeMeasurementsEnabled != actual.mergeMeasurementsEnabled ||
        !compareActiveFilter(expected.activeFilterChain, actual.activeFilterChain, error) ||
        !compareBaffle(expected.baffleSettings, actual.baffleSettings, error) ||
        !compareFloor(expected.floorReflectionSettings, actual.floorReflectionSettings, error) ||
        expected.hasTubeDiameterCm != actual.hasTubeDiameterCm ||
        !fuzzyEqual(expected.tubeDiameterCm, actual.tubeDiameterCm)) {
        if (error.isEmpty()) {
            error = QStringLiteral("KFD round-trip metadata mismatch.");
        }
        errorStream << error << '\n';
        return 6;
    }

    if (!KFilterDriverIo::mergeMeasurementsEnabledAfterImport(false, false, true) ||
        KFilterDriverIo::mergeMeasurementsEnabledAfterImport(true, false, false) ||
        !KFilterDriverIo::mergeMeasurementsEnabledAfterImport(true, true, false) ||
        KFilterDriverIo::mergeMeasurementsEnabledAfterImport(false, true, true)) {
        errorStream << "Measurement merge import policy mismatch.\n";
        return 7;
    }

    KFilterDoc emptyProject;
    if (!KFilterDriverIo::applyDriverSlotToDocument(emptyProject, 0, actual) ||
        !emptyProject.measurementMergeEnabled() ||
        !emptyProject.measurementHiddenForDriver(0) ||
        emptyProject.splCorrectionCurve(0).size() != actual.measurementCurve.size() ||
        emptyProject.activeFilterChain(0).sectionCount() != actual.activeFilterChain.sectionCount() ||
        !compareBaffle(actual.baffleSettings, emptyProject.baffleSettings(0), error) ||
        !compareFloor(actual.floorReflectionSettings, emptyProject.floorReflectionSettings(0), error)) {
        errorStream << "Complete driver-slot application to an empty project failed: "
                    << error << '\n';
        return 8;
    }

    KFilterDoc establishedProject;
    establishedProject.splCorrectionCurve(1).appendPoint(100.0, 0.0);
    establishedProject.splCorrectionCurve(1).appendPoint(1000.0, 1.0);
    establishedProject.setMeasurementMergeEnabled(false);
    KFilterDriverIo::DriverSlot mergeOnSlot = actual;
    mergeOnSlot.mergeMeasurementsEnabled = true;
    if (!KFilterDriverIo::applyDriverSlotToDocument(establishedProject, 0, mergeOnSlot) ||
        establishedProject.measurementMergeEnabled()) {
        errorStream << "Existing project Merge=OFF was overwritten by imported KFD state.\n";
        return 9;
    }

    establishedProject.setMeasurementMergeEnabled(true);
    KFilterDriverIo::DriverSlot mergeOffSlot = actual;
    mergeOffSlot.mergeMeasurementsEnabled = false;
    if (!KFilterDriverIo::applyDriverSlotToDocument(establishedProject, 0, mergeOffSlot) ||
        !establishedProject.measurementMergeEnabled()) {
        errorStream << "Existing project Merge=ON was overwritten by imported KFD state.\n";
        return 10;
    }
    if (KFilterDriverIo::applyDriverSlotToDocument(establishedProject, -1, actual) ||
        KFilterDriverIo::applyDriverSlotToDocument(
            establishedProject, KFilterProjectIo::DriverCount, actual)) {
        errorStream << "Invalid driver-slot target index was accepted.\n";
        return 11;
    }

    QJsonObject legacyRoot = root;
    legacyRoot.insert(QStringLiteral("formatVersion"), 1);
    const QString legacyPath = temporaryDirectory.filePath(QStringLiteral("legacy-v1.kfd"));
    QFile legacyFile(legacyPath);
    if (!legacyFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        legacyFile.write(QJsonDocument(legacyRoot).toJson()) <= 0) {
        errorStream << "Cannot create KFD v1 rejection fixture.\n";
        return 12;
    }
    legacyFile.close();

    KFilterDriverIo::DriverSlot legacyTarget;
    error.clear();
    if (KFilterDriverIo::loadDriverSlotFromFile(legacyPath, legacyTarget, &error) ||
        !error.contains(QStringLiteral("version 1"))) {
        errorStream << "KFD v1 was not rejected as intended.\n";
        return 13;
    }

    return 0;
}
