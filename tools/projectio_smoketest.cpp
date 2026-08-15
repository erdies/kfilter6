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

void populateActiveFilters(KFilterProjectIo::ActiveFilterChains& chains)
{
    ActiveFilterChain& first = chains[0];
    first.setEnabled(true);
    first.setShowResponseInPlot(true);
    first.addSection(ActiveFilterType::LowPass);
    auto& lowPass = std::get<ActiveFilterLowPassParameters>(first.section(0).parameters());
    lowPass.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
    lowPass.order = 4;
    lowPass.frequencyHz = 2350.5;
    lowPass.q = 0.8123;
    first.addSection(ActiveFilterType::BandPass);
    first.section(1).setEnabled(false);
    auto& bandPass = std::get<ActiveFilterBandPassParameters>(first.section(1).parameters());
    bandPass.characteristic = ActiveFilterCharacteristic::Bessel;
    bandPass.order = 3;
    bandPass.lowerFrequencyHz = 450.25;
    bandPass.upperFrequencyHz = 3450.75;
    bandPass.q = 1.2345;

    ActiveFilterChain& second = chains[1];
    second.setEnabled(true);
    second.addSection(ActiveFilterType::HighPass);
    auto& highPass = std::get<ActiveFilterHighPassParameters>(second.section(0).parameters());
    highPass.characteristic = ActiveFilterCharacteristic::Butterworth;
    highPass.order = 7;
    highPass.frequencyHz = 87.125;
    highPass.q = 0.6543;
    second.addSection(ActiveFilterType::Notch);
    auto& notch = std::get<ActiveFilterNotchParameters>(second.section(1).parameters());
    notch.centerFrequencyHz = 1234.5;
    notch.q = 4.25;
    notch.gainDb = -8.75;
    second.addSection(ActiveFilterType::PeakingEq);
    auto& peakingEq =
        std::get<ActiveFilterPeakingEqParameters>(second.section(2).parameters());
    peakingEq.centerFrequencyHz = 1789.25;
    peakingEq.q = 2.75;
    peakingEq.gainDb = 5.625;
    second.addSection(ActiveFilterType::LowShelf);
    auto& lowShelf =
        std::get<ActiveFilterLowShelfParameters>(second.section(3).parameters());
    lowShelf.transitionFrequencyHz = 220.75;
    lowShelf.q = 0.82;
    lowShelf.gainDb = 4.25;
    second.addSection(ActiveFilterType::HighShelf);
    auto& highShelf =
        std::get<ActiveFilterHighShelfParameters>(second.section(4).parameters());
    highShelf.transitionFrequencyHz = 8750.5;
    highShelf.q = 0.63;
    highShelf.gainDb = -2.875;

    ActiveFilterChain& third = chains[2];
    third.setShowResponseInPlot(true);
    third.addSection(ActiveFilterType::AllPass);
    auto& allPass = std::get<ActiveFilterAllPassParameters>(third.section(0).parameters());
    allPass.order = 2;
    allPass.frequencyHz = 765.5;
    allPass.q = 0.5432;
    third.addSection(ActiveFilterType::Gain);
    std::get<ActiveFilterGainParameters>(third.section(1).parameters()).gainDb = -3.125;
    third.addSection(ActiveFilterType::Delay);
    std::get<ActiveFilterDelayParameters>(third.section(2).parameters()).delayMs = 0.375;
    third.addSection(ActiveFilterType::Polarity);
    std::get<ActiveFilterPolarityParameters>(third.section(3).parameters()).inverted = true;

    ActiveFilterChain& fourth = chains[3];
    fourth.setEnabled(true);
    fourth.addSection(ActiveFilterType::HighPass);
    auto& genericHighPass = std::get<ActiveFilterHighPassParameters>(fourth.section(0).parameters());
    genericHighPass.characteristic = ActiveFilterCharacteristic::GenericQ;
    genericHighPass.order = 2;
    genericHighPass.frequencyHz = 31.75;
    genericHighPass.q = 0.91;
}

bool compareActiveFilterSections(const ActiveFilterSection& expected,
                                 const ActiveFilterSection& actual)
{
    if (expected.enabled() != actual.enabled() || expected.type() != actual.type()) {
        return false;
    }

    switch (expected.type()) {
    case ActiveFilterType::LowPass: {
        const auto& left = std::get<ActiveFilterLowPassParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterLowPassParameters>(actual.parameters());
        return left.characteristic == right.characteristic && left.order == right.order &&
               fuzzyEqual(left.frequencyHz, right.frequencyHz) && fuzzyEqual(left.q, right.q);
    }
    case ActiveFilterType::HighPass: {
        const auto& left = std::get<ActiveFilterHighPassParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterHighPassParameters>(actual.parameters());
        return left.characteristic == right.characteristic && left.order == right.order &&
               fuzzyEqual(left.frequencyHz, right.frequencyHz) && fuzzyEqual(left.q, right.q);
    }
    case ActiveFilterType::BandPass: {
        const auto& left = std::get<ActiveFilterBandPassParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterBandPassParameters>(actual.parameters());
        return left.characteristic == right.characteristic && left.order == right.order &&
               fuzzyEqual(left.lowerFrequencyHz, right.lowerFrequencyHz) &&
               fuzzyEqual(left.upperFrequencyHz, right.upperFrequencyHz) &&
               fuzzyEqual(left.q, right.q);
    }
    case ActiveFilterType::Notch: {
        const auto& left = std::get<ActiveFilterNotchParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterNotchParameters>(actual.parameters());
        return fuzzyEqual(left.centerFrequencyHz, right.centerFrequencyHz) &&
               fuzzyEqual(left.q, right.q) && fuzzyEqual(left.gainDb, right.gainDb);
    }
    case ActiveFilterType::PeakingEq: {
        const auto& left = std::get<ActiveFilterPeakingEqParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterPeakingEqParameters>(actual.parameters());
        return fuzzyEqual(left.centerFrequencyHz, right.centerFrequencyHz) &&
               fuzzyEqual(left.q, right.q) && fuzzyEqual(left.gainDb, right.gainDb);
    }
    case ActiveFilterType::LowShelf: {
        const auto& left = std::get<ActiveFilterLowShelfParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterLowShelfParameters>(actual.parameters());
        return fuzzyEqual(left.transitionFrequencyHz, right.transitionFrequencyHz) &&
               fuzzyEqual(left.q, right.q) && fuzzyEqual(left.gainDb, right.gainDb);
    }
    case ActiveFilterType::HighShelf: {
        const auto& left = std::get<ActiveFilterHighShelfParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterHighShelfParameters>(actual.parameters());
        return fuzzyEqual(left.transitionFrequencyHz, right.transitionFrequencyHz) &&
               fuzzyEqual(left.q, right.q) && fuzzyEqual(left.gainDb, right.gainDb);
    }
    case ActiveFilterType::AllPass: {
        const auto& left = std::get<ActiveFilterAllPassParameters>(expected.parameters());
        const auto& right = std::get<ActiveFilterAllPassParameters>(actual.parameters());
        return left.order == right.order && fuzzyEqual(left.frequencyHz, right.frequencyHz) &&
               fuzzyEqual(left.q, right.q);
    }
    case ActiveFilterType::Gain:
        return fuzzyEqual(std::get<ActiveFilterGainParameters>(expected.parameters()).gainDb,
                          std::get<ActiveFilterGainParameters>(actual.parameters()).gainDb);
    case ActiveFilterType::Delay:
        return fuzzyEqual(std::get<ActiveFilterDelayParameters>(expected.parameters()).delayMs,
                          std::get<ActiveFilterDelayParameters>(actual.parameters()).delayMs);
    case ActiveFilterType::Polarity:
        return std::get<ActiveFilterPolarityParameters>(expected.parameters()).inverted ==
               std::get<ActiveFilterPolarityParameters>(actual.parameters()).inverted;
    }

    return false;
}

bool compareActiveFilters(const KFilterProjectIo::ActiveFilterChains& expected,
                          const KFilterProjectIo::ActiveFilterChains& actual,
                          QString& error)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        const std::size_t index = static_cast<std::size_t>(driverIndex);
        const ActiveFilterChain& expectedChain = expected[index];
        const ActiveFilterChain& actualChain = actual[index];
        if (expectedChain.enabled() != actualChain.enabled() ||
            expectedChain.showResponseInPlot() != actualChain.showResponseInPlot() ||
            expectedChain.sectionCount() != actualChain.sectionCount()) {
            error = QStringLiteral("Active-filter chain metadata mismatch for driver %1")
                        .arg(driverIndex + 1);
            return false;
        }

        for (std::size_t sectionIndex = 0; sectionIndex < expectedChain.sectionCount(); ++sectionIndex) {
            if (!compareActiveFilterSections(expectedChain.section(sectionIndex),
                                             actualChain.section(sectionIndex))) {
                error = QStringLiteral("Active-filter section mismatch for driver %1, section %2")
                            .arg(driverIndex + 1)
                            .arg(static_cast<qulonglong>(sectionIndex + 1));
                return false;
            }
        }
    }
    return true;
}

bool activeFiltersAreDefault(const KFilterProjectIo::ActiveFilterChains& chains)
{
    return std::all_of(chains.cbegin(), chains.cend(), [](const ActiveFilterChain& chain) {
        return !chain.enabled() && !chain.showResponseInPlot() && chain.empty();
    });
}

void populateBaffleSettings(KFilterProjectIo::BaffleSettingsPerDriver& settings)
{
    settings[0].enabled = true;
    settings[0].model = BaffleModel::SimpleBaffleStep;
    settings[0].widthMm = 231.0;
    settings[0].showResponseInPlot = true;

    settings[1].enabled = false;
    settings[1].model = BaffleModel::SimpleBaffleStep;
    settings[1].widthMm = 180.5;
    settings[1].showResponseInPlot = true;

    settings[2].enabled = true;
    settings[2].model = BaffleModel::RectangularEdgeDiffraction;
    settings[2].widthMm = 260.0;
    settings[2].heightMm = 720.0;
    settings[2].driverXmm = 110.0;
    settings[2].driverYmm = 245.0;
    settings[2].showResponseInPlot = false;
    settings[2].edgeSourceCount = 320;
    settings[2].leftEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    settings[2].leftChamferSetbackMm = 25.0;
    settings[2].rightEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    settings[2].rightChamferSetbackMm = 30.0;

    settings[3].enabled = true;
    settings[3].model = BaffleModel::RectangularEdgeDiffraction;
    settings[3].widthMm = 231.0;
    settings[3].heightMm = 965.0;
    settings[3].driverXmm = 115.5;
    settings[3].driverYmm = 868.5;
    settings[3].boundaryCondition =
        BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    settings[3].edgeSourceCount = 200;
}

bool compareBaffleSettings(const KFilterProjectIo::BaffleSettingsPerDriver& expected,
                           const KFilterProjectIo::BaffleSettingsPerDriver& actual,
                           QString& error)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        const std::size_t index = static_cast<std::size_t>(driverIndex);
        const BaffleSettings& left = expected[index];
        const BaffleSettings& right = actual[index];
        if (left.enabled != right.enabled || left.model != right.model ||
            !fuzzyEqual(left.widthMm, right.widthMm) ||
            !fuzzyEqual(left.heightMm, right.heightMm) ||
            !fuzzyEqual(left.driverXmm, right.driverXmm) ||
            !fuzzyEqual(left.driverYmm, right.driverYmm) ||
            left.boundaryCondition != right.boundaryCondition ||
            left.showResponseInPlot != right.showResponseInPlot ||
            left.edgeSourceCount != right.edgeSourceCount ||
            left.leftEdgeTreatment != right.leftEdgeTreatment ||
            !fuzzyEqual(left.leftChamferSetbackMm, right.leftChamferSetbackMm) ||
            left.rightEdgeTreatment != right.rightEdgeTreatment ||
            !fuzzyEqual(left.rightChamferSetbackMm, right.rightChamferSetbackMm)) {
            error = QStringLiteral("Baffle settings mismatch for driver %1").arg(driverIndex + 1);
            return false;
        }
    }
    return true;
}

bool baffleSettingsAreDefault(const KFilterProjectIo::BaffleSettingsPerDriver& settings)
{
    const BaffleSettings defaults;
    return std::all_of(settings.cbegin(), settings.cend(), [&](const BaffleSettings& value) {
        return value.enabled == defaults.enabled && value.model == defaults.model &&
               fuzzyEqual(value.widthMm, defaults.widthMm) &&
               fuzzyEqual(value.heightMm, defaults.heightMm) &&
               fuzzyEqual(value.driverXmm, defaults.driverXmm) &&
               fuzzyEqual(value.driverYmm, defaults.driverYmm) &&
               value.boundaryCondition == defaults.boundaryCondition &&
               value.showResponseInPlot == defaults.showResponseInPlot &&
               value.edgeSourceCount == defaults.edgeSourceCount &&
               value.leftEdgeTreatment == defaults.leftEdgeTreatment &&
               fuzzyEqual(value.leftChamferSetbackMm, defaults.leftChamferSetbackMm) &&
               value.rightEdgeTreatment == defaults.rightEdgeTreatment &&
               fuzzyEqual(value.rightChamferSetbackMm, defaults.rightChamferSetbackMm);
    });
}

void populateFloorReflectionSettings(
    KFilterProjectIo::FloorReflectionSettingsPerDriver& settings)
{
    settings[0].enabled = true;
    settings[0].cabinetBottomAboveFloorMm = 120.0;
    settings[0].listenerHeightAboveFloorMm = 1075.5;
    settings[0].horizontalDistanceMm = 2450.25;
    settings[0].surfacePreset = FloorSurfacePreset::HardRigid;

    settings[1].enabled = false;
    settings[1].cabinetBottomAboveFloorMm = 310.0;
    settings[1].listenerHeightAboveFloorMm = 980.0;
    settings[1].horizontalDistanceMm = 1800.0;

    settings[2].enabled = true;
    settings[2].cabinetBottomAboveFloorMm = 0.0;
    settings[2].listenerHeightAboveFloorMm = 1150.0;
    settings[2].horizontalDistanceMm = 3200.0;
    settings[2].surfacePreset = FloorSurfacePreset::MikiReference10mm100k;

    settings[3].enabled = true;
    settings[3].cabinetBottomAboveFloorMm = 45.0;
    settings[3].listenerHeightAboveFloorMm = 1030.0;
    settings[3].horizontalDistanceMm = 2750.0;
}

bool compareFloorReflectionSettings(
    const KFilterProjectIo::FloorReflectionSettingsPerDriver& expected,
    const KFilterProjectIo::FloorReflectionSettingsPerDriver& actual,
    QString& error)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        const std::size_t index = static_cast<std::size_t>(driverIndex);
        const FloorReflectionSettings& left = expected[index];
        const FloorReflectionSettings& right = actual[index];
        if (left.enabled != right.enabled ||
            !fuzzyEqual(left.cabinetBottomAboveFloorMm, right.cabinetBottomAboveFloorMm) ||
            !fuzzyEqual(left.listenerHeightAboveFloorMm, right.listenerHeightAboveFloorMm) ||
            !fuzzyEqual(left.horizontalDistanceMm, right.horizontalDistanceMm) ||
            left.surfacePreset != right.surfacePreset) {
            error = QStringLiteral("Floor-reflection settings mismatch for driver %1")
                        .arg(driverIndex + 1);
            return false;
        }
    }
    return true;
}

bool floorReflectionSettingsAreDefault(
    const KFilterProjectIo::FloorReflectionSettingsPerDriver& settings)
{
    const FloorReflectionSettings defaults;
    return std::all_of(settings.cbegin(), settings.cend(), [&](const FloorReflectionSettings& value) {
        return value.enabled == defaults.enabled &&
               fuzzyEqual(value.cabinetBottomAboveFloorMm, defaults.cabinetBottomAboveFloorMm) &&
               fuzzyEqual(value.listenerHeightAboveFloorMm, defaults.listenerHeightAboveFloorMm) &&
               fuzzyEqual(value.horizontalDistanceMm, defaults.horizontalDistanceMm) &&
               value.surfacePreset == defaults.surfacePreset;
    });
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
    KFilterProjectIo::ActiveFilterChains originalActiveFilters{};
    populateActiveFilters(originalActiveFilters);
    KFilterProjectIo::BaffleSettingsPerDriver originalBaffleSettings{};
    populateBaffleSettings(originalBaffleSettings);
    KFilterProjectIo::FloorReflectionSettingsPerDriver originalFloorReflectionSettings{};
    populateFloorReflectionSettings(originalFloorReflectionSettings);
    KFilterProjectIo::FloorReflectionSettingsPerDriver scratchFloorReflectionSettings{};

    if (!KFilterProjectIo::saveToFile(jsonFilePath,
                                      original,
                                      originalMeasurements,
                                      true,
                                      originalHiddenStates,
                                      originalActiveFilters,
                                      originalBaffleSettings,
                                      originalFloorReflectionSettings,
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
    const QJsonObject firstActiveFilter =
        jsonDrivers.at(0).toObject().value(QStringLiteral("activeFilter")).toObject();
    const QJsonArray firstActiveSections =
        firstActiveFilter.value(QStringLiteral("sections")).toArray();
    const QJsonObject firstLowPass = firstActiveSections.at(0).toObject();
    const QJsonObject firstLowPassParameters =
        firstLowPass.value(QStringLiteral("parameters")).toObject();
    const QJsonObject firstBaffle =
        jsonDrivers.at(0).toObject().value(QStringLiteral("baffle")).toObject();
    const QJsonObject thirdBaffle =
        jsonDrivers.at(2).toObject().value(QStringLiteral("baffle")).toObject();
    const QJsonObject fourthBaffle =
        jsonDrivers.at(3).toObject().value(QStringLiteral("baffle")).toObject();
    const QJsonObject firstFloorReflection =
        jsonDrivers.at(0).toObject().value(QStringLiteral("floorReflection")).toObject();
    const QJsonObject secondFloorReflection =
        jsonDrivers.at(1).toObject().value(QStringLiteral("floorReflection")).toObject();
    const QJsonObject thirdFloorReflection =
        jsonDrivers.at(2).toObject().value(QStringLiteral("floorReflection")).toObject();
    if (validRoot.value(QStringLiteral("format")).toString() != QStringLiteral("KFilter project") ||
        validRoot.value(QStringLiteral("formatVersion")).toInt(-1) != KFilterProjectIo::JsonFormatVersion ||
        jsonDrivers.size() != KFilterProjectIo::DriverCount ||
        !measurementSettings.value(QStringLiteral("mergeCorrectionCurves")).toBool(false) ||
        measurementSettings.contains(QStringLiteral("hideMeasurements")) ||
        firstCorrection.value(QStringLiteral("hidden")).toBool(false) != true ||
        thirdCorrection.value(QStringLiteral("hidden")).toBool(true) != false ||
        !firstActiveFilter.value(QStringLiteral("enabled")).toBool(false) ||
        !firstActiveFilter.value(QStringLiteral("showResponseInPlot")).toBool(false) ||
        firstActiveSections.size() != 2 ||
        firstLowPass.value(QStringLiteral("type")).toString() != QStringLiteral("lowPass") ||
        firstLowPassParameters.value(QStringLiteral("characteristic")).toString() !=
            QStringLiteral("linkwitzRiley") ||
        firstLowPassParameters.value(QStringLiteral("order")).toInt() != 4 ||
        !fuzzyEqual(firstLowPassParameters.value(QStringLiteral("frequencyHz")).toDouble(), 2350.5) ||
        !fuzzyEqual(firstLowPassParameters.value(QStringLiteral("q")).toDouble(), 0.8123) ||
        !firstBaffle.value(QStringLiteral("enabled")).toBool(false) ||
        firstBaffle.value(QStringLiteral("model")).toString() != QStringLiteral("simpleBaffleStep") ||
        firstBaffle.value(QStringLiteral("boundaryCondition")).toString() != QStringLiteral("freeField") ||
        !fuzzyEqual(firstBaffle.value(QStringLiteral("widthMm")).toDouble(), 231.0) ||
        !firstBaffle.value(QStringLiteral("showResponseInPlot")).toBool(false) ||
        firstBaffle.value(QStringLiteral("edgeSourceCount")).toInt() != 200 ||
        firstBaffle.value(QStringLiteral("leftEdgeTreatment")).toString() != QStringLiteral("sharp") ||
        firstBaffle.value(QStringLiteral("rightEdgeTreatment")).toString() != QStringLiteral("sharp") ||
        !fuzzyEqual(firstBaffle.value(QStringLiteral("leftChamferSetbackMm")).toDouble(), 20.0) ||
        !fuzzyEqual(firstBaffle.value(QStringLiteral("rightChamferSetbackMm")).toDouble(), 20.0) ||
        thirdBaffle.value(QStringLiteral("leftEdgeTreatment")).toString() != QStringLiteral("chamfer45") ||
        thirdBaffle.value(QStringLiteral("rightEdgeTreatment")).toString() != QStringLiteral("chamfer45") ||
        !fuzzyEqual(thirdBaffle.value(QStringLiteral("leftChamferSetbackMm")).toDouble(), 25.0) ||
        !fuzzyEqual(thirdBaffle.value(QStringLiteral("rightChamferSetbackMm")).toDouble(), 30.0) ||
        thirdBaffle.value(QStringLiteral("boundaryCondition")).toString() != QStringLiteral("freeField") ||
        fourthBaffle.value(QStringLiteral("boundaryCondition")).toString() !=
            QStringLiteral("rigidFloorContactDiffractionOnly") ||
        !firstFloorReflection.value(QStringLiteral("enabled")).toBool(false) ||
        !fuzzyEqual(firstFloorReflection.value(QStringLiteral("cabinetBottomAboveFloorMm")).toDouble(), 120.0) ||
        !fuzzyEqual(firstFloorReflection.value(QStringLiteral("listenerHeightAboveFloorMm")).toDouble(), 1075.5) ||
        !fuzzyEqual(firstFloorReflection.value(QStringLiteral("horizontalDistanceMm")).toDouble(), 2450.25) ||
        firstFloorReflection.value(QStringLiteral("surfacePreset")).toString() != QStringLiteral("hardRigid") ||
        secondFloorReflection.value(QStringLiteral("enabled")).toBool(true) ||
        thirdFloorReflection.value(QStringLiteral("surfacePreset")).toString() !=
            QStringLiteral("mikiReference10mm100k")) {
        QTextStream(stderr) << "Saved JSON project metadata, measurements, active filters, baffle/floor settings or driver count is invalid\n";
        return 1;
    }

    driver jsonLoaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves jsonLoadedMeasurements;
    bool jsonLoadedMergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates jsonLoadedHiddenStates{};
    KFilterProjectIo::ActiveFilterChains jsonLoadedActiveFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver jsonLoadedBaffleSettings{};
    KFilterProjectIo::FloorReflectionSettingsPerDriver jsonLoadedFloorReflectionSettings{};
    if (!KFilterProjectIo::loadFromFile(jsonFilePath,
                                        jsonLoaded,
                                        jsonLoadedMeasurements,
                                        jsonLoadedMergeEnabled,
                                        jsonLoadedHiddenStates,
                                        jsonLoadedActiveFilters,
                                        jsonLoadedBaffleSettings,
                                        jsonLoadedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    if (!compareDrivers(original, jsonLoaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, jsonLoadedMeasurements, errorMessage) ||
        !compareMeasurementHiddenStates(originalHiddenStates, jsonLoadedHiddenStates, errorMessage) ||
        !compareActiveFilters(originalActiveFilters, jsonLoadedActiveFilters, errorMessage) ||
        !compareBaffleSettings(originalBaffleSettings, jsonLoadedBaffleSettings, errorMessage) ||
        !compareFloorReflectionSettings(originalFloorReflectionSettings,
                                        jsonLoadedFloorReflectionSettings,
                                        errorMessage) ||
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
    KFilterProjectIo::ActiveFilterChains legacyActiveFilters{};
    populateActiveFilters(legacyActiveFilters);
    KFilterProjectIo::BaffleSettingsPerDriver legacyBaffleSettings{};
    populateBaffleSettings(legacyBaffleSettings);
    KFilterProjectIo::FloorReflectionSettingsPerDriver legacyFloorReflectionSettings{};
    populateFloorReflectionSettings(legacyFloorReflectionSettings);
    if (!KFilterProjectIo::loadFromFile(legacyFilePath,
                                        legacyLoaded,
                                        legacyMeasurements,
                                        legacyMergeEnabled,
                                        legacyHiddenStates,
                                        legacyActiveFilters,
                                        legacyBaffleSettings,
                                        legacyFloorReflectionSettings,
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
                    [](bool hidden) { return hidden; }) ||
        !activeFiltersAreDefault(legacyActiveFilters) ||
        !baffleSettingsAreDefault(legacyBaffleSettings) ||
        !floorReflectionSettingsAreDefault(legacyFloorReflectionSettings)) {
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
                                      legacyActiveFilters,
                                      legacyBaffleSettings,
                                      legacyFloorReflectionSettings,
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
    KFilterProjectIo::ActiveFilterChains oldLegacyActiveFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver oldLegacyBaffleSettings{};
    if (!KFilterProjectIo::loadFromFile(oldLegacyFilePath,
                                        oldLegacyLoaded,
                                        oldLegacyMeasurements,
                                        oldLegacyMergeEnabled,
                                        oldLegacyHiddenStates,
                                        oldLegacyActiveFilters,
                                        oldLegacyBaffleSettings,
                                        scratchFloorReflectionSettings,
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

    if (!activeFiltersAreDefault(oldLegacyActiveFilters) ||
        !baffleSettingsAreDefault(oldLegacyBaffleSettings) ||
        !floorReflectionSettingsAreDefault(scratchFloorReflectionSettings)) {
        QTextStream(stderr) << "Old legacy file did not reset active-filter/Baffle/Floor metadata\n";
        return 1;
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
    KFilterProjectIo::ActiveFilterChains patch155ActiveFilters{};
    populateActiveFilters(patch155ActiveFilters);
    KFilterProjectIo::BaffleSettingsPerDriver patch155BaffleSettings{};
    populateBaffleSettings(patch155BaffleSettings);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        patch155Loaded,
                                        patch155Measurements,
                                        patch155MergeEnabled,
                                        patch155HiddenStates,
                                        patch155ActiveFilters,
                                        patch155BaffleSettings,
                                        scratchFloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, patch155Loaded, true, errorMessage) ||
        !std::all_of(patch155Measurements.cbegin(),
                     patch155Measurements.cend(),
                     [](const KFilterMeasurementCurve& curve) { return curve.isEmpty(); }) ||
        patch155MergeEnabled ||
        std::any_of(patch155HiddenStates.cbegin(), patch155HiddenStates.cend(),
                    [](bool hidden) { return hidden; }) ||
        !activeFiltersAreDefault(patch155ActiveFilters) ||
        !baffleSettingsAreDefault(patch155BaffleSettings) ||
        !floorReflectionSettingsAreDefault(scratchFloorReflectionSettings)) {
        QTextStream(stderr) << "Patch 155 JSON compatibility failed: " << errorMessage << '\n';
        return 1;
    }

    // Format version 9 is the immediate predecessor of Patch 229. It already
    // contains Floor Reflection but could persist only HardRigid. Loading it
    // must preserve all placement metadata and the rigid preset unchanged.
    QJsonObject version9Root = validRoot;
    version9Root.insert(QStringLiteral("formatVersion"), 9);
    QJsonObject version9Project = version9Root.value(QStringLiteral("project")).toObject();
    QJsonArray version9Drivers = version9Project.value(QStringLiteral("drivers")).toArray();
    for (int index = 0; index < version9Drivers.size(); ++index) {
        QJsonObject driverObject = version9Drivers.at(index).toObject();
        QJsonObject floorObject = driverObject.value(QStringLiteral("floorReflection")).toObject();
        floorObject.insert(QStringLiteral("surfacePreset"), QStringLiteral("hardRigid"));
        driverObject.insert(QStringLiteral("floorReflection"), floorObject);
        version9Drivers[index] = driverObject;
    }
    version9Project.insert(QStringLiteral("drivers"), version9Drivers);
    version9Root.insert(QStringLiteral("project"), version9Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version9Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version9Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version9Measurements;
    bool version9MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version9HiddenStates{};
    KFilterProjectIo::ActiveFilterChains version9ActiveFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver version9BaffleSettings{};
    KFilterProjectIo::FloorReflectionSettingsPerDriver version9FloorReflectionSettings{};
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version9Loaded, version9Measurements,
                                        version9MergeEnabled, version9HiddenStates,
                                        version9ActiveFilters, version9BaffleSettings,
                                        version9FloorReflectionSettings, &errorMessage)) {
        QTextStream(stderr) << "Format version 9 compatibility failed: "
                            << errorMessage << '\n';
        return 1;
    }
    for (const FloorReflectionSettings& value : version9FloorReflectionSettings) {
        if (value.surfacePreset != FloorSurfacePreset::HardRigid) {
            QTextStream(stderr) << "Format version 9 surface preset did not remain HardRigid\n";
            return 1;
        }
    }

    // Format version 8 is the immediate predecessor of Patch 225. It contains
    // the complete Baffle boundary metadata but no Floor Reflection object.
    // Every new per-driver FloorReflectionSettings entry must default exactly.
    QJsonObject version8Root = validRoot;
    version8Root.insert(QStringLiteral("formatVersion"), 8);
    QJsonObject version8Project = version8Root.value(QStringLiteral("project")).toObject();
    QJsonArray version8Drivers = version8Project.value(QStringLiteral("drivers")).toArray();
    for (int index = 0; index < version8Drivers.size(); ++index) {
        QJsonObject driverObject = version8Drivers.at(index).toObject();
        driverObject.remove(QStringLiteral("floorReflection"));
        version8Drivers[index] = driverObject;
    }
    version8Project.insert(QStringLiteral("drivers"), version8Drivers);
    version8Root.insert(QStringLiteral("project"), version8Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version8Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version8Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version8Measurements;
    bool version8MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version8HiddenStates{};
    KFilterProjectIo::ActiveFilterChains version8ActiveFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver version8BaffleSettings{};
    KFilterProjectIo::FloorReflectionSettingsPerDriver version8FloorReflectionSettings{};
    populateFloorReflectionSettings(version8FloorReflectionSettings);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version8Loaded,
                                        version8Measurements,
                                        version8MergeEnabled,
                                        version8HiddenStates,
                                        version8ActiveFilters,
                                        version8BaffleSettings,
                                        version8FloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, version8Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version8Measurements, errorMessage) ||
        !compareMeasurementHiddenStates(originalHiddenStates, version8HiddenStates, errorMessage) ||
        !compareActiveFilters(originalActiveFilters, version8ActiveFilters, errorMessage) ||
        !compareBaffleSettings(originalBaffleSettings, version8BaffleSettings, errorMessage) ||
        !floorReflectionSettingsAreDefault(version8FloorReflectionSettings) ||
        !version8MergeEnabled) {
        QTextStream(stderr) << "Format version 8 Floor Reflection compatibility failed: "
                            << errorMessage << '\n';
        return 1;
    }

    // Format version 7 contains side-chamfer persistence but predates the
    // Patch-220 boundary-condition field. All existing Baffle data must load
    // while the new boundary condition defaults to Free field.
    QJsonObject version7Root = validRoot;
    version7Root.insert(QStringLiteral("formatVersion"), 7);
    QJsonObject version7Project = version7Root.value(QStringLiteral("project")).toObject();
    QJsonArray version7Drivers = version7Project.value(QStringLiteral("drivers")).toArray();
    for (int index = 0; index < version7Drivers.size(); ++index) {
        QJsonObject driverObject = version7Drivers.at(index).toObject();
        QJsonObject baffle = driverObject.value(QStringLiteral("baffle")).toObject();
        baffle.remove(QStringLiteral("boundaryCondition"));
        driverObject.insert(QStringLiteral("baffle"), baffle);
        version7Drivers[index] = driverObject;
    }
    version7Project.insert(QStringLiteral("drivers"), version7Drivers);
    version7Root.insert(QStringLiteral("project"), version7Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version7Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version7Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version7Measurements;
    bool version7MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version7HiddenStates{};
    KFilterProjectIo::ActiveFilterChains version7ActiveFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver version7BaffleSettings{};
    KFilterProjectIo::BaffleSettingsPerDriver version7ExpectedBaffle = originalBaffleSettings;
    for (BaffleSettings& settings : version7ExpectedBaffle) {
        settings.boundaryCondition = BaffleBoundaryCondition::FreeField;
    }
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version7Loaded,
                                        version7Measurements,
                                        version7MergeEnabled,
                                        version7HiddenStates,
                                        version7ActiveFilters,
                                        version7BaffleSettings,
                                        scratchFloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, version7Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version7Measurements, errorMessage) ||
        !compareMeasurementHiddenStates(originalHiddenStates, version7HiddenStates, errorMessage) ||
        !compareActiveFilters(originalActiveFilters, version7ActiveFilters, errorMessage) ||
        !compareBaffleSettings(version7ExpectedBaffle, version7BaffleSettings, errorMessage) ||
        !version7MergeEnabled) {
        QTextStream(stderr) << "Format version 7 Free-field Baffle compatibility failed: "
                            << errorMessage << '\n';
        return 1;
    }

    // Format version 6 contains the original sharp rectangular Baffle fields
    // but predates side-edge treatment and boundary-condition persistence. It
    // must load existing geometry while defaulting both additions.
    QJsonObject version6Root = validRoot;
    version6Root.insert(QStringLiteral("formatVersion"), 6);
    QJsonObject version6Project = version6Root.value(QStringLiteral("project")).toObject();
    QJsonArray version6Drivers = version6Project.value(QStringLiteral("drivers")).toArray();
    for (int index = 0; index < version6Drivers.size(); ++index) {
        QJsonObject driverObject = version6Drivers.at(index).toObject();
        QJsonObject baffle = driverObject.value(QStringLiteral("baffle")).toObject();
        baffle.remove(QStringLiteral("leftEdgeTreatment"));
        baffle.remove(QStringLiteral("leftChamferSetbackMm"));
        baffle.remove(QStringLiteral("rightEdgeTreatment"));
        baffle.remove(QStringLiteral("rightChamferSetbackMm"));
        baffle.remove(QStringLiteral("boundaryCondition"));
        driverObject.insert(QStringLiteral("baffle"), baffle);
        version6Drivers[index] = driverObject;
    }
    version6Project.insert(QStringLiteral("drivers"), version6Drivers);
    version6Root.insert(QStringLiteral("project"), version6Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version6Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version6Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version6Measurements;
    bool version6MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version6HiddenStates{};
    KFilterProjectIo::ActiveFilterChains version6ActiveFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver version6BaffleSettings{};
    KFilterProjectIo::BaffleSettingsPerDriver version6ExpectedBaffle = originalBaffleSettings;
    for (BaffleSettings& settings : version6ExpectedBaffle) {
        settings.boundaryCondition = BaffleBoundaryCondition::FreeField;
        settings.leftEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
        settings.leftChamferSetbackMm = BaffleSettings{}.leftChamferSetbackMm;
        settings.rightEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
        settings.rightChamferSetbackMm = BaffleSettings{}.rightChamferSetbackMm;
    }
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version6Loaded,
                                        version6Measurements,
                                        version6MergeEnabled,
                                        version6HiddenStates,
                                        version6ActiveFilters,
                                        version6BaffleSettings,
                                        scratchFloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, version6Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version6Measurements, errorMessage) ||
        !compareMeasurementHiddenStates(originalHiddenStates, version6HiddenStates, errorMessage) ||
        !compareActiveFilters(originalActiveFilters, version6ActiveFilters, errorMessage) ||
        !compareBaffleSettings(version6ExpectedBaffle, version6BaffleSettings, errorMessage) ||
        !version6MergeEnabled) {
        QTextStream(stderr) << "Format version 6 sharp-edge Baffle compatibility failed: "
                            << errorMessage << '\n';
        return 1;
    }

    // Format version 5 contains active-filter persistence but predates Baffle /
    // Diffraction. Active filters must survive while every BaffleSettings entry
    // falls back to the Patch-190 defaults.
    QJsonObject version5Root = validRoot;
    version5Root.insert(QStringLiteral("formatVersion"), 5);
    QJsonObject version5Project = version5Root.value(QStringLiteral("project")).toObject();
    QJsonArray version5Drivers = version5Project.value(QStringLiteral("drivers")).toArray();
    for (int index = 0; index < version5Drivers.size(); ++index) {
        QJsonObject driverObject = version5Drivers.at(index).toObject();
        driverObject.remove(QStringLiteral("baffle"));
        version5Drivers[index] = driverObject;
    }
    version5Project.insert(QStringLiteral("drivers"), version5Drivers);
    version5Root.insert(QStringLiteral("project"), version5Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version5Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version5Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version5Measurements;
    bool version5MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version5HiddenStates{};
    KFilterProjectIo::ActiveFilterChains version5ActiveFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver version5BaffleSettings{};
    populateBaffleSettings(version5BaffleSettings);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version5Loaded,
                                        version5Measurements,
                                        version5MergeEnabled,
                                        version5HiddenStates,
                                        version5ActiveFilters,
                                        version5BaffleSettings,
                                        scratchFloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, version5Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version5Measurements, errorMessage) ||
        !compareMeasurementHiddenStates(originalHiddenStates, version5HiddenStates, errorMessage) ||
        !compareActiveFilters(originalActiveFilters, version5ActiveFilters, errorMessage) ||
        !version5MergeEnabled ||
        !baffleSettingsAreDefault(version5BaffleSettings)) {
        QTextStream(stderr) << "Format version 5 Baffle compatibility failed: "
                            << errorMessage << '\n';
        return 1;
    }

    // Format version 4 predates active-filter persistence. Existing driver,
    // measurement and per-driver hide data must load unchanged while every
    // ActiveFilterChain is reset to its default state.
    QJsonObject version4Root = validRoot;
    version4Root.insert(QStringLiteral("formatVersion"), 4);
    QJsonObject version4Project = version4Root.value(QStringLiteral("project")).toObject();
    QJsonArray version4Drivers = version4Project.value(QStringLiteral("drivers")).toArray();
    for (int index = 0; index < version4Drivers.size(); ++index) {
        QJsonObject driverObject = version4Drivers.at(index).toObject();
        driverObject.remove(QStringLiteral("activeFilter"));
        version4Drivers[index] = driverObject;
    }
    version4Project.insert(QStringLiteral("drivers"), version4Drivers);
    version4Root.insert(QStringLiteral("project"), version4Project);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(version4Root).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver version4Loaded[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves version4Measurements;
    bool version4MergeEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates version4HiddenStates{};
    KFilterProjectIo::ActiveFilterChains version4ActiveFilters{};
    populateActiveFilters(version4ActiveFilters);
    KFilterProjectIo::BaffleSettingsPerDriver version4BaffleSettings{};
    populateBaffleSettings(version4BaffleSettings);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version4Loaded,
                                        version4Measurements,
                                        version4MergeEnabled,
                                        version4HiddenStates,
                                        version4ActiveFilters,
                                        version4BaffleSettings,
                                        scratchFloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, version4Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version4Measurements, errorMessage) ||
        !compareMeasurementHiddenStates(originalHiddenStates, version4HiddenStates, errorMessage) ||
        !version4MergeEnabled ||
        !activeFiltersAreDefault(version4ActiveFilters) ||
        !baffleSettingsAreDefault(version4BaffleSettings)) {
        QTextStream(stderr) << "Format version 4 active-filter compatibility failed: "
                            << errorMessage << '\n';
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
    KFilterProjectIo::ActiveFilterChains version3ActiveFilters{};
    populateActiveFilters(version3ActiveFilters);
    KFilterProjectIo::BaffleSettingsPerDriver version3BaffleSettings{};
    populateBaffleSettings(version3BaffleSettings);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version3Loaded,
                                        version3Measurements,
                                        version3MergeEnabled,
                                        version3HiddenStates,
                                        version3ActiveFilters,
                                        version3BaffleSettings,
                                        scratchFloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, version3Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version3Measurements, errorMessage) ||
        !version3MergeEnabled ||
        !version3HiddenStates[0] || version3HiddenStates[1] ||
        !version3HiddenStates[2] || version3HiddenStates[3] ||
        !activeFiltersAreDefault(version3ActiveFilters) ||
        !baffleSettingsAreDefault(version3BaffleSettings)) {
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
    KFilterProjectIo::ActiveFilterChains version2ActiveFilters{};
    populateActiveFilters(version2ActiveFilters);
    KFilterProjectIo::BaffleSettingsPerDriver version2BaffleSettings{};
    populateBaffleSettings(version2BaffleSettings);
    if (!KFilterProjectIo::loadFromFile(invalidFilePath,
                                        version2Loaded,
                                        version2Measurements,
                                        version2MergeEnabled,
                                        version2HiddenStates,
                                        version2ActiveFilters,
                                        version2BaffleSettings,
                                        scratchFloorReflectionSettings,
                                        &errorMessage) ||
        !compareDrivers(original, version2Loaded, true, errorMessage) ||
        !compareMeasurements(originalMeasurements, version2Measurements, errorMessage) ||
        !version2MergeEnabled ||
        std::any_of(version2HiddenStates.cbegin(), version2HiddenStates.cend(),
                    [](bool hidden) { return hidden; }) ||
        !activeFiltersAreDefault(version2ActiveFilters) ||
        !baffleSettingsAreDefault(version2BaffleSettings)) {
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
    KFilterProjectIo::ActiveFilterChains unchangedActiveFilters{};
    populateActiveFilters(unchangedActiveFilters);
    const KFilterProjectIo::ActiveFilterChains unchangedActiveFiltersExpected = unchangedActiveFilters;
    KFilterProjectIo::BaffleSettingsPerDriver unchangedBaffleSettings{};
    populateBaffleSettings(unchangedBaffleSettings);
    const KFilterProjectIo::BaffleSettingsPerDriver unchangedBaffleSettingsExpected = unchangedBaffleSettings;
    KFilterProjectIo::FloorReflectionSettingsPerDriver unchangedFloorReflectionSettings{};
    populateFloorReflectionSettings(unchangedFloorReflectionSettings);
    const KFilterProjectIo::FloorReflectionSettingsPerDriver unchangedFloorReflectionSettingsExpected =
        unchangedFloorReflectionSettings;
    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                        unchanged,
                                        unchangedMeasurements,
                                        unchangedMergeEnabled,
                                        unchangedHiddenStates,
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Invalid measurement point ordering was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 ||
        !fuzzyEqual(unchangedMeasurements[0].points().constFirst().frequencyHz, 123.0) ||
        !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; }) ||
        !compareActiveFilters(unchangedActiveFiltersExpected, unchangedActiveFilters, errorMessage) ||
        !compareBaffleSettings(unchangedBaffleSettingsExpected, unchangedBaffleSettings, errorMessage) ||
        !compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
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
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Invalid per-driver hidden value was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 ||
        !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; }) ||
        !compareActiveFilters(unchangedActiveFiltersExpected, unchangedActiveFilters, errorMessage) ||
        !compareBaffleSettings(unchangedBaffleSettingsExpected, unchangedBaffleSettings, errorMessage) ||
        !compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
        QTextStream(stderr) << "Failed hide-state load modified the destination project state\n";
        return 1;
    }

    // Unknown active-filter metadata in format 5 must fail transactionally.
    QJsonObject invalidActiveFilterRoot = validRoot;
    QJsonObject invalidActiveFilterProject =
        invalidActiveFilterRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidActiveFilterDrivers =
        invalidActiveFilterProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidActiveFilterDriver = invalidActiveFilterDrivers.at(0).toObject();
    QJsonObject invalidActiveFilter =
        invalidActiveFilterDriver.value(QStringLiteral("activeFilter")).toObject();
    QJsonArray invalidActiveFilterSections =
        invalidActiveFilter.value(QStringLiteral("sections")).toArray();
    QJsonObject invalidActiveFilterSection = invalidActiveFilterSections.at(0).toObject();
    invalidActiveFilterSection.insert(QStringLiteral("type"), QStringLiteral("futureFilter"));
    invalidActiveFilterSections[0] = invalidActiveFilterSection;
    invalidActiveFilter.insert(QStringLiteral("sections"), invalidActiveFilterSections);
    invalidActiveFilterDriver.insert(QStringLiteral("activeFilter"), invalidActiveFilter);
    invalidActiveFilterDrivers[0] = invalidActiveFilterDriver;
    invalidActiveFilterProject.insert(QStringLiteral("drivers"), invalidActiveFilterDrivers);
    invalidActiveFilterRoot.insert(QStringLiteral("project"), invalidActiveFilterProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidActiveFilterRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                        unchanged,
                                        unchangedMeasurements,
                                        unchangedMergeEnabled,
                                        unchangedHiddenStates,
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Unknown active-filter type was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 || !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; }) ||
        !compareActiveFilters(unchangedActiveFiltersExpected, unchangedActiveFilters, errorMessage) ||
        !compareBaffleSettings(unchangedBaffleSettingsExpected, unchangedBaffleSettings, errorMessage) ||
        !compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
        QTextStream(stderr) << "Failed active-filter load modified the destination project state\n";
        return 1;
    }

    // Invalid Baffle metadata in format 8 must fail transactionally.
    QJsonObject invalidBaffleRoot = validRoot;
    QJsonObject invalidBaffleProject = invalidBaffleRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidBaffleDrivers = invalidBaffleProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidBaffleDriver = invalidBaffleDrivers.at(0).toObject();
    QJsonObject invalidBaffle = invalidBaffleDriver.value(QStringLiteral("baffle")).toObject();
    invalidBaffle.insert(QStringLiteral("widthMm"), 0.0);
    invalidBaffleDriver.insert(QStringLiteral("baffle"), invalidBaffle);
    invalidBaffleDrivers[0] = invalidBaffleDriver;
    invalidBaffleProject.insert(QStringLiteral("drivers"), invalidBaffleDrivers);
    invalidBaffleRoot.insert(QStringLiteral("project"), invalidBaffleProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidBaffleRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                        unchanged,
                                        unchangedMeasurements,
                                        unchangedMergeEnabled,
                                        unchangedHiddenStates,
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Invalid Baffle width was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 || !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; }) ||
        !compareActiveFilters(unchangedActiveFiltersExpected, unchangedActiveFilters, errorMessage) ||
        !compareBaffleSettings(unchangedBaffleSettingsExpected, unchangedBaffleSettings, errorMessage) ||
        !compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
        QTextStream(stderr) << "Failed Baffle load modified the destination project state\n";
        return 1;
    }

    // Format 8 retains the Version-7 chamfer validation contract transactionally.
    QJsonObject invalidChamferRoot = validRoot;
    QJsonObject invalidChamferProject = invalidChamferRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidChamferDrivers = invalidChamferProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidChamferDriver = invalidChamferDrivers.at(2).toObject();
    QJsonObject invalidChamfer = invalidChamferDriver.value(QStringLiteral("baffle")).toObject();
    invalidChamfer.insert(QStringLiteral("leftEdgeTreatment"), QStringLiteral("chamfer45"));
    invalidChamfer.insert(QStringLiteral("leftChamferSetbackMm"), 4.0);
    invalidChamferDriver.insert(QStringLiteral("baffle"), invalidChamfer);
    invalidChamferDrivers[2] = invalidChamferDriver;
    invalidChamferProject.insert(QStringLiteral("drivers"), invalidChamferDrivers);
    invalidChamferRoot.insert(QStringLiteral("project"), invalidChamferProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidChamferRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }
    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                        unchanged,
                                        unchangedMeasurements,
                                        unchangedMergeEnabled,
                                        unchangedHiddenStates,
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Sub-5-mm active chamfer was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 || !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; }) ||
        !compareActiveFilters(unchangedActiveFiltersExpected, unchangedActiveFilters, errorMessage) ||
        !compareBaffleSettings(unchangedBaffleSettingsExpected, unchangedBaffleSettings, errorMessage) ||
        !compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
        QTextStream(stderr) << "Failed chamfer metadata load modified the destination project state\n";
        return 1;
    }

    // Patch 220: unknown boundary values and Rigid-floor + Chamfer combinations
    // must fail transactionally.
    QJsonObject invalidBoundaryRoot = validRoot;
    QJsonObject invalidBoundaryProject = invalidBoundaryRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidBoundaryDrivers = invalidBoundaryProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidBoundaryDriver = invalidBoundaryDrivers.at(0).toObject();
    QJsonObject invalidBoundary = invalidBoundaryDriver.value(QStringLiteral("baffle")).toObject();
    invalidBoundary.insert(QStringLiteral("boundaryCondition"), QStringLiteral("unsupportedBoundary"));
    invalidBoundaryDriver.insert(QStringLiteral("baffle"), invalidBoundary);
    invalidBoundaryDrivers[0] = invalidBoundaryDriver;
    invalidBoundaryProject.insert(QStringLiteral("drivers"), invalidBoundaryDrivers);
    invalidBoundaryRoot.insert(QStringLiteral("project"), invalidBoundaryProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidBoundaryRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }
    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                        unchanged,
                                        unchangedMeasurements,
                                        unchangedMergeEnabled,
                                        unchangedHiddenStates,
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Unsupported Baffle boundary condition was accepted\n";
        return 1;
    }

    QJsonObject invalidFloorChamferRoot = validRoot;
    QJsonObject invalidFloorChamferProject =
        invalidFloorChamferRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidFloorChamferDrivers =
        invalidFloorChamferProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidFloorChamferDriver = invalidFloorChamferDrivers.at(2).toObject();
    QJsonObject invalidFloorChamfer =
        invalidFloorChamferDriver.value(QStringLiteral("baffle")).toObject();
    invalidFloorChamfer.insert(QStringLiteral("boundaryCondition"),
                               QStringLiteral("rigidFloorContactDiffractionOnly"));
    invalidFloorChamferDriver.insert(QStringLiteral("baffle"), invalidFloorChamfer);
    invalidFloorChamferDrivers[2] = invalidFloorChamferDriver;
    invalidFloorChamferProject.insert(QStringLiteral("drivers"), invalidFloorChamferDrivers);
    invalidFloorChamferRoot.insert(QStringLiteral("project"), invalidFloorChamferProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidFloorChamferRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }
    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                        unchanged,
                                        unchangedMeasurements,
                                        unchangedMergeEnabled,
                                        unchangedHiddenStates,
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Rigid-floor plus Chamfer Baffle combination was accepted\n";
        return 1;
    }

    // Patch 225: malformed Floor Reflection metadata must fail transactionally.
    QJsonObject invalidFloorReflectionRoot = validRoot;
    QJsonObject invalidFloorReflectionProject =
        invalidFloorReflectionRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidFloorReflectionDrivers =
        invalidFloorReflectionProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidFloorReflectionDriver = invalidFloorReflectionDrivers.at(0).toObject();
    QJsonObject invalidFloorReflection =
        invalidFloorReflectionDriver.value(QStringLiteral("floorReflection")).toObject();
    invalidFloorReflection.insert(QStringLiteral("cabinetBottomAboveFloorMm"), -1.0);
    invalidFloorReflectionDriver.insert(QStringLiteral("floorReflection"), invalidFloorReflection);
    invalidFloorReflectionDrivers[0] = invalidFloorReflectionDriver;
    invalidFloorReflectionProject.insert(QStringLiteral("drivers"), invalidFloorReflectionDrivers);
    invalidFloorReflectionRoot.insert(QStringLiteral("project"), invalidFloorReflectionProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidFloorReflectionRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }
    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                       unchanged, unchangedMeasurements, unchangedMergeEnabled,
                                       unchangedHiddenStates, unchangedActiveFilters,
                                       unchangedBaffleSettings, unchangedFloorReflectionSettings,
                                       &errorMessage)) {
        QTextStream(stderr) << "Negative Floor Reflection placement value was accepted\n";
        return 1;
    }
    if (!compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
        QTextStream(stderr) << "Failed Floor Reflection load modified destination floor metadata\n";
        return 1;
    }

    QJsonObject invalidSurfaceRoot = validRoot;
    QJsonObject invalidSurfaceProject = invalidSurfaceRoot.value(QStringLiteral("project")).toObject();
    QJsonArray invalidSurfaceDrivers = invalidSurfaceProject.value(QStringLiteral("drivers")).toArray();
    QJsonObject invalidSurfaceDriver = invalidSurfaceDrivers.at(0).toObject();
    QJsonObject invalidSurface =
        invalidSurfaceDriver.value(QStringLiteral("floorReflection")).toObject();
    invalidSurface.insert(QStringLiteral("surfacePreset"), QStringLiteral("unsupportedSurface"));
    invalidSurfaceDriver.insert(QStringLiteral("floorReflection"), invalidSurface);
    invalidSurfaceDrivers[0] = invalidSurfaceDriver;
    invalidSurfaceProject.insert(QStringLiteral("drivers"), invalidSurfaceDrivers);
    invalidSurfaceRoot.insert(QStringLiteral("project"), invalidSurfaceProject);
    if (!writeTextFile(invalidFilePath,
                       QString::fromUtf8(QJsonDocument(invalidSurfaceRoot).toJson(QJsonDocument::Indented)),
                       errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }
    if (KFilterProjectIo::loadFromFile(invalidFilePath,
                                       unchanged, unchangedMeasurements, unchangedMergeEnabled,
                                       unchangedHiddenStates, unchangedActiveFilters,
                                       unchangedBaffleSettings, unchangedFloorReflectionSettings,
                                       &errorMessage)) {
        QTextStream(stderr) << "Unsupported Floor Reflection surface preset was accepted\n";
        return 1;
    }
    if (!compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
        QTextStream(stderr) << "Failed surface-preset load modified destination floor metadata\n";
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
                                        unchangedActiveFilters,
                                        unchangedBaffleSettings,
                                        unchangedFloorReflectionSettings,
                                        &errorMessage)) {
        QTextStream(stderr) << "Unsupported JSON project version was accepted\n";
        return 1;
    }
    if (unchanged[0].GetTitle() != QStringLiteral("unchanged sentinel") ||
        unchangedMeasurements[0].size() != 1 ||
        !unchangedMergeEnabled ||
        !std::all_of(unchangedHiddenStates.cbegin(), unchangedHiddenStates.cend(),
                     [](bool hidden) { return hidden; }) ||
        !compareActiveFilters(unchangedActiveFiltersExpected, unchangedActiveFilters, errorMessage) ||
        !compareBaffleSettings(unchangedBaffleSettingsExpected, unchangedBaffleSettings, errorMessage) ||
        !compareFloorReflectionSettings(unchangedFloorReflectionSettingsExpected,
                                        unchangedFloorReflectionSettings,
                                        errorMessage)) {
        QTextStream(stderr) << "Failed JSON load modified the destination project state\n";
        return 1;
    }

    QFile::remove(invalidFilePath);
    QFile::remove(oldLegacyFilePath);
    QFile::remove(legacyFilePath);
    QFile::remove(jsonFilePath);
    QTextStream(stdout) << "KFilterProjectIo JSON measurement, active-filter, baffle, and legacy compatibility smoke test passed\n";
    return 0;
}
