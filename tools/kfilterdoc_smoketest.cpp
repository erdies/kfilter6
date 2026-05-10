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

    for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
        d.setUnit(unitIndex, driverIndex * 1000.0 + unitIndex / 20.0);
    }
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
        !fuzzyEqual(expected.Fb, actual.Fb) ||
        !fuzzyEqual(expected.V2, actual.V2) ||
        expected.GTypProposal != actual.GTypProposal ||
        !fuzzyEqual(expected.gain, actual.gain) ||
        expected.PressureisActive != actual.PressureisActive ||
        expected.ImpedanzisActive != actual.ImpedanzisActive ||
        expected.SummaryisActive != actual.SummaryisActive ||
        expected.ScalarSummaryisActive != actual.ScalarSummaryisActive ||
        expected.ImpedanzSummaryisActive != actual.ImpedanzSummaryisActive ||
        expected.InvertPhase != actual.InvertPhase) {
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

    KFilterDoc original;
    int refreshCount = 0;
    QObject::connect(&original, &KFilterDoc::forceviewrefresh, [&refreshCount]() {
        ++refreshCount;
    });

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        fillDriver(original.m_driverDriver[driverIndex], driverIndex);
    }

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
        if (!compareDriver(original.m_driverDriver[driverIndex], loaded.m_driverDriver[driverIndex], driverIndex)) {
            return 1;
        }
    }

    if (refreshCount != 1) {
        QTextStream(stderr) << "Expected exactly one forceviewrefresh emission, got " << refreshCount << '\n';
        return 1;
    }

    QFile::remove(filePath);
    QTextStream(stdout) << "KFilterDoc document smoke test passed\n";
    return 0;
}
