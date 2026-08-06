/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "driver.h"

#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr int ResultValueCount = 300;
using ResultArray = std::array<double, ResultValueCount>;

ResultArray copyResult(const double* values)
{
    ResultArray result{};
    std::copy_n(values, ResultValueCount, result.begin());
    return result;
}

bool nearlyEqual(double left, double right)
{
    if (!std::isfinite(left) || !std::isfinite(right)) {
        return false;
    }

    const double scale = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= 1.0e-10 * scale;
}

bool resultArraysEqual(const ResultArray& left, const double* right)
{
    for (int index = 0; index < ResultValueCount; ++index) {
        if (!nearlyEqual(left[static_cast<std::size_t>(index)], right[index])) {
            return false;
        }
    }
    return true;
}

bool resultArraysDiffer(const ResultArray& left, const double* right)
{
    return !resultArraysEqual(left, right);
}

void calculateResults(driver& drv)
{
    drv.Schall();
    drv.Impedanz();
}

void configureDefault(driver&)
{
}

void configureClosed(driver& drv)
{
    drv.Vb = 20.0;
    drv.Fb = 0.0;
    drv.V2 = 0.0;
    drv.GTypProposal = 1;
    drv.setmodified();
}

void configureVented(driver& drv)
{
    drv.Vb = 20.0;
    drv.Fb = 40.0;
    drv.V2 = 0.0;
    drv.GTypProposal = 2;
    drv.setQl(7.0);
    drv.setmodified();
}

void configureFullCircuit(driver& drv)
{
    drv.setFullCircuit(true);
}

template<typename Configure, typename Mutate>
bool checkSetterInvalidation(const char* label, Configure configure, Mutate mutate)
{
    driver cachedDriver;
    configure(cachedDriver);
    calculateResults(cachedDriver);
    const ResultArray baselineSound = copyResult(cachedDriver.ResultSchall);
    const ResultArray baselineImpedance = copyResult(cachedDriver.ResultImpedanz);

    mutate(cachedDriver);
    calculateResults(cachedDriver);

    driver freshDriver;
    configure(freshDriver);
    mutate(freshDriver);
    calculateResults(freshDriver);

    if (!resultArraysEqual(copyResult(freshDriver.ResultSchall), cachedDriver.ResultSchall) ||
        !resultArraysEqual(copyResult(freshDriver.ResultImpedanz), cachedDriver.ResultImpedanz)) {
        QTextStream(stderr) << label << " left stale cached results\n";
        return false;
    }

    if (!resultArraysDiffer(baselineSound, cachedDriver.ResultSchall) &&
        !resultArraysDiffer(baselineImpedance, cachedDriver.ResultImpedanz)) {
        QTextStream(stderr) << label << " did not affect either calculated result; test setup is ineffective\n";
        return false;
    }

    return true;
}

bool checkAllSetterInvalidations()
{
    return
        checkSetterInvalidation("setRdc", configureDefault,
                                [](driver& drv) { drv.setRdc(6.3); }) &&
        checkSetterInvalidation("setLsp", configureDefault,
                                [](driver& drv) { drv.setLsp(0.00031); }) &&
        checkSetterInvalidation("setF0", configureDefault,
                                [](driver& drv) { drv.setF0(180.0); }) &&
        checkSetterInvalidation("setQtc", configureDefault,
                                [](driver& drv) { drv.setQtc(0.72); }) &&
        checkSetterInvalidation("setQes", configureDefault,
                                [](driver& drv) { drv.setQes(0.63); }) &&
        checkSetterInvalidation("setQms", configureDefault,
                                [](driver& drv) { drv.setQms(4.2); }) &&
        checkSetterInvalidation("setVas", configureClosed,
                                [](driver& drv) { drv.setVas(35.0); }) &&
        checkSetterInvalidation("setDm", configureFullCircuit,
                                [](driver& drv) { drv.setDm(10.0); }) &&
        checkSetterInvalidation("setQl", configureVented,
                                [](driver& drv) { drv.setQl(4.5); }) &&
        checkSetterInvalidation("setFullCircuit", configureDefault,
                                [](driver& drv) { drv.setFullCircuit(true); });
}

bool checkNetworkCleanupInvalidation()
{
    driver drv;
    drv.setUnit(1, 10.0);
    calculateResults(drv);
    const ResultArray networkSound = copyResult(drv.ResultSchall);
    const ResultArray networkImpedance = copyResult(drv.ResultImpedanz);

    drv.cleanupNetwork();
    calculateResults(drv);

    driver freshDriver;
    calculateResults(freshDriver);

    if (!resultArraysEqual(copyResult(freshDriver.ResultSchall), drv.ResultSchall) ||
        !resultArraysEqual(copyResult(freshDriver.ResultImpedanz), drv.ResultImpedanz)) {
        QTextStream(stderr) << "cleanupNetwork left stale cached results\n";
        return false;
    }

    if (!resultArraysDiffer(networkSound, drv.ResultSchall) &&
        !resultArraysDiffer(networkImpedance, drv.ResultImpedanz)) {
        QTextStream(stderr) << "cleanupNetwork regression setup is ineffective\n";
        return false;
    }

    return true;
}

bool checkPhaseStateReset()
{
    driver transitionedDriver;
    configureVented(transitionedDriver);
    transitionedDriver.Schall();
    if (transitionedDriver.GTyp != 2 || transitionedDriver.Phase_flag != 0) {
        QTextStream(stderr) << "Vented-box phase setup failed\n";
        return false;
    }

    transitionedDriver.Vb = 0.0;
    transitionedDriver.Fb = 0.0;
    transitionedDriver.V2 = 0.0;
    transitionedDriver.GTypProposal = 0;
    transitionedDriver.setmodified();
    transitionedDriver.Schall();

    if (transitionedDriver.GTyp != 0 || transitionedDriver.Phase_flag != 1) {
        QTextStream(stderr) << "Phase_flag was not reset after changing from vented to free-air\n";
        return false;
    }

    driver directDriver;
    directDriver.setQl(7.0);
    directDriver.Schall();
    if (!resultArraysEqual(copyResult(directDriver.ResultSchall), transitionedDriver.ResultSchall)) {
        QTextStream(stderr) << "Identical free-air end states produced different complex SPL results\n";
        return false;
    }

    return true;
}

bool checkCalculationFlagRecovery()
{
    driver recoveredDriver;
    recoveredDriver.setF0(0.0);
    recoveredDriver.Schall();
    if (recoveredDriver.Parameter_flag != 0 || recoveredDriver.AkustikESB_flag != 0) {
        QTextStream(stderr) << "F0 == 0 did not disable invalid parameter calculation\n";
        return false;
    }

    recoveredDriver.setF0(180.0);
    recoveredDriver.Schall();
    if (recoveredDriver.Parameter_flag != 1 || recoveredDriver.AkustikESB_flag != 1) {
        QTextStream(stderr) << "Valid F0 did not restore calculation flags\n";
        return false;
    }

    driver directDriver;
    directDriver.setF0(180.0);
    directDriver.Schall();
    if (!resultArraysEqual(copyResult(directDriver.ResultSchall), recoveredDriver.ResultSchall)) {
        QTextStream(stderr) << "Recovered and directly valid drivers produced different SPL results\n";
        return false;
    }

    return true;
}
}

int main()
{
    driver d;
    d.SetTitle(QStringLiteral("Qt6 driver smoke test"));
    d.setRdc(5.6);
    d.setF0(300.0);
    d.setQl(7.5);
    calculateResults(d);

    if (!checkAllSetterInvalidations() ||
        !checkNetworkCleanupInvalidation() ||
        !checkPhaseStateReset() ||
        !checkCalculationFlagRecovery()) {
        return 1;
    }

    QTextStream out(stdout);
    out << d.GetTitle() << '\n';
    out << "Rdc=" << d.getRdc() << '\n';
    out << "Ql=" << d.getQl() << '\n';
    out << "Sound active=" << d.PressureisActive << '\n';
    out << "Impedance[0]=" << d.ResultImpedanz[0] << '\n';
    out << "Driver state regression smoke test passed\n";
    return 0;
}
