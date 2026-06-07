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
#include <QString>
#include <QTextStream>

namespace
{
bool fuzzyEqual(double left, double right)
{
    const double diff = left - right;
    return diff > -0.000001 && diff < 0.000001;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    driver original[KFilterProjectIo::DriverCount];
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& d = original[driverIndex];
        d.SetTitle(QStringLiteral("Driver %1").arg(driverIndex + 1));
        d.setRdc(5.0 + driverIndex);
        d.setLsp(0.75 + driverIndex);
        d.setF0(40.0 + driverIndex);
        d.setQtc(0.7 + driverIndex);
        d.setQes(0.8 + driverIndex);
        d.setQms(4.0 + driverIndex);
        d.setVas(55.0 + driverIndex);
        d.setDm(0.18 + driverIndex);
        d.Vb = 20.0 + driverIndex;
        d.setQl(6.5 + driverIndex);
        d.Fb = 42.0 + driverIndex;
        d.V2 = 12.0 + driverIndex;
        d.GTypProposal = driverIndex;
        d.gain = 1.5 + driverIndex;
        d.PressureisActive = (driverIndex % 2) == 0;
        d.ImpedanzisActive = true;
        d.SummaryisActive = false;
        d.ScalarSummaryisActive = true;
        d.ImpedanzSummaryisActive = false;
        d.InvertPhase = (driverIndex % 2) != 0;

        for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
            d.setUnit(unitIndex, driverIndex * 100.0 + unitIndex / 10.0);
        }
    }

    const QString filePath = QDir::temp().filePath(QStringLiteral("kfilter_projectio_smoketest.kfp"));
    QString errorMessage;
    if (!KFilterProjectIo::saveToFile(filePath, original, &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    driver loaded[KFilterProjectIo::DriverCount];
    if (!KFilterProjectIo::loadFromFile(filePath, loaded, &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& expected = original[driverIndex];
        driver& actual = loaded[driverIndex];

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
            expected.InvertPhase != actual.InvertPhase) {
            QTextStream(stderr) << "Round-trip mismatch for driver " << (driverIndex + 1) << '\n';
            return 1;
        }

        for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
            if (!fuzzyEqual(expected.getUnit(unitIndex), actual.getUnit(unitIndex))) {
                QTextStream(stderr) << "Network unit mismatch for driver " << (driverIndex + 1)
                                    << ", unit " << unitIndex << '\n';
                return 1;
            }
        }
    }

    QFile savedFile(filePath);
    if (!savedFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream(stderr) << "Could not reopen saved project file for legacy compatibility test\n";
        return 1;
    }
    const QString savedContent = QString::fromUtf8(savedFile.readAll());
    savedFile.close();

    const qsizetype enclosureLossesSection = savedContent.indexOf(QStringLiteral("[Driver enclosure losses]"));
    if (enclosureLossesSection < 0) {
        QTextStream(stderr) << "Saved project file does not contain the Driver enclosure losses section\n";
        return 1;
    }

    const QString legacyFilePath = QDir::temp().filePath(QStringLiteral("kfilter_projectio_legacy_smoketest.kfp"));
    QFile legacyFile(legacyFilePath);
    if (!legacyFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream(stderr) << "Could not create legacy compatibility project file\n";
        return 1;
    }
    legacyFile.write(savedContent.left(enclosureLossesSection).toUtf8());
    legacyFile.write("\n");
    legacyFile.close();

    driver legacyLoaded[KFilterProjectIo::DriverCount];
    if (!KFilterProjectIo::loadFromFile(legacyFilePath, legacyLoaded, &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 1;
    }

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        if (!fuzzyEqual(legacyLoaded[driverIndex].getQl(), 10.0)) {
            QTextStream(stderr) << "Legacy project Ql default mismatch for driver " << (driverIndex + 1) << '\n';
            return 1;
        }
    }

    QFile::remove(legacyFilePath);
    QFile::remove(filePath);
    QTextStream(stdout) << "KFilterProjectIo round-trip smoke test passed\n";
    return 0;
}
