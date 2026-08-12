/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterdoc.h"
#include "kfilterprojectio.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QUrl>

#include <cmath>
#include <complex>
#include <cstddef>

namespace
{
bool fuzzyEqual(double left, double right)
{
    const double diff = left - right;
    return diff > -0.000001 && diff < 0.000001;
}

void fillDriver(driver& d, int driverIndex)
{
    d.SetTitle(QStringLiteral("Document driver %1").arg(driverIndex + 1));
    d.setRdc(6.0 + driverIndex);
    d.setLsp(0.9 + driverIndex);
    d.setF0(35.0 + driverIndex);
    d.setQtc(0.6 + driverIndex);
    d.setQes(0.7 + driverIndex);
    d.setQms(3.5 + driverIndex);
    d.setVas(45.0 + driverIndex);
    d.setDm(0.16 + driverIndex);
    d.Vb = 18.0 + driverIndex;
    d.setQl(7.0 + driverIndex);
    d.Fb = 38.0 + driverIndex;
    d.V2 = 10.0 + driverIndex;
    d.GTypProposal = driverIndex + 1;
    d.gain = 2.0 + driverIndex;
    d.PressureisActive = (driverIndex % 2) == 0;
    d.ImpedanzisActive = true;
    d.SummaryisActive = (driverIndex == 0);
    d.ScalarSummaryisActive = (driverIndex == 1);
    d.ImpedanzSummaryisActive = (driverIndex == 2);
    d.InvertPhase = (driverIndex % 2) != 0;
    d.setFullCircuit((driverIndex % 2) == 0);

    for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
        d.setUnit(unitIndex, driverIndex * 1000.0 + unitIndex / 20.0);
    }
}

bool expectNear(const char* label, double actual, double expected, double tolerance = 1.0e-6)
{
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        QTextStream(stderr) << label << " mismatch: expected " << expected
                            << ", got " << actual << '\n';
        return false;
    }
    return true;
}

bool checkCentralizedCorrectionCalculation()
{
    constexpr double PressureFrequencyStep = 1.047128548;
    constexpr int InterpolationSampleIndex = 10;
    constexpr double LowerCorrectionDb = 0.0;
    constexpr double UpperCorrectionDb = 8.0;
    constexpr double ExpectedInterpolatedDb = 4.0;

    KFilterDoc document;
    KFilterMeasurementCurve& curve = document.splCorrectionCurve(0);
    const double lowerFrequencyHz =
        20.0 * std::pow(PressureFrequencyStep, InterpolationSampleIndex - 1);
    const double upperFrequencyHz =
        20.0 * std::pow(PressureFrequencyStep, InterpolationSampleIndex + 1);
    curve.appendPoint(lowerFrequencyHz, LowerCorrectionDb);
    curve.appendPoint(upperFrequencyHz, UpperCorrectionDb);

    if (!expectNear("Correction dB with merge disabled",
                    document.splCorrectionDb(0, InterpolationSampleIndex),
                    0.0) ||
        !expectNear("Correction factor with merge disabled",
                    document.splCorrectionAmplitudeFactor(0, InterpolationSampleIndex),
                    1.0)) {
        return false;
    }

    document.setMeasurementHiddenForDriver(0, true);
    if (document.measurementMergeEnabled() || !document.measurementHiddenForDriver(0) ||
        !expectNear("Hidden correction with merge disabled",
                    document.splCorrectionDb(0, InterpolationSampleIndex),
                    0.0)) {
        QTextStream(stderr) << "Hide state could not be enabled independently of merge\n";
        return false;
    }
    document.setMeasurementHiddenForDriver(0, false);

    document.setMeasurementMergeEnabled(true);
    if (!expectNear("Central logarithmic correction interpolation",
                    document.splCorrectionDb(0, InterpolationSampleIndex),
                    ExpectedInterpolatedDb) ||
        !expectNear("Central correction amplitude factor",
                    document.splCorrectionAmplitudeFactor(0, InterpolationSampleIndex),
                    std::pow(10.0, ExpectedInterpolatedDb / 20.0))) {
        return false;
    }

    if (!expectNear("Correction below curve range", document.splCorrectionDb(0, 0), 0.0) ||
        !expectNear("Correction factor below curve range",
                    document.splCorrectionAmplitudeFactor(0, 0),
                    1.0) ||
        !expectNear("Correction for invalid driver",
                    document.splCorrectionDb(-1, InterpolationSampleIndex),
                    0.0) ||
        !expectNear("Correction factor for invalid driver",
                    document.splCorrectionAmplitudeFactor(4, InterpolationSampleIndex),
                    1.0) ||
        !expectNear("Correction for invalid sample", document.splCorrectionDb(0, 150), 0.0) ||
        !expectNear("Correction factor for invalid sample",
                    document.splCorrectionAmplitudeFactor(0, -1),
                    1.0)) {
        return false;
    }

    return true;
}

bool checkCorrectionFastPaths()
{
    KFilterDoc document;
    KFilterMeasurementCurve& curve = document.splCorrectionCurve(0);

    if (document.splCorrectionActiveForDriver(0) ||
        document.splCorrectionActiveForDriver(-1) ||
        document.splCorrectionActiveForDriver(KFilterProjectIo::DriverCount)) {
        QTextStream(stderr) << "Correction fast path accepted an inactive or invalid driver\n";
        return false;
    }

    curve.appendPoint(20.0, 0.0);
    curve.appendPoint(20000.0, 0.0);
    document.setMeasurementMergeEnabled(true);
    if (document.splCorrectionActiveForDriver(0) ||
        !expectNear("Neutral correction factor",
                    document.splCorrectionAmplitudeFactor(0, 75),
                    1.0)) {
        QTextStream(stderr) << "Neutral correction curve did not use the fast path\n";
        return false;
    }

    curve.setPointValue(1, 1.0);
    if (!document.splCorrectionActiveForDriver(0)) {
        QTextStream(stderr) << "Overlapping non-neutral correction was not activated\n";
        return false;
    }

    document.setMeasurementMergeEnabled(false);
    if (document.splCorrectionActiveForDriver(0)) {
        QTextStream(stderr) << "Disabled merge state did not use the fast path\n";
        return false;
    }

    curve.clear();
    curve.appendPoint(1.0, 2.0);
    curve.appendPoint(10.0, 2.0);
    document.setMeasurementMergeEnabled(true);
    if (document.splCorrectionActiveForDriver(0)) {
        QTextStream(stderr) << "Correction below the simulation raster was activated\n";
        return false;
    }

    curve.clear();
    curve.appendPoint(30000.0, 2.0);
    curve.appendPoint(40000.0, 2.0);
    if (document.splCorrectionActiveForDriver(0)) {
        QTextStream(stderr) << "Correction above the simulation raster was activated\n";
        return false;
    }

    curve.clear();
    curve.appendPoint(1000.0, 0.0);
    curve.appendPoint(30000.0, 2.0);
    if (!document.splCorrectionActiveForDriver(0)) {
        QTextStream(stderr) << "Partly overlapping correction was not activated\n";
        return false;
    }

    return true;
}

bool checkCorrectionCacheInvalidation()
{
    constexpr int TestSampleIndex = 75;

    KFilterDoc document;
    KFilterMeasurementCurve& curve = document.splCorrectionCurve(0);
    curve.appendPoint(20.0, 2.0);
    curve.appendPoint(20000.0, 2.0);
    document.setMeasurementMergeEnabled(true);

    if (!expectNear("Initial cached correction",
                    document.splCorrectionDb(0, TestSampleIndex),
                    2.0) ||
        !expectNear("Initial cached correction factor",
                    document.splCorrectionAmplitudeFactor(0, TestSampleIndex),
                    std::pow(10.0, 2.0 / 20.0))) {
        return false;
    }

    if (!curve.setPointValue(0, 6.0) || !curve.setPointValue(1, 6.0) ||
        !expectNear("Point mutation cache invalidation",
                    document.splCorrectionDb(0, TestSampleIndex),
                    6.0) ||
        !expectNear("Point mutation factor cache invalidation",
                    document.splCorrectionAmplitudeFactor(0, TestSampleIndex),
                    std::pow(10.0, 6.0 / 20.0))) {
        return false;
    }

    KFilterMeasurementCurve replacement;
    replacement.appendPoint(20.0, -3.0);
    replacement.appendPoint(20000.0, -3.0);
    curve = replacement;
    if (!expectNear("Curve assignment cache invalidation",
                    document.splCorrectionDb(0, TestSampleIndex),
                    -3.0)) {
        return false;
    }

    document.setMeasurementMergeEnabled(false);
    if (!expectNear("Merge disable cache invalidation",
                    document.splCorrectionDb(0, TestSampleIndex),
                    0.0) ||
        !expectNear("Merge disable factor cache invalidation",
                    document.splCorrectionAmplitudeFactor(0, TestSampleIndex),
                    1.0)) {
        return false;
    }

    document.setMeasurementMergeEnabled(true);
    if (!expectNear("Merge re-enable cache rebuild",
                    document.splCorrectionDb(0, TestSampleIndex),
                    -3.0)) {
        return false;
    }

    document.setMeasurementHiddenForDriver(0, true);
    if (!document.measurementMergeEnabled() || !document.measurementHiddenForDriver(0) ||
        document.splCorrectionActiveForDriver(0) ||
        !expectNear("Hide cache invalidation",
                    document.splCorrectionDb(0, TestSampleIndex),
                    0.0) ||
        !expectNear("Hide factor cache invalidation",
                    document.splCorrectionAmplitudeFactor(0, TestSampleIndex),
                    1.0)) {
        QTextStream(stderr) << "Hide state did not preserve merge while neutralizing correction\n";
        return false;
    }

    document.setMeasurementHiddenForDriver(0, false);
    if (document.measurementHiddenForDriver(0) ||
        !expectNear("Hide disable cache rebuild",
                    document.splCorrectionDb(0, TestSampleIndex),
                    -3.0)) {
        return false;
    }

    curve.clear();
    if (document.splCorrectionActiveForDriver(0) ||
        !expectNear("Curve clear cache invalidation",
                    document.splCorrectionDb(0, TestSampleIndex),
                    0.0)) {
        return false;
    }

    return true;
}

bool checkMeasurementSummaryMerge()
{
    constexpr int TestSampleIndex = 75;
    constexpr double CorrectionDb = 6.0;

    KFilterDoc singleDriverDocument;
    driver& singleDriver = singleDriverDocument.m_driverDriver[0];
    singleDriver.SummaryisActive = true;
    singleDriver.ScalarSummaryisActive = false;

    KFilterMeasurementCurve& singleDriverCurve = singleDriverDocument.splCorrectionCurve(0);
    singleDriverCurve.appendPoint(20.0, CorrectionDb);
    singleDriverCurve.appendPoint(20000.0, CorrectionDb);
    singleDriverDocument.setMeasurementMergeEnabled(true);

    singleDriverDocument.setMeasurementMergeEnabled(false);
    singleDriverDocument.PressureSummary();
    const double vectorBaseline = singleDriverDocument.m_doubleXContainer[0][TestSampleIndex];
    singleDriverDocument.setMeasurementMergeEnabled(true);
    singleDriverDocument.PressureSummary();
    const double vectorCorrected = singleDriverDocument.m_doubleXContainer[0][TestSampleIndex];
    if (!expectNear("Vector summary correction", vectorCorrected - vectorBaseline, CorrectionDb)) {
        return false;
    }

    singleDriverDocument.setMeasurementHiddenForDriver(0, true);
    singleDriverDocument.PressureSummary();
    if (!singleDriverDocument.measurementMergeEnabled() ||
        !expectNear("Hidden vector summary",
                    singleDriverDocument.m_doubleXContainer[0][TestSampleIndex],
                    vectorBaseline)) {
        return false;
    }
    singleDriverDocument.setMeasurementHiddenForDriver(0, false);

    singleDriverCurve.setPointValue(0, 0.0);
    singleDriverCurve.setPointValue(1, 0.0);
    singleDriverDocument.PressureSummary();
    if (!expectNear("Neutral vector summary fast path",
                    singleDriverDocument.m_doubleXContainer[0][TestSampleIndex],
                    vectorBaseline)) {
        return false;
    }

    singleDriverCurve.setPointValue(0, CorrectionDb);
    singleDriverCurve.setPointValue(1, CorrectionDb);
    singleDriver.SummaryisActive = false;
    singleDriver.ScalarSummaryisActive = true;
    singleDriverDocument.setMeasurementMergeEnabled(false);
    singleDriverDocument.PressureScalarSummary();
    const double energeticBaseline = singleDriverDocument.m_doubleXContainer[0][TestSampleIndex];
    singleDriverDocument.setMeasurementMergeEnabled(true);
    singleDriverDocument.PressureScalarSummary();
    const double energeticCorrected = singleDriverDocument.m_doubleXContainer[0][TestSampleIndex];
    if (!expectNear("Energetic summary correction", energeticCorrected - energeticBaseline, CorrectionDb)) {
        return false;
    }

    singleDriverDocument.setMeasurementHiddenForDriver(0, true);
    singleDriverDocument.PressureScalarSummary();
    if (!singleDriverDocument.measurementMergeEnabled() ||
        !expectNear("Hidden energetic summary",
                    singleDriverDocument.m_doubleXContainer[0][TestSampleIndex],
                    energeticBaseline)) {
        return false;
    }
    singleDriverDocument.setMeasurementHiddenForDriver(0, false);

    singleDriverCurve.setPointValue(0, 0.0);
    singleDriverCurve.setPointValue(1, 0.0);
    singleDriverDocument.PressureScalarSummary();
    if (!expectNear("Neutral energetic summary fast path",
                    singleDriverDocument.m_doubleXContainer[0][TestSampleIndex],
                    energeticBaseline)) {
        return false;
    }

    singleDriver.SummaryisActive = true;
    singleDriver.ScalarSummaryisActive = false;
    singleDriverCurve.clear();
    singleDriverCurve.appendPoint(100.0, CorrectionDb);
    singleDriverCurve.appendPoint(1000.0, CorrectionDb);
    singleDriverDocument.setMeasurementMergeEnabled(false);
    singleDriverDocument.PressureSummary();
    const double outsideBaseline = singleDriverDocument.m_doubleXContainer[0][0];
    singleDriverDocument.setMeasurementMergeEnabled(true);
    singleDriverDocument.PressureSummary();
    const double outsideCorrected = singleDriverDocument.m_doubleXContainer[0][0];
    if (!expectNear("Vector summary outside correction range", outsideCorrected, outsideBaseline)) {
        return false;
    }

    KFilterDoc phaseDocument;
    driver& normalDriver = phaseDocument.m_driverDriver[0];
    driver& invertedDriver = phaseDocument.m_driverDriver[1];
    normalDriver.SummaryisActive = true;
    invertedDriver.SummaryisActive = true;
    invertedDriver.InvertPhase = true;
    invertedDriver.setmodified();

    constexpr double PhaseTestCorrectionDb = 12.0;
    KFilterMeasurementCurve& phaseCurve = phaseDocument.splCorrectionCurve(0);
    phaseCurve.appendPoint(20.0, PhaseTestCorrectionDb);
    phaseCurve.appendPoint(20000.0, PhaseTestCorrectionDb);
    phaseDocument.setMeasurementMergeEnabled(true);

    normalDriver.Schall();
    const int resultIndex = TestSampleIndex * 2;
    const double individualMagnitude =
        std::hypot(normalDriver.ResultSchall[resultIndex], normalDriver.ResultSchall[resultIndex + 1]);
    const double amplitudeFactor = std::pow(10.0, PhaseTestCorrectionDb / 20.0);
    const double expectedVectorDb = phaseDocument.DB(individualMagnitude * (amplitudeFactor - 1.0));

    phaseDocument.PressureSummary();
    const double actualVectorDb = phaseDocument.m_doubleXContainer[0][TestSampleIndex];
    if (!expectNear("Vector summary phase preservation", actualVectorDb, expectedVectorDb)) {
        return false;
    }

    return true;
}

bool checkActiveFilterSimulationIntegration()
{
    constexpr std::size_t TestSampleIndex = 75;
    const double cutoffHz = kfilterFrequencyGridHz()[TestSampleIndex];
    const double cutoffDb = 20.0 * std::log10(1.0 / std::sqrt(2.0));

    KFilterDoc document;
    driver& d = document.m_driverDriver[0];
    d.PressureisActive = true;

    if (!document.Sound(0)) {
        QTextStream(stderr) << "Active-filter single-driver baseline could not be calculated\n";
        return false;
    }
    const double baselineDb = document.m_doubleXContainer[0][TestSampleIndex];

    ActiveFilterChain& chain = document.activeFilterChain(0);
    chain.setEnabled(true);
    chain.setShowResponseInPlot(false); // visualization must not gate simulation
    chain.addSection(ActiveFilterType::LowPass);
    auto& lowPass = std::get<ActiveFilterLowPassParameters>(chain.section(0).parameters());
    lowPass.characteristic = ActiveFilterCharacteristic::Butterworth;
    lowPass.order = 1;
    lowPass.frequencyHz = cutoffHz;

    if (document.activeFilterResponse(0).status != ActiveFilterResponseStatus::Valid ||
        !document.Sound(0) ||
        !expectNear("Single-driver active-filter magnitude",
                    document.m_doubleXContainer[0][TestSampleIndex] - baselineDb,
                    cutoffDb,
                    1.0e-5)) {
        return false;
    }
    const double filteredDb = document.m_doubleXContainer[0][TestSampleIndex];

    constexpr double MeasurementCorrectionDb = 6.0;
    KFilterMeasurementCurve& curve = document.splCorrectionCurve(0);
    curve.appendPoint(20.0, MeasurementCorrectionDb);
    curve.appendPoint(20000.0, MeasurementCorrectionDb);
    document.setMeasurementMergeEnabled(true);
    document.Sound(0);
    if (!expectNear("Active-filter plus measurement single-driver path",
                    document.m_doubleXContainer[0][TestSampleIndex] - filteredDb,
                    MeasurementCorrectionDb,
                    1.0e-5)) {
        return false;
    }
    const double effectiveSingleDb = document.m_doubleXContainer[0][TestSampleIndex];

    document.setMeasurementHiddenForDriver(0, true);
    document.Sound(0);
    if (!expectNear("Hide Measurement preserves active-filter effect",
                    document.m_doubleXContainer[0][TestSampleIndex],
                    filteredDb,
                    1.0e-5)) {
        return false;
    }
    document.setMeasurementHiddenForDriver(0, false);
    document.Sound(0);

    d.SummaryisActive = true;
    d.ScalarSummaryisActive = true;
    document.PressureSummary();
    if (!expectNear("Single-driver vector summary uses effective response",
                    document.m_doubleXContainer[0][TestSampleIndex],
                    effectiveSingleDb,
                    1.0e-5)) {
        return false;
    }
    document.PressureScalarSummary();
    if (!expectNear("Single-driver energy summary uses effective response",
                    document.m_doubleXContainer[0][TestSampleIndex],
                    effectiveSingleDb,
                    1.0e-5)) {
        return false;
    }

    // Two identical drivers make the filter phase observable in the vector sum.
    // At fc an LP1 is 0.5 - 0.5j. Together with an unfiltered identical driver
    // the vector magnitude is sqrt(2.5) times one raw driver, while the energy
    // sum is sqrt(1.5) times one raw driver.
    KFilterDoc phaseDocument;
    driver& filteredDriver = phaseDocument.m_driverDriver[0];
    driver& rawDriver = phaseDocument.m_driverDriver[1];
    filteredDriver.SummaryisActive = true;
    rawDriver.SummaryisActive = true;
    filteredDriver.ScalarSummaryisActive = true;
    rawDriver.ScalarSummaryisActive = true;

    rawDriver.Schall();
    const int resultIndex = static_cast<int>(TestSampleIndex) * 2;
    const double rawMagnitude = std::hypot(rawDriver.ResultSchall[resultIndex],
                                           rawDriver.ResultSchall[resultIndex + 1]);

    ActiveFilterChain& phaseChain = phaseDocument.activeFilterChain(0);
    phaseChain.setEnabled(true);
    phaseChain.addSection(ActiveFilterType::LowPass);
    auto& phaseLowPass = std::get<ActiveFilterLowPassParameters>(phaseChain.section(0).parameters());
    phaseLowPass.order = 1;
    phaseLowPass.frequencyHz = cutoffHz;

    phaseDocument.PressureSummary();
    if (!expectNear("Active-filter phase in vector summary",
                    phaseDocument.m_doubleXContainer[0][TestSampleIndex],
                    phaseDocument.DB(rawMagnitude * std::sqrt(2.5)),
                    1.0e-5)) {
        return false;
    }

    phaseDocument.PressureScalarSummary();
    if (!expectNear("Active-filter magnitude in energy summary",
                    phaseDocument.m_doubleXContainer[0][TestSampleIndex],
                    phaseDocument.DB(rawMagnitude * std::sqrt(1.5)),
                    1.0e-5)) {
        return false;
    }

    // Patch 186 Linkwitz-Riley integration: LR4 is -6.0206 dB at the
    // crossover frequency and must use the same centralized complex path.
    KFilterDoc linkwitzRileyDocument;
    linkwitzRileyDocument.m_driverDriver[0].PressureisActive = true;
    linkwitzRileyDocument.Sound(0);
    const double linkwitzRileyBaselineDb =
        linkwitzRileyDocument.m_doubleXContainer[0][TestSampleIndex];
    ActiveFilterChain& linkwitzRileyChain = linkwitzRileyDocument.activeFilterChain(0);
    linkwitzRileyChain.setEnabled(true);
    linkwitzRileyChain.addSection(ActiveFilterType::LowPass);
    auto& linkwitzRileyLowPass =
        std::get<ActiveFilterLowPassParameters>(linkwitzRileyChain.section(0).parameters());
    linkwitzRileyLowPass.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
    linkwitzRileyLowPass.order = 4;
    linkwitzRileyLowPass.frequencyHz = cutoffHz;
    const ActiveFilterResponse& linkwitzRileyResponse =
        linkwitzRileyDocument.activeFilterResponse(0);
    if (linkwitzRileyResponse.status != ActiveFilterResponseStatus::Valid) {
        QTextStream(stderr) << "LR4 active-filter chain was not reported as valid\n";
        return false;
    }
    linkwitzRileyDocument.Sound(0);
    if (!expectNear("LR4 single-driver active-filter magnitude",
                    linkwitzRileyDocument.m_doubleXContainer[0][TestSampleIndex] -
                        linkwitzRileyBaselineDb,
                    20.0 * std::log10(0.5),
                    1.0e-5)) {
        return false;
    }

    // Patch 182 Notch integration: use the grid point immediately below f0 so
    // the expected attenuation is finite and can be compared in dB directly.
    constexpr std::size_t NotchSampleIndex = TestSampleIndex - 1;
    KFilterDoc notchDocument;
    notchDocument.m_driverDriver[0].PressureisActive = true;
    notchDocument.Sound(0);
    const double notchBaselineDb = notchDocument.m_doubleXContainer[0][NotchSampleIndex];
    ActiveFilterChain& notchChain = notchDocument.activeFilterChain(0);
    notchChain.setEnabled(true);
    notchChain.addSection(ActiveFilterType::Notch);
    auto& notchParameters =
        std::get<ActiveFilterNotchParameters>(notchChain.section(0).parameters());
    notchParameters.centerFrequencyHz = cutoffHz;
    notchParameters.q = 4.0;
    const ActiveFilterResponse& notchResponse = notchDocument.activeFilterResponse(0);
    if (notchResponse.status != ActiveFilterResponseStatus::Valid) {
        QTextStream(stderr) << "Notch active-filter chain was not reported as valid\n";
        return false;
    }
    const double notchDeltaDb =
        20.0 * std::log10(std::abs(notchResponse.values[NotchSampleIndex]));
    notchDocument.Sound(0);
    if (!expectNear("Notch single-driver active-filter magnitude",
                    notchDocument.m_doubleXContainer[0][NotchSampleIndex] - notchBaselineDb,
                    notchDeltaDb,
                    1.0e-5)) {
        return false;
    }

    // Patch 183 Band-pass integration: the crossover-style Band-pass is applied
    // through the same centralized complex driver path as LP/HP and Notch.
    KFilterDoc bandPassDocument;
    bandPassDocument.m_driverDriver[0].PressureisActive = true;
    bandPassDocument.Sound(0);
    const double bandPassBaselineDb =
        bandPassDocument.m_doubleXContainer[0][TestSampleIndex];
    ActiveFilterChain& bandPassChain = bandPassDocument.activeFilterChain(0);
    bandPassChain.setEnabled(true);
    bandPassChain.addSection(ActiveFilterType::BandPass);
    auto& bandPassParameters =
        std::get<ActiveFilterBandPassParameters>(bandPassChain.section(0).parameters());
    bandPassParameters.characteristic = ActiveFilterCharacteristic::Butterworth;
    bandPassParameters.order = 2;
    bandPassParameters.lowerFrequencyHz = kfilterFrequencyGridHz()[45];
    bandPassParameters.upperFrequencyHz = kfilterFrequencyGridHz()[105];
    const ActiveFilterResponse& bandPassResponse = bandPassDocument.activeFilterResponse(0);
    if (bandPassResponse.status != ActiveFilterResponseStatus::Valid) {
        QTextStream(stderr) << "Band-pass active-filter chain was not reported as valid\n";
        return false;
    }
    const double bandPassDeltaDb =
        20.0 * std::log10(std::abs(bandPassResponse.values[TestSampleIndex]));
    bandPassDocument.Sound(0);
    if (!expectNear("Band-pass single-driver active-filter magnitude",
                    bandPassDocument.m_doubleXContainer[0][TestSampleIndex] - bandPassBaselineDb,
                    bandPassDeltaDb,
                    1.0e-5)) {
        return false;
    }

    // Patch 187 Gain/Delay/Polarity integration. Two identical drivers make the
    // relative phase directly observable in the vector sum. At the test frequency
    // Gain = -6.0206 dB contributes 0.5, the quarter-cycle Delay contributes -j,
    // and inverted Polarity contributes -1, so the filtered driver factor is +0.5j.
    KFilterDoc elementaryDocument;
    driver& elementaryFilteredDriver = elementaryDocument.m_driverDriver[0];
    driver& elementaryRawDriver = elementaryDocument.m_driverDriver[1];
    elementaryFilteredDriver.SummaryisActive = true;
    elementaryRawDriver.SummaryisActive = true;
    elementaryRawDriver.Schall();
    const double elementaryRawMagnitude =
        std::hypot(elementaryRawDriver.ResultSchall[resultIndex],
                   elementaryRawDriver.ResultSchall[resultIndex + 1]);

    ActiveFilterChain& elementaryChain = elementaryDocument.activeFilterChain(0);
    elementaryChain.setEnabled(true);
    elementaryChain.addSection(ActiveFilterType::Gain);
    std::get<ActiveFilterGainParameters>(elementaryChain.section(0).parameters()).gainDb =
        -6.020599913279624;
    elementaryChain.addSection(ActiveFilterType::Delay);
    std::get<ActiveFilterDelayParameters>(elementaryChain.section(1).parameters()).delayMs =
        250.0 / cutoffHz;
    elementaryChain.addSection(ActiveFilterType::Polarity);
    std::get<ActiveFilterPolarityParameters>(elementaryChain.section(2).parameters()).inverted = true;

    if (elementaryDocument.activeFilterResponse(0).status != ActiveFilterResponseStatus::Valid) {
        QTextStream(stderr) << "Gain/Delay/Polarity active-filter chain was not reported as valid\n";
        return false;
    }
    elementaryDocument.PressureSummary();
    if (!expectNear("Gain/Delay/Polarity vector integration",
                    elementaryDocument.m_doubleXContainer[0][TestSampleIndex],
                    elementaryDocument.DB(elementaryRawMagnitude * std::sqrt(1.25)),
                    1.0e-5)) {
        return false;
    }

    // Unsupported chains must bypass the complete active-filter stage instead
    // of applying only the supported prefix or propagating NaNs into the plot.
    KFilterDoc unsupportedDocument;
    unsupportedDocument.m_driverDriver[0].PressureisActive = true;
    unsupportedDocument.Sound(0);
    const double unsupportedBaseline =
        unsupportedDocument.m_doubleXContainer[0][TestSampleIndex];
    ActiveFilterChain& unsupportedChain = unsupportedDocument.activeFilterChain(0);
    unsupportedChain.setEnabled(true);
    unsupportedChain.addSection(ActiveFilterType::LowPass);
    auto& unsupportedParameters =
        std::get<ActiveFilterLowPassParameters>(unsupportedChain.section(0).parameters());
    unsupportedParameters.characteristic = ActiveFilterCharacteristic::GenericQ;
    unsupportedParameters.order = 4;
    if (unsupportedDocument.activeFilterResponse(0).status !=
            ActiveFilterResponseStatus::Unsupported) {
        QTextStream(stderr) << "Unsupported active-filter chain was not reported\n";
        return false;
    }
    unsupportedDocument.Sound(0);
    if (!expectNear("Unsupported active-filter chain bypass",
                    unsupportedDocument.m_doubleXContainer[0][TestSampleIndex],
                    unsupportedBaseline,
                    1.0e-6)) {
        return false;
    }

    return true;
}

bool checkBaffleSimulationIntegration()
{
    constexpr std::size_t TestSampleIndex = 75;
    const double testFrequencyHz = kfilterFrequencyGridHz()[TestSampleIndex];
    const double alignedWidthMm = 115000.0 / testFrequencyHz;
    const double baffleMidpointDb = 20.0 * std::log10(std::sqrt(2.0));

    KFilterDoc document;
    driver& d = document.m_driverDriver[0];
    d.PressureisActive = true;

    if (!document.Sound(0)) {
        QTextStream(stderr) << "Baffle single-driver baseline could not be calculated\n";
        return false;
    }
    const double baselineDb = document.m_doubleXContainer[0][TestSampleIndex];

    BaffleSettings& settings = document.baffleSettings(0);
    settings.enabled = true;
    settings.model = BaffleModel::SimpleBaffleStep;
    settings.widthMm = alignedWidthMm;
    settings.showResponseInPlot = false; // future visualization must not gate simulation

    const BaffleResponse& response = document.baffleResponse(0);
    if (response.status != BaffleResponseStatus::Valid ||
        !expectNear("Baffle response midpoint magnitude",
                    std::abs(response.values[TestSampleIndex]),
                    std::sqrt(2.0),
                    1.0e-8) ||
        !document.Sound(0) ||
        !expectNear("Single-driver Simple Baffle Step magnitude",
                    document.m_doubleXContainer[0][TestSampleIndex] - baselineDb,
                    baffleMidpointDb,
                    1.0e-5)) {
        return false;
    }
    const double baffledDb = document.m_doubleXContainer[0][TestSampleIndex];

    constexpr double MeasurementCorrectionDb = 6.0;
    KFilterMeasurementCurve& curve = document.splCorrectionCurve(0);
    curve.appendPoint(20.0, MeasurementCorrectionDb);
    curve.appendPoint(20000.0, MeasurementCorrectionDb);
    document.setMeasurementMergeEnabled(true);
    document.Sound(0);
    if (!expectNear("Baffle plus measurement single-driver path",
                    document.m_doubleXContainer[0][TestSampleIndex] - baffledDb,
                    MeasurementCorrectionDb,
                    1.0e-5)) {
        return false;
    }

    document.setMeasurementHiddenForDriver(0, true);
    document.Sound(0);
    if (!expectNear("Hide Measurement preserves baffle effect",
                    document.m_doubleXContainer[0][TestSampleIndex],
                    baffledDb,
                    1.0e-5)) {
        return false;
    }

    // Active Filters and Baffle are independent complex stages. At the common
    // midpoint/cutoff, the +3.0103 dB baffle magnitude and -3.0103 dB LP1
    // magnitude cancel exactly in the individual SPL magnitude.
    ActiveFilterChain& chain = document.activeFilterChain(0);
    chain.setEnabled(true);
    chain.addSection(ActiveFilterType::LowPass);
    auto& lowPass = std::get<ActiveFilterLowPassParameters>(chain.section(0).parameters());
    lowPass.characteristic = ActiveFilterCharacteristic::Butterworth;
    lowPass.order = 1;
    lowPass.frequencyHz = testFrequencyHz;
    document.Sound(0);
    if (!expectNear("Active Filter times Baffle magnitude",
                    document.m_doubleXContainer[0][TestSampleIndex],
                    baselineDb,
                    1.0e-5)) {
        return false;
    }

    // Invalid baffle parameters bypass only H_baffle; the valid active-filter
    // stage must remain effective.
    settings.widthMm = 0.0;
    if (document.baffleResponse(0).status != BaffleResponseStatus::InvalidParameters) {
        QTextStream(stderr) << "Invalid baffle width was not reported\n";
        return false;
    }
    document.Sound(0);
    if (!expectNear("Invalid baffle bypass preserves active filter",
                    document.m_doubleXContainer[0][TestSampleIndex] - baselineDb,
                    -baffleMidpointDb,
                    1.0e-5)) {
        return false;
    }

    // Patch 192: Rectangular Edge Diffraction uses the same centralized complex
    // H_baffle stage. Verify productive Stage-2 magnitude and invalid-geometry bypass.
    KFilterDoc rectangularDocument;
    rectangularDocument.m_driverDriver[0].PressureisActive = true;
    rectangularDocument.Sound(0);
    const double rectangularBaselineDb =
        rectangularDocument.m_doubleXContainer[0][TestSampleIndex];

    BaffleSettings& rectangularSettings = rectangularDocument.baffleSettings(0);
    rectangularSettings.enabled = true;
    rectangularSettings.model = BaffleModel::RectangularEdgeDiffraction;
    rectangularSettings.widthMm = 231.0;
    rectangularSettings.heightMm = 900.0;
    rectangularSettings.driverXmm = 90.0;
    rectangularSettings.driverYmm = 310.0;
    rectangularSettings.edgeSourceCount = 200;

    // Patch 194: Dm remains driver data and is passed transiently into the
    // rectangular Baffle response/cache. Verify both point fallback and finite
    // source without copying Dm into BaffleSettings.
    driver& rectangularDriver = rectangularDocument.m_driverDriver[0];
    rectangularDriver.setDm(0.0);
    const BaffleResponse pointFromDocument = rectangularDocument.baffleResponse(0);
    const BaffleResponse expectedPoint = calculateBaffleResponse(rectangularSettings, 0.0);
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        if (std::abs(pointFromDocument.values[sampleIndex] -
                     expectedPoint.values[sampleIndex]) > 1.0e-12) {
            QTextStream(stderr) << "Document Dm<=0 point-source fallback mismatch\n";
            return false;
        }
    }

    constexpr double RectangularEffectiveDiameterCm = 13.0;
    rectangularDriver.setDm(RectangularEffectiveDiameterCm);
    const BaffleResponse finiteFromDocument = rectangularDocument.baffleResponse(0);
    const BaffleResponse expectedFinite =
        calculateBaffleResponse(rectangularSettings, RectangularEffectiveDiameterCm);
    bool finiteSourceChangedResponse = false;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        if (std::abs(finiteFromDocument.values[sampleIndex] -
                     expectedFinite.values[sampleIndex]) > 1.0e-12) {
            QTextStream(stderr) << "Document finite-piston Dm data path mismatch\n";
            return false;
        }
        if (std::abs(finiteFromDocument.values[sampleIndex] -
                     pointFromDocument.values[sampleIndex]) > 1.0e-5) {
            finiteSourceChangedResponse = true;
        }
    }
    if (!finiteSourceChangedResponse) {
        QTextStream(stderr) << "Document Dm change did not activate finite-piston response\n";
        return false;
    }

    const BaffleResponse& rectangularResponse = rectangularDocument.baffleResponse(0);
    if (rectangularResponse.status != BaffleResponseStatus::Valid) {
        QTextStream(stderr) << "Valid Rectangular Edge Diffraction response was not reported\n";
        return false;
    }
    rectangularDocument.Sound(0);
    const double expectedRectangularDeltaDb =
        20.0 * std::log10(std::abs(rectangularResponse.values[TestSampleIndex]));
    if (!expectNear("Single-driver Rectangular Edge Diffraction magnitude",
                    rectangularDocument.m_doubleXContainer[0][TestSampleIndex] - rectangularBaselineDb,
                    expectedRectangularDeltaDb,
                    1.0e-5)) {
        return false;
    }

    rectangularSettings.driverXmm = 0.0;
    if (rectangularDocument.baffleResponse(0).status != BaffleResponseStatus::InvalidParameters) {
        QTextStream(stderr) << "Invalid rectangular driver position was not reported\n";
        return false;
    }
    rectangularDocument.Sound(0);
    if (!expectNear("Invalid rectangular geometry bypass",
                    rectangularDocument.m_doubleXContainer[0][TestSampleIndex],
                    rectangularBaselineDb,
                    1.0e-5)) {
        return false;
    }

    // A second identical raw driver makes the baffle shelf's phase observable
    // in the vector sum. The energy sum, in contrast, depends only on |H|.
    KFilterDoc phaseDocument;
    driver& baffledDriver = phaseDocument.m_driverDriver[0];
    driver& rawDriver = phaseDocument.m_driverDriver[1];
    baffledDriver.SummaryisActive = true;
    rawDriver.SummaryisActive = true;
    baffledDriver.ScalarSummaryisActive = true;
    rawDriver.ScalarSummaryisActive = true;

    rawDriver.Schall();
    const int resultIndex = static_cast<int>(TestSampleIndex) * 2;
    const double rawMagnitude = std::hypot(rawDriver.ResultSchall[resultIndex],
                                           rawDriver.ResultSchall[resultIndex + 1]);

    BaffleSettings& phaseSettings = phaseDocument.baffleSettings(0);
    phaseSettings.enabled = true;
    phaseSettings.widthMm = alignedWidthMm;
    const BaffleResponse& phaseResponse = phaseDocument.baffleResponse(0);
    if (phaseResponse.status != BaffleResponseStatus::Valid) {
        QTextStream(stderr) << "Baffle phase test response was not valid\n";
        return false;
    }
    const std::complex<double> h = phaseResponse.values[TestSampleIndex];

    phaseDocument.PressureSummary();
    if (!expectNear("Baffle phase in vector summary",
                    phaseDocument.m_doubleXContainer[0][TestSampleIndex],
                    phaseDocument.DB(rawMagnitude * std::abs(std::complex<double>{1.0, 0.0} + h)),
                    1.0e-5)) {
        return false;
    }

    phaseDocument.PressureScalarSummary();
    if (!expectNear("Baffle magnitude in energy summary",
                    phaseDocument.m_doubleXContainer[0][TestSampleIndex],
                    phaseDocument.DB(rawMagnitude * std::sqrt(1.0 + std::norm(h))),
                    1.0e-5)) {
        return false;
    }

    return true;
}

bool checkSelectiveMeasurementClearing()
{
    KFilterDoc document;
    KFilterMeasurementCurve& firstCurve = document.splCorrectionCurve(0);
    firstCurve.appendPoint(100.0, -1.0);
    firstCurve.appendPoint(1000.0, 1.0);

    KFilterMeasurementCurve& secondCurve = document.splCorrectionCurve(1);
    secondCurve.appendPoint(200.0, -2.0);
    secondCurve.appendPoint(2000.0, 2.0);

    document.setMeasurementMergeEnabled(true);
    document.setMeasurementHiddenForDriver(0, true);
    document.setMeasurementHiddenForDriver(1, true);
    if (!document.measurementMergeEnabled() ||
        !document.measurementHiddenForDriver(0) ||
        !document.measurementHiddenForDriver(1)) {
        QTextStream(stderr) << "Per-driver measurement hide could not be enabled for clearing test\n";
        return false;
    }

    if (!document.clearMeasurementCurve(0) || !firstCurve.isEmpty()) {
        QTextStream(stderr) << "Selective measurement clearing did not clear driver 1\n";
        return false;
    }
    if (secondCurve.isEmpty()) {
        QTextStream(stderr) << "Selective measurement clearing changed another driver\n";
        return false;
    }
    if (!document.measurementMergeEnabled()) {
        QTextStream(stderr) << "Measurement merge was disabled while a mergeable curve remained\n";
        return false;
    }
    if (document.measurementHiddenForDriver(0) ||
        !document.measurementHiddenForDriver(1)) {
        QTextStream(stderr) << "Selective clearing did not reset only the cleared driver's hide state\n";
        return false;
    }

    if (!document.clearMeasurementCurve(1) || !secondCurve.isEmpty()) {
        QTextStream(stderr) << "Selective measurement clearing did not clear driver 2\n";
        return false;
    }
    if (document.measurementMergeEnabled()) {
        QTextStream(stderr) << "Measurement merge remained enabled without mergeable curves\n";
        return false;
    }
    if (document.measurementHiddenForDriver(1)) {
        QTextStream(stderr) << "Measurement hide remained enabled without its measurement curve\n";
        return false;
    }
    if (document.clearMeasurementCurve(1)) {
        QTextStream(stderr) << "Clearing an already empty measurement reported a change\n";
        return false;
    }
    if (document.clearMeasurementCurve(-1) ||
        document.clearMeasurementCurve(KFilterProjectIo::DriverCount)) {
        QTextStream(stderr) << "Invalid driver index was accepted for measurement clearing\n";
        return false;
    }
    if (document.setMeasurementHiddenForDriver(0, true) ||
        document.measurementHiddenForDriver(0)) {
        QTextStream(stderr) << "Measurement hide was enabled without a stored measurement\n";
        return false;
    }
    if (document.setMeasurementHiddenForDriver(-1, true) ||
        document.setMeasurementHiddenForDriver(KFilterProjectIo::DriverCount, true) ||
        document.measurementHiddenForDriver(-1) ||
        document.measurementHiddenForDriver(KFilterProjectIo::DriverCount)) {
        QTextStream(stderr) << "Invalid driver index was accepted for measurement hide\n";
        return false;
    }

    return true;
}

bool checkSelectiveMeasurementSums()
{
    constexpr int TestSampleIndex = 75;
    constexpr double FirstCorrectionDb = 6.0;
    constexpr double SecondCorrectionDb = -4.0;

    KFilterDoc actual;
    KFilterDoc expected;
    for (int driverIndex = 0; driverIndex < 2; ++driverIndex) {
        actual.m_driverDriver[driverIndex].SummaryisActive = true;
        actual.m_driverDriver[driverIndex].ScalarSummaryisActive = true;
        expected.m_driverDriver[driverIndex].SummaryisActive = true;
        expected.m_driverDriver[driverIndex].ScalarSummaryisActive = true;
    }

    KFilterMeasurementCurve& actualFirstCurve = actual.splCorrectionCurve(0);
    actualFirstCurve.appendPoint(20.0, FirstCorrectionDb);
    actualFirstCurve.appendPoint(20000.0, FirstCorrectionDb);
    KFilterMeasurementCurve& actualSecondCurve = actual.splCorrectionCurve(1);
    actualSecondCurve.appendPoint(20.0, SecondCorrectionDb);
    actualSecondCurve.appendPoint(20000.0, SecondCorrectionDb);

    KFilterMeasurementCurve& expectedFirstCurve = expected.splCorrectionCurve(0);
    expectedFirstCurve.appendPoint(20.0, FirstCorrectionDb);
    expectedFirstCurve.appendPoint(20000.0, FirstCorrectionDb);

    actual.setMeasurementMergeEnabled(true);
    expected.setMeasurementMergeEnabled(true);
    actual.setMeasurementHiddenForDriver(1, true);

    if (!actual.splCorrectionActiveForDriver(0) ||
        actual.splCorrectionActiveForDriver(1) ||
        !actual.measurementHiddenForDriver(1)) {
        QTextStream(stderr) << "Selective measurement activation state is inconsistent\n";
        return false;
    }

    actual.PressureSummary();
    expected.PressureSummary();
    if (!expectNear("Selective vector summary",
                    actual.m_doubleXContainer[0][TestSampleIndex],
                    expected.m_doubleXContainer[0][TestSampleIndex],
                    1.0e-5)) {
        return false;
    }

    actual.PressureScalarSummary();
    expected.PressureScalarSummary();
    if (!expectNear("Selective energetic summary",
                    actual.m_doubleXContainer[0][TestSampleIndex],
                    expected.m_doubleXContainer[0][TestSampleIndex],
                    1.0e-5)) {
        return false;
    }

    actual.setMeasurementHiddenForDriver(1, false);
    if (!actual.splCorrectionActiveForDriver(1)) {
        QTextStream(stderr) << "Unhiding a driver did not restore its correction\n";
        return false;
    }

    return true;
}

bool compareDriver(driver& expected, driver& actual, int driverIndex)
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
        QTextStream(stderr) << "Document round-trip mismatch for driver " << (driverIndex + 1) << '\n';
        return false;
    }

    for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
        if (!fuzzyEqual(expected.getUnit(unitIndex), actual.getUnit(unitIndex))) {
            QTextStream(stderr) << "Document network unit mismatch for driver " << (driverIndex + 1)
                                << ", unit " << unitIndex << '\n';
            return false;
        }
    }

    return true;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    (void)app;

    if (!checkCentralizedCorrectionCalculation() ||
        !checkCorrectionFastPaths() ||
        !checkCorrectionCacheInvalidation() ||
        !checkMeasurementSummaryMerge() ||
        !checkActiveFilterSimulationIntegration() ||
        !checkBaffleSimulationIntegration() ||
        !checkSelectiveMeasurementClearing() ||
        !checkSelectiveMeasurementSums()) {
        return 1;
    }

    KFilterDoc original;
    int refreshCount = 0;
    QObject::connect(&original, &KFilterDoc::forceviewrefresh, [&refreshCount]() {
        ++refreshCount;
    });

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        fillDriver(original.m_driverDriver[driverIndex], driverIndex);
    }
    original.splCorrectionCurve(0).appendPoint(100.0, -2.0);
    original.splCorrectionCurve(0).appendPoint(1000.0, 1.0);
    original.splCorrectionCurve(3).appendPoint(200.0, 0.5);
    original.splCorrectionCurve(3).appendPoint(5000.0, -1.5);
    original.setMeasurementMergeEnabled(true);
    original.setMeasurementHiddenForDriver(0, true);

    // Patch 181: active-filter metadata is part of the .kfp project format.
    // Patch 187 also makes this persisted Gain section transfer-active.
    original.activeFilterChain(1).setEnabled(true);
    original.activeFilterChain(1).addSection(ActiveFilterType::Gain);
    std::get<ActiveFilterGainParameters>(original.activeFilterChain(1).section(0).parameters()).gainDb = -3.0;


    // The document exposes the same cached complex response for diagnostic
    // plotting and for the supported simulation path.
    original.activeFilterChain(2).setEnabled(true);
    original.activeFilterChain(2).setShowResponseInPlot(true);
    original.activeFilterChain(2).addSection(ActiveFilterType::LowPass);
    auto& diagnosticLowPass = std::get<ActiveFilterLowPassParameters>(
        original.activeFilterChain(2).section(0).parameters());
    diagnosticLowPass.characteristic = ActiveFilterCharacteristic::Butterworth;
    diagnosticLowPass.order = 2;
    diagnosticLowPass.frequencyHz = kfilterFrequencyGridHz()[75];
    const ActiveFilterResponse& diagnosticResponse = original.activeFilterResponse(2);
    if (!diagnosticResponse.plottable() ||
        !fuzzyEqual(std::abs(diagnosticResponse.values[75]), 1.0 / std::sqrt(2.0))) {
        QTextStream(stderr) << "Document active-filter diagnostic response is invalid\n";
        return 1;
    }

    // Patch 191 persists the Stage-1 Baffle settings in .kfp format version 6.
    original.baffleSettings(2).enabled = true;
    original.baffleSettings(2).model = BaffleModel::SimpleBaffleStep;
    original.baffleSettings(2).widthMm = 231.0;
    original.baffleSettings(2).showResponseInPlot = true;

    const QString filePath = QDir::temp().filePath(QStringLiteral("kfilter_doc_smoketest.kfp"));
    const QUrl fileUrl = QUrl::fromLocalFile(filePath);

    if (!original.saveDocument(fileUrl)) {
        QTextStream(stderr) << "KFilterDoc::saveDocument failed\n";
        return 1;
    }

    if (original.isModified()) {
        QTextStream(stderr) << "KFilterDoc should not be modified after saveDocument\n";
        return 1;
    }
    if (!original.activeFilterChain(1).enabled() || original.activeFilterChain(1).sectionCount() != 1 ||
        !original.activeFilterChain(2).enabled() || original.activeFilterChain(2).sectionCount() != 1 ||
        !original.activeFilterChain(2).showResponseInPlot()) {
        QTextStream(stderr) << "saveDocument unexpectedly changed the in-memory active-filter model\n";
        return 1;
    }

    if (!original.baffleSettings(2).enabled ||
        !fuzzyEqual(original.baffleSettings(2).widthMm, 231.0) ||
        !original.baffleSettings(2).showResponseInPlot) {
        QTextStream(stderr) << "saveDocument unexpectedly changed baffle state\n";
        return 1;
    }

    KFilterDoc loaded;
    loaded.activeFilterChain(0).setEnabled(true);
    loaded.activeFilterChain(0).addSection(ActiveFilterType::Delay);
    loaded.baffleSettings(0).enabled = true;
    loaded.baffleSettings(0).widthMm = 999.0;
    QObject::connect(&loaded, &KFilterDoc::forceviewrefresh, [&refreshCount]() {
        ++refreshCount;
    });

    if (!loaded.openDocument(fileUrl)) {
        QTextStream(stderr) << "KFilterDoc::openDocument failed\n";
        return 1;
    }

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        if (!compareDriver(original.m_driverDriver[driverIndex],
                           loaded.m_driverDriver[driverIndex],
                           driverIndex)) {
            return 1;
        }

        const KFilterMeasurementCurve& expectedCurve = original.splCorrectionCurve(driverIndex);
        const KFilterMeasurementCurve& actualCurve = loaded.splCorrectionCurve(driverIndex);
        if (expectedCurve.size() != actualCurve.size()) {
            QTextStream(stderr) << "Document measurement point count mismatch for driver "
                                << (driverIndex + 1) << '\n';
            return 1;
        }
        for (qsizetype pointIndex = 0; pointIndex < expectedCurve.size(); ++pointIndex) {
            if (!fuzzyEqual(expectedCurve.points().at(pointIndex).frequencyHz,
                            actualCurve.points().at(pointIndex).frequencyHz) ||
                !fuzzyEqual(expectedCurve.points().at(pointIndex).value,
                            actualCurve.points().at(pointIndex).value)) {
                QTextStream(stderr) << "Document measurement point mismatch for driver "
                                    << (driverIndex + 1) << '\n';
                return 1;
            }
        }
    }

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        const BaffleSettings& expected = original.baffleSettings(driverIndex);
        const BaffleSettings& actual = loaded.baffleSettings(driverIndex);
        if (expected.enabled != actual.enabled || expected.model != actual.model ||
            !fuzzyEqual(expected.widthMm, actual.widthMm) ||
            !fuzzyEqual(expected.heightMm, actual.heightMm) ||
            !fuzzyEqual(expected.driverXmm, actual.driverXmm) ||
            !fuzzyEqual(expected.driverYmm, actual.driverYmm) ||
            expected.showResponseInPlot != actual.showResponseInPlot ||
            expected.edgeSourceCount != actual.edgeSourceCount) {
            QTextStream(stderr) << "Baffle settings were not restored for driver "
                                << (driverIndex + 1) << '\n';
            return 1;
        }
    }

    if (loaded.activeFilterChain(0).enabled() || !loaded.activeFilterChain(0).empty()) {
        QTextStream(stderr) << "Loading .kfp did not replace stale active-filter state for driver 1\n";
        return 1;
    }

    const ActiveFilterChain& loadedGainChain = loaded.activeFilterChain(1);
    if (!loadedGainChain.enabled() || loadedGainChain.showResponseInPlot() ||
        loadedGainChain.sectionCount() != 1 ||
        loadedGainChain.section(0).type() != ActiveFilterType::Gain ||
        !fuzzyEqual(std::get<ActiveFilterGainParameters>(loadedGainChain.section(0).parameters()).gainDb,
                    -3.0)) {
        QTextStream(stderr) << "Gain active-filter metadata was not restored for driver 2\n";
        return 1;
    }

    const ActiveFilterChain& loadedDiagnosticChain = loaded.activeFilterChain(2);
    if (!loadedDiagnosticChain.enabled() || !loadedDiagnosticChain.showResponseInPlot() ||
        loadedDiagnosticChain.sectionCount() != 1 ||
        loadedDiagnosticChain.section(0).type() != ActiveFilterType::LowPass) {
        QTextStream(stderr) << "Active-filter chain metadata was not restored for driver 3\n";
        return 1;
    }
    const auto& loadedLowPass = std::get<ActiveFilterLowPassParameters>(
        loadedDiagnosticChain.section(0).parameters());
    if (loadedLowPass.characteristic != ActiveFilterCharacteristic::Butterworth ||
        loadedLowPass.order != 2 ||
        !fuzzyEqual(loadedLowPass.frequencyHz, kfilterFrequencyGridHz()[75])) {
        QTextStream(stderr) << "Active-filter section parameters were not restored for driver 3\n";
        return 1;
    }

    if (loaded.activeFilterChain(3).enabled() || !loaded.activeFilterChain(3).empty()) {
        QTextStream(stderr) << "Unexpected active-filter state restored for driver 4\n";
        return 1;
    }

    if (!loaded.measurementMergeEnabled() ||
        !loaded.measurementHiddenForDriver(0) ||
        loaded.measurementHiddenForDriver(3)) {
        QTextStream(stderr) << "Document measurement merge/per-driver hide state was not restored\n";
        return 1;
    }

    if (refreshCount != 1) {
        QTextStream(stderr) << "Expected exactly one forceviewrefresh emission, got "
                            << refreshCount << '\n';
        return 1;
    }

    loaded.activeFilterChain(3).setEnabled(true);
    loaded.activeFilterChain(3).addSection(ActiveFilterType::Notch);
    loaded.baffleSettings(3).enabled = true;
    loaded.baffleSettings(3).widthMm = 450.0;
    loaded.newDocument();
    if (loaded.hasMeasurementCurves() || loaded.measurementMergeEnabled() ||
        loaded.measurementHiddenForDriver(0) || loaded.measurementHiddenForDriver(3) ||
        loaded.activeFilterChain(3).enabled() || !loaded.activeFilterChain(3).empty() ||
        loaded.baffleSettings(3).enabled) {
        QTextStream(stderr) << "New document did not clear measurement/active-filter/baffle state\n";
        return 1;
    }

    QFile::remove(filePath);
    QTextStream(stdout) << "KFilterDoc document, measurement, active-filter, and baffle smoke test passed\n";
    return 0;
}
