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
    ok = expectDouble(prefix, "Qts", drv.getQts(), 1.14) && ok;
    ok = expectDouble(prefix, "Qms", drv.getQms(), 1.9) && ok;
    ok = expectDouble(prefix, "Qes", drv.getQes(), 2.87) && ok;
    ok = expectDouble(prefix, "Vas", drv.getVas(), 10.0) && ok;
    ok = expectDouble(prefix, "Dm", drv.getDm(), 7.3) && ok;
    ok = expectDouble(prefix, "gain", drv.getGainLinear(), 1.0) && ok;
    ok = expectDouble(prefix, "Vb", drv.getVb(), 0.0) && ok;
    ok = expectDouble(prefix, "Fb", drv.getFb(), 0.0) && ok;
    ok = expectDouble(prefix, "V2", drv.getV2(), 0.0) && ok;
    ok = expectDouble(prefix, "Ql", drv.getQl(), 10.0) && ok;

    ok = expectInt(prefix, "GTypProposal", static_cast<int>(drv.getEnclosureTypeProposal()), 0) && ok;

    ok = expectBool(prefix, "pressureIsActive", drv.plotState().pressure, false) && ok;
    ok = expectBool(prefix, "ImpedanzisActive", drv.plotState().impedance, false) && ok;
    ok = expectBool(prefix, "summaryIsActive", drv.plotState().vectorSummary, false) && ok;
    ok = expectBool(prefix, "ScalarSummaryisActive", drv.plotState().scalarSummary, false) && ok;
    ok = expectBool(prefix, "ImpedanzSummaryisActive", drv.plotState().impedanceSummary, false) && ok;
    ok = expectBool(prefix, "InvertPhase", drv.isPhaseInverted(), false) && ok;
    ok = expectBool(prefix, "Full circuit", drv.getFullCircuit(), false) && ok;

    int networkValueIndex = 1;
    for (int sectionIndex = 0; sectionIndex < 8; ++sectionIndex) {
        for (NetworkBranchType branch : {NetworkBranchType::Series, NetworkBranchType::Shunt}) {
            for (NetworkComponent component : {NetworkComponent::Resistance,
                                               NetworkComponent::Capacitance,
                                               NetworkComponent::Inductance}) {
                const double value = drv.getNetworkValue(sectionIndex, branch, component);
                if (!fuzzyEqual(value, 0.0)) {
                    QTextStream(stderr) << prefix << " network value " << networkValueIndex
                                        << " mismatch: " << value << '\n';
                    ok = false;
                }
                ++networkValueIndex;
            }
        }
    }

    return ok;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    driver singleDriver;
    if (!checkHistoricalDefaults(singleDriver, QStringLiteral("driver::resetToDefaults"))) {
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
