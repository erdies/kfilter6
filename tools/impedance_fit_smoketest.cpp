/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "impedancefit.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
constexpr double TrueRdc = 6.1;
constexpr double TrueLsp = 0.15e-3;
constexpr double TrueR2 = 10.4;
constexpr double TrueL2 = 0.33e-3;

bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "impedance fit smoketest failed: " << message << '\n';
        return false;
    }
    return true;
}

bool relativelyClose(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance * std::abs(expected);
}

void configureTsDriver(driver& drv)
{
    drv.setF0(55.0);
    drv.setQes(0.45);
    drv.setQms(3.2);
    drv.setQts(0.45 * 3.2 / (0.45 + 3.2));
    drv.setVas(12.0);
    drv.setDm(10.0);
    drv.setVb(8.0);
    drv.setEnclosureTypeProposal(EnclosureType::Sealed);
}

// Synthetic ZMA from the true LR2 driver, 160 log-spaced points 20 Hz..20 kHz.
KFilterImpedanceReferenceCurve syntheticZma(bool withPhase)
{
    driver reference;
    configureTsDriver(reference);
    reference.setRdc(TrueRdc);
    reference.setLsp(TrueLsp);
    reference.setR2(TrueR2);
    reference.setL2(TrueL2);

    KFilterImpedanceReferenceCurve curve;
    for (int index = 0; index < 160; ++index) {
        const double frequencyHz = 20.0 * std::pow(1000.0, index / 159.0);
        const std::complex<double> z = reference.electricalImpedance(frequencyHz);
        curve.append(KFilterImpedancePoint{frequencyHz,
                                           std::abs(z),
                                           withPhase ? std::arg(z) * 180.0 / 3.141592653589793
                                                     : std::numeric_limits<double>::quiet_NaN()});
    }
    return curve;
}

driver plainStartDriver()
{
    driver drv;
    configureTsDriver(drv);
    drv.setRdc(TrueRdc);
    drv.setLsp(0.5e-3);
    return drv;
}
}

int main()
{
    const KFilterImpedanceReferenceCurve zma = syntheticZma(true);
    driver start = plainStartDriver();

    KFilterImpedanceFitOptions options;
    defaultImpedanceFitBand(start, zma, options.minimumFrequencyHz, options.maximumFrequencyHz);
    if (!require(std::abs(options.minimumFrequencyHz - 110.0) < 1.0e-9 &&
                     std::abs(options.maximumFrequencyHz - 20000.0) < 1.0e-6,
                 "default band is not 2 x Fs .. last ZMA frequency")) {
        return 1;
    }

    // Lsp, R2, L2 with fixed Rdc; the resonance stays in the model.
    KFilterImpedanceFitResult result = fitVoiceCoilToImpedance(start, zma, options);
    if (!require(result.success, "complex fit failed") ||
        !require(!result.magnitudeOnly, "complex ZMA was treated as magnitude only") ||
        !require(result.initialRmsPercent > 5.0, "plain start model already matched") ||
        !require(result.finalRmsPercent < 1.0e-3, "complex fit residual too large") ||
        !require(relativelyClose(result.lspH, TrueLsp, 1.0e-4) &&
                     relativelyClose(result.r2Ohm, TrueR2, 1.0e-4) &&
                     relativelyClose(result.l2H, TrueL2, 1.0e-4),
                 "complex fit did not recover Lsp/R2/L2") ||
        !require(result.rdcOhm == TrueRdc, "fixed Rdc was changed")) {
        return 1;
    }

    // Rdc released: a wrong start Rdc must be corrected as well.
    driver wrongRdc = plainStartDriver();
    wrongRdc.setRdc(7.0);
    options.fitRdc = true;
    result = fitVoiceCoilToImpedance(wrongRdc, zma, options);
    if (!require(result.success && result.finalRmsPercent < 1.0e-3, "fit with free Rdc failed") ||
        !require(relativelyClose(result.rdcOhm, TrueRdc, 1.0e-4) &&
                     relativelyClose(result.l2H, TrueL2, 1.0e-3),
                 "fit with free Rdc did not recover Rdc/L2")) {
        return 1;
    }
    options.fitRdc = false;

    // Without phase only |Z| is compared.
    result = fitVoiceCoilToImpedance(start, syntheticZma(false), options);
    if (!require(result.success && result.magnitudeOnly, "magnitude-only fit failed") ||
        !require(result.finalRmsPercent < 0.1, "magnitude-only fit residual too large") ||
        !require(relativelyClose(result.lspH, TrueLsp, 0.05) &&
                     relativelyClose(result.l2H, TrueL2, 0.05),
                 "magnitude-only fit did not approximately recover Lsp/L2")) {
        return 1;
    }

    // Invalid bands are reported, not fitted.
    KFilterImpedanceFitOptions narrow;
    narrow.minimumFrequencyHz = 1000.0;
    narrow.maximumFrequencyHz = 1050.0;
    result = fitVoiceCoilToImpedance(start, zma, narrow);
    if (!require(!result.success && !result.errorMessage.isEmpty(), "too narrow band was fitted")) {
        return 1;
    }
    narrow.maximumFrequencyHz = 500.0;
    result = fitVoiceCoilToImpedance(start, zma, narrow);
    if (!require(!result.success, "inverted band was fitted")) {
        return 1;
    }

    std::cout << "impedance fit smoketest passed\n";
    return 0;
}
