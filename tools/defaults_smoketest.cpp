/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "driver.h"
#include "kfilterdoc.h"

#include <QCoreApplication>
#include <QTextStream>

namespace
{
bool fuzzyEqual(double left, double right)
{
    const double diff = left - right;
    return diff > -0.000001 && diff < 0.000001;
}

bool expectDouble(const QString &prefix, const char *name, double actual, double expected)
{
    if (!fuzzyEqual(actual, expected)) {
        QTextStream(stderr) << prefix << ' ' << name << " mismatch: expected " << expected << ", got " << actual << '\n';
        return false;
    }
    return true;
}

bool expectBool(const QString &prefix, const char *name, bool actual, bool expected)
{
    if (actual != expected) {
        QTextStream(stderr) << prefix << ' ' << name << " mismatch: expected " << expected << ", got " << actual << '\n';
        return false;
    }
    return true;
}

bool expectInt(const QString &prefix, const char *name, int actual, int expected)
{
    if (actual != expected) {
        QTextStream(stderr) << prefix << ' ' << name << " mismatch: expected " << expected << ", got " << actual << '\n';
        return false;
    }
    return true;
}

bool checkHistoricalDefaults(driver &drv, const QString &prefix)
{
    if (drv.getTitle() != QStringLiteral("This is a default driver")) {
        QTextStream(stderr) << prefix << " title mismatch: " << drv.getTitle() << '\n';
        return false;
    }

    bool ok = true;
    ok = expectDouble(prefix, "Rdc", drv.getRdc(), 5.1) && ok;
    ok = expectDouble(prefix, "Lsp", drv.getLsp(), 0.00017) && ok;
    ok = expectDouble(prefix, "F0", drv.getF0(), 307.0) && ok;
    ok = expectDouble(prefix, "Qts", drv.getQtc(), 1.14) && ok;
    ok = expectDouble(prefix, "Qms", drv.getQms(), 1.9) && ok;
    ok = expectDouble(prefix, "Qes", drv.getQes(), 2.87) && ok;
    ok = expectDouble(prefix, "Vas", drv.getVas(), 10.0) && ok;
    ok = expectDouble(prefix, "Dm", drv.getDm(), 7.3) && ok;
    ok = expectDouble(prefix, "gain", drv.gain, 1.0) && ok;
    ok = expectDouble(prefix, "Vb", drv.Vb, 0.0) && ok;
    ok = expectDouble(prefix, "Fb", drv.Fb, 0.0) && ok;
    ok = expectDouble(prefix, "V2", drv.V2, 0.0) && ok;
    ok = expectDouble(prefix, "Ql", drv.getQl(), 10.0) && ok;

    ok = expectInt(prefix, "GTypProposal", static_cast<int>(drv.enclosureTypeProposal), 0) && ok;
    ok = expectBool(prefix, "parameterFlag", drv.parameterFlag, true) && ok;
    ok = expectBool(prefix, "Tiefpass_flag", drv.pistonLowPassActive, false) && ok;
    ok = expectBool(prefix, "Realschall_flag", drv.fullCircuitFlag, false) && ok;

    ok = expectBool(prefix, "pressureIsActive", drv.pressureIsActive, false) && ok;
    ok = expectBool(prefix, "ImpedanzisActive", drv.impedanceIsActive, false) && ok;
    ok = expectBool(prefix, "summaryIsActive", drv.summaryIsActive, false) && ok;
    ok = expectBool(prefix, "ScalarSummaryisActive", drv.ScalarSummaryisActive, false) && ok;
    ok = expectBool(prefix, "ImpedanzSummaryisActive", drv.ImpedanceSummaryisActive, false) && ok;
    ok = expectBool(prefix, "InvertPhase", drv.InvertPhase, false) && ok;
    ok = expectBool(prefix, "Full circuit", drv.getFullCircuit(), false) && ok;

    for (int unitIndex = 0; unitIndex < 49; ++unitIndex) {
        if (!fuzzyEqual(drv.getUnit(unitIndex), 0.0)) {
            QTextStream(stderr) << prefix << " Unit[" << unitIndex << "] mismatch: "
                                << drv.getUnit(unitIndex) << '\n';
            ok = false;
        }
    }

    return ok;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    driver singleDriver;
    if (!checkHistoricalDefaults(singleDriver, QStringLiteral("driver::initContents"))) {
        return 1;
    }

    KFilterDoc document;
    document.newDocument();
    if (document.isModified()) {
        QTextStream(stderr) << "newDocument should not leave the document modified\n";
        return 1;
    }

    for (int index = 0; index < 4; ++index) {
        if (!checkHistoricalDefaults(document.m_driverDriver[index], QStringLiteral("KFilterDoc driver %1").arg(index + 1))) {
            return 1;
        }
    }

    QTextStream(stdout) << "Historical default regression smoke test passed\n";
    return 0;
}
