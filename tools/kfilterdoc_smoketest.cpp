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
    if (!document.measurementMergeEnabled()) {
        QTextStream(stderr) << "Measurement merge could not be enabled for clearing test\n";
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

    if (!document.clearMeasurementCurve(1) || !secondCurve.isEmpty()) {
        QTextStream(stderr) << "Selective measurement clearing did not clear driver 2\n";
        return false;
    }
    if (document.measurementMergeEnabled()) {
        QTextStream(stderr) << "Measurement merge remained enabled without mergeable curves\n";
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

    if (!checkMeasurementSummaryMerge() || !checkSelectiveMeasurementClearing()) {
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

    KFilterDoc loaded;
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
            if (!fuzzyEqual(expectedCurve.points.at(pointIndex).frequencyHz,
                            actualCurve.points.at(pointIndex).frequencyHz) ||
                !fuzzyEqual(expectedCurve.points.at(pointIndex).value,
                            actualCurve.points.at(pointIndex).value)) {
                QTextStream(stderr) << "Document measurement point mismatch for driver "
                                    << (driverIndex + 1) << '\n';
                return 1;
            }
        }
    }

    if (!loaded.measurementMergeEnabled()) {
        QTextStream(stderr) << "Document measurement merge state was not restored\n";
        return 1;
    }

    if (refreshCount != 1) {
        QTextStream(stderr) << "Expected exactly one forceviewrefresh emission, got "
                            << refreshCount << '\n';
        return 1;
    }

    loaded.newDocument();
    if (loaded.hasMeasurementCurves() || loaded.measurementMergeEnabled()) {
        QTextStream(stderr) << "New document did not clear persisted measurement state\n";
        return 1;
    }

    QFile::remove(filePath);
    QTextStream(stdout) << "KFilterDoc document and measurement persistence smoke test passed\n";
    return 0;
}
