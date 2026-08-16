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
#include <complex>

namespace
{
constexpr std::size_t ResultSampleCount = 150;
using ResultArray = std::array<std::complex<double>, ResultSampleCount>;

ResultArray copyResult(const ResultArray& values)
{
    return values;
}

bool nearlyEqual(const std::complex<double>& left, const std::complex<double>& right)
{
    if (!std::isfinite(left.real()) || !std::isfinite(left.imag()) ||
        !std::isfinite(right.real()) || !std::isfinite(right.imag())) {
        return false;
    }

    const double scale = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= 1.0e-10 * scale;
}

bool resultArraysEqual(const ResultArray& left, const ResultArray& right)
{
    for (std::size_t index = 0; index < ResultSampleCount; ++index) {
        if (!nearlyEqual(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool resultArraysDiffer(const ResultArray& left, const ResultArray& right)
{
    return !resultArraysEqual(left, right);
}

void calculateResults(driver& drv)
{
    drv.calculatePressureResponse();
    drv.calculateImpedanceResponse();
}

void configureDefault(driver&)
{
}

void configureClosed(driver& drv)
{
    drv.Vb = 20.0;
    drv.Fb = 0.0;
    drv.V2 = 0.0;
    drv.enclosureTypeProposal = EnclosureType::Sealed;
    drv.setModified();
}

void configureVented(driver& drv)
{
    drv.Vb = 20.0;
    drv.Fb = 40.0;
    drv.V2 = 0.0;
    drv.enclosureTypeProposal = EnclosureType::Vented;
    drv.setQl(7.0);
    drv.setModified();
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
    const ResultArray baselineSound = copyResult(cachedDriver.ResultPressure);
    const ResultArray baselineImpedance = copyResult(cachedDriver.ResultImpedance);

    mutate(cachedDriver);
    calculateResults(cachedDriver);

    driver freshDriver;
    configure(freshDriver);
    mutate(freshDriver);
    calculateResults(freshDriver);

    if (!resultArraysEqual(copyResult(freshDriver.ResultPressure), cachedDriver.ResultPressure) ||
        !resultArraysEqual(copyResult(freshDriver.ResultImpedance), cachedDriver.ResultImpedance)) {
        QTextStream(stderr) << label << " left stale cached results\n";
        return false;
    }

    if (!resultArraysDiffer(baselineSound, cachedDriver.ResultPressure) &&
        !resultArraysDiffer(baselineImpedance, cachedDriver.ResultImpedance)) {
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
    const ResultArray networkSound = copyResult(drv.ResultPressure);
    const ResultArray networkImpedance = copyResult(drv.ResultImpedance);

    drv.cleanupNetwork();
    calculateResults(drv);

    driver freshDriver;
    calculateResults(freshDriver);

    if (!resultArraysEqual(copyResult(freshDriver.ResultPressure), drv.ResultPressure) ||
        !resultArraysEqual(copyResult(freshDriver.ResultImpedance), drv.ResultImpedance)) {
        QTextStream(stderr) << "cleanupNetwork left stale cached results\n";
        return false;
    }

    if (!resultArraysDiffer(networkSound, drv.ResultPressure) &&
        !resultArraysDiffer(networkImpedance, drv.ResultImpedance)) {
        QTextStream(stderr) << "cleanupNetwork regression setup is ineffective\n";
        return false;
    }

    return true;
}

bool checkEnclosureTransitionConsistency()
{
    driver transitionedDriver;
    configureVented(transitionedDriver);
    transitionedDriver.calculatePressureResponse();
    if (transitionedDriver.enclosureType != EnclosureType::Vented) {
        QTextStream(stderr) << "Vented-box setup failed\n";
        return false;
    }

    transitionedDriver.Vb = 0.0;
    transitionedDriver.Fb = 0.0;
    transitionedDriver.V2 = 0.0;
    transitionedDriver.enclosureTypeProposal = EnclosureType::OpenBaffle;
    transitionedDriver.setModified();
    transitionedDriver.calculatePressureResponse();

    if (transitionedDriver.enclosureType != EnclosureType::OpenBaffle) {
        QTextStream(stderr) << "Free-air enclosure state was not restored after vented operation\n";
        return false;
    }

    driver directDriver;
    directDriver.setQl(7.0);
    directDriver.calculatePressureResponse();
    if (!resultArraysEqual(copyResult(directDriver.ResultPressure), transitionedDriver.ResultPressure)) {
        QTextStream(stderr) << "Identical free-air end states produced different complex SPL results\n";
        return false;
    }

    return true;
}

bool checkZeroF0ImpedanceModel()
{
    auto verifyVoiceCoilOnly = [](driver& drv, const char* label) {
        drv.calculateImpedanceResponse();

        if (drv.parameterFlag) {
            QTextStream(stderr) << label << " did not disable TS-parameter calculation\n";
            return false;
        }

        double omega = 125.6637061;
        constexpr double frequencyFactor = 1.047128548;
        for (std::size_t index = 0; index < ResultSampleCount; ++index) {
            const std::complex<double> expected{drv.getRdc(), omega * drv.getLsp()};
            if (!nearlyEqual(drv.ResultImpedance[index], expected)) {
                QTextStream(stderr)
                    << label << " produced a non-voice-coil impedance at sample "
                    << static_cast<unsigned long long>(index) << '\n';
                return false;
            }
            omega *= frequencyFactor;
        }
        return true;
    };

    driver directZeroDriver;
    directZeroDriver.setF0(0.0);
    if (!verifyVoiceCoilOnly(directZeroDriver, "Direct F0 == 0")) {
        return false;
    }

    driver transitionedToZeroDriver;
    transitionedToZeroDriver.setF0(180.0);
    transitionedToZeroDriver.calculateImpedanceResponse();
    transitionedToZeroDriver.setF0(0.0);
    if (!verifyVoiceCoilOnly(transitionedToZeroDriver, "F0 > 0 -> F0 == 0")) {
        return false;
    }

    if (!resultArraysEqual(copyResult(directZeroDriver.ResultImpedance),
                           transitionedToZeroDriver.ResultImpedance)) {
        QTextStream(stderr) << "Direct and transitioned F0 == 0 impedances differ\n";
        return false;
    }

    transitionedToZeroDriver.setF0(180.0);
    transitionedToZeroDriver.calculateImpedanceResponse();

    driver directValidDriver;
    directValidDriver.setF0(180.0);
    directValidDriver.calculateImpedanceResponse();
    if (!resultArraysEqual(copyResult(directValidDriver.ResultImpedance),
                           transitionedToZeroDriver.ResultImpedance)) {
        QTextStream(stderr) << "F0 == 0 -> F0 > 0 did not restore the TS impedance model\n";
        return false;
    }

    return true;
}

bool checkCalculationFlagRecovery()
{
    driver recoveredDriver;
    recoveredDriver.setF0(0.0);
    recoveredDriver.calculatePressureResponse();
    if (recoveredDriver.parameterFlag) {
        QTextStream(stderr) << "F0 == 0 did not disable invalid parameter calculation\n";
        return false;
    }

    recoveredDriver.setF0(180.0);
    recoveredDriver.calculatePressureResponse();
    if (!recoveredDriver.parameterFlag) {
        QTextStream(stderr) << "Valid F0 did not restore TS-parameter calculation\n";
        return false;
    }

    driver directDriver;
    directDriver.setF0(180.0);
    directDriver.calculatePressureResponse();
    if (!resultArraysEqual(copyResult(directDriver.ResultPressure), recoveredDriver.ResultPressure)) {
        QTextStream(stderr) << "Recovered and directly valid drivers produced different SPL results\n";
        return false;
    }

    return true;
}
}

int main()
{
    driver d;
    d.setTitle(QStringLiteral("Qt6 driver smoke test"));
    d.setRdc(5.6);
    d.setF0(300.0);
    d.setQl(7.5);
    calculateResults(d);

    if (!checkAllSetterInvalidations() ||
        !checkNetworkCleanupInvalidation() ||
        !checkEnclosureTransitionConsistency() ||
        !checkZeroF0ImpedanceModel() ||
        !checkCalculationFlagRecovery()) {
        return 1;
    }

    QTextStream out(stdout);
    out << d.getTitle() << '\n';
    out << "Rdc=" << d.getRdc() << '\n';
    out << "Ql=" << d.getQl() << '\n';
    out << "Sound active=" << d.pressureIsActive << '\n';
    out << "Impedance[0]=" << d.ResultImpedance[0].real()
        << "+j" << d.ResultImpedance[0].imag() << '\n';
    out << "Driver state regression smoke test passed\n";
    return 0;
}
