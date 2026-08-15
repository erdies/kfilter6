/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffle_lf_hybrid_diagnostic_model.h"
#include "kfilterfrequencygrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "baffle LF hybrid smoke test failed: " << message << '\n';
        return false;
    }
    return true;
}

double magnitudeDb(const std::complex<double>& value)
{
    return 20.0 * std::log10(std::abs(value));
}

std::size_t nearestSampleIndex(double targetFrequencyHz)
{
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    std::size_t bestIndex = 0;
    double bestError = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < frequencies.size(); ++index) {
        const double error = std::abs(frequencies[index] - targetFrequencyHz);
        if (error < bestError) {
            bestError = error;
            bestIndex = index;
        }
    }
    return bestIndex;
}

bool responsesNear(const BaffleResponse& first,
                   const BaffleResponse& second,
                   double tolerance = 2.0e-12)
{
    if (first.status != second.status) {
        return false;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        if (std::abs(first.values[index] - second.values[index]) > tolerance) {
            return false;
        }
    }
    return true;
}

bool finiteResponse(const BaffleResponse& response)
{
    if (response.status != BaffleResponseStatus::Valid) {
        return false;
    }
    for (const std::complex<double>& value : response.values) {
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag()) ||
            std::abs(value) <= 0.0) {
            return false;
        }
    }
    return true;
}

BaffleSettings rectangularSettings(double widthMm,
                                    double heightMm,
                                    double driverXmm,
                                    double driverYmm)
{
    BaffleSettings settings;
    settings.enabled = true;
    settings.model = BaffleModel::RectangularEdgeDiffraction;
    settings.widthMm = widthMm;
    settings.heightMm = heightMm;
    settings.driverXmm = driverXmm;
    settings.driverYmm = driverYmm;
    settings.edgeSourceCount = 200;
    return settings;
}

bool checkGeometry(const BaffleSettings& settings, double diameterCm)
{
    const BaffleLfHybridDiagnostic diagnostic =
        calculateBaffleLfHybridDiagnostic(settings, diameterCm);
    const BaffleResponse productive = calculateBaffleResponse(settings, diameterCm);
    if (!require(diagnostic.valid, "valid sharp free-field geometry must produce diagnostic data") ||
        !require(responsesNear(productive, diagnostic.widthAnchoredHybridN2),
                 "Patch-246 productive Sharp response must equal the promoted width n=2 candidate") ||
        !require(finiteResponse(diagnostic.simple), "simple diagnostic response must be finite") ||
        !require(finiteResponse(diagnostic.rectangular), "rectangular diagnostic response must be finite") ||
        !require(finiteResponse(diagnostic.hybrid), "sqrt(W*H) hybrid diagnostic response must be finite") ||
        !require(finiteResponse(diagnostic.widthAnchoredHybrid), "width n=1 hybrid diagnostic response must be finite") ||
        !require(finiteResponse(diagnostic.widthAnchoredHybridN15), "width n=1.5 hybrid diagnostic response must be finite") ||
        !require(finiteResponse(diagnostic.widthAnchoredHybridN2), "width n=2 hybrid diagnostic response must be finite") ||
        !require(std::isfinite(diagnostic.effectiveLengthM) && diagnostic.effectiveLengthM > 0.0,
                 "effective LF geometry length must be finite and positive") ||
        !require(std::isfinite(diagnostic.simpleMidpointFrequencyHz) &&
                     diagnostic.simpleMidpointFrequencyHz > 0.0,
                 "Simple Baffle Step midpoint must be finite and positive")) {
        return false;
    }

    double previousWeight = -1.0;
    double previousWidthWeightN1 = -1.0;
    double previousWidthWeightN15 = -1.0;
    double previousWidthWeightN2 = -1.0;
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const double weight = diagnostic.blendWeight[index];
        const double widthWeightN1 = diagnostic.widthAnchoredBlendWeight[index];
        const double widthWeightN15 = diagnostic.widthAnchoredBlendWeightN15[index];
        const double widthWeightN2 = diagnostic.widthAnchoredBlendWeightN2[index];
        const double simpleDb = magnitudeDb(diagnostic.simple.values[index]);
        const double rectangularDb = magnitudeDb(diagnostic.rectangular.values[index]);
        const double hybridDb = magnitudeDb(diagnostic.hybrid.values[index]);
        const double widthHybridDbN1 = magnitudeDb(diagnostic.widthAnchoredHybrid.values[index]);
        const double widthHybridDbN15 = magnitudeDb(diagnostic.widthAnchoredHybridN15.values[index]);
        const double widthHybridDbN2 = magnitudeDb(diagnostic.widthAnchoredHybridN2.values[index]);
        const double minimumDb = std::min(simpleDb, rectangularDb) - 1.0e-10;
        const double maximumDb = std::max(simpleDb, rectangularDb) + 1.0e-10;
        const double rectangularPhase = std::arg(diagnostic.rectangular.values[index]);

        if (!require(weight > 0.0 && weight < 1.0,
                     "finite positive frequency must have 0 < sqrt(W*H) weight < 1") ||
            !require(widthWeightN1 > 0.0 && widthWeightN1 < 1.0,
                     "finite positive frequency must have 0 < width n=1 weight < 1") ||
            !require(widthWeightN15 > 0.0 && widthWeightN15 < 1.0,
                     "finite positive frequency must have 0 < width n=1.5 weight < 1") ||
            !require(widthWeightN2 > 0.0 && widthWeightN2 < 1.0,
                     "finite positive frequency must have 0 < width n=2 weight < 1") ||
            !require(weight > previousWeight,
                     "sqrt(W*H) hybrid weight must increase monotonically with frequency") ||
            !require(widthWeightN1 > previousWidthWeightN1,
                     "width n=1 weight must increase monotonically with frequency") ||
            !require(widthWeightN15 > previousWidthWeightN15,
                     "width n=1.5 weight must increase monotonically with frequency") ||
            !require(widthWeightN2 > previousWidthWeightN2,
                     "width n=2 weight must increase monotonically with frequency") ||
            !require(hybridDb >= minimumDb && hybridDb <= maximumDb,
                     "sqrt(W*H) hybrid magnitude must stay between simple and rectangular magnitudes") ||
            !require(widthHybridDbN1 >= minimumDb && widthHybridDbN1 <= maximumDb,
                     "width n=1 magnitude must stay between simple and rectangular magnitudes") ||
            !require(widthHybridDbN15 >= minimumDb && widthHybridDbN15 <= maximumDb,
                     "width n=1.5 magnitude must stay between simple and rectangular magnitudes") ||
            !require(widthHybridDbN2 >= minimumDb && widthHybridDbN2 <= maximumDb,
                     "width n=2 magnitude must stay between simple and rectangular magnitudes") ||
            !require(std::abs(std::arg(diagnostic.hybrid.values[index]) - rectangularPhase) < 1.0e-12,
                     "sqrt(W*H) candidate must preserve rectangular phase exactly") ||
            !require(std::abs(std::arg(diagnostic.widthAnchoredHybrid.values[index]) - rectangularPhase) < 1.0e-12,
                     "width n=1 candidate must preserve rectangular phase exactly") ||
            !require(std::abs(std::arg(diagnostic.widthAnchoredHybridN15.values[index]) - rectangularPhase) < 1.0e-12,
                     "width n=1.5 candidate must preserve rectangular phase exactly") ||
            !require(std::abs(std::arg(diagnostic.widthAnchoredHybridN2.values[index]) - rectangularPhase) < 1.0e-12,
                     "width n=2 candidate must preserve rectangular phase exactly")) {
            return false;
        }

        // The exponent family must fan out around the same 0.5 midpoint.
        if (frequencies[index] < diagnostic.simpleMidpointFrequencyHz) {
            if (!require(widthWeightN2 < widthWeightN15 && widthWeightN15 < widthWeightN1,
                         "below fBS higher exponents must remain more Simple-weighted")) {
                return false;
            }
        } else if (frequencies[index] > diagnostic.simpleMidpointFrequencyHz) {
            if (!require(widthWeightN2 > widthWeightN15 && widthWeightN15 > widthWeightN1,
                         "above fBS higher exponents must approach Rectangular faster")) {
                return false;
            }
        }

        previousWeight = weight;
        previousWidthWeightN1 = widthWeightN1;
        previousWidthWeightN15 = widthWeightN15;
        previousWidthWeightN2 = widthWeightN2;
    }

    return true;
}
}

int main()
{
    constexpr double ZrtEffectiveDiameterCm = 13.81976597885342;
    const BaffleSettings zrt = rectangularSettings(231.0, 965.0, 115.5, 228.6);
    const BaffleLfHybridDiagnostic zrtDiagnostic =
        calculateBaffleLfHybridDiagnostic(zrt, ZrtEffectiveDiameterCm);

    if (!require(zrtDiagnostic.valid, "ZRT reference geometry must be valid") ||
        !checkGeometry(zrt, ZrtEffectiveDiameterCm)) {
        return 1;
    }

    // Keep Patch-242/244 regression anchors around the region that motivated
    // the LF investigation. These remain engineering candidates, not measured
    // or MFS/BEM/FEM truth values.
    const std::size_t near100 = nearestSampleIndex(100.0);
    const std::size_t near200 = nearestSampleIndex(200.0);
    const std::size_t near500 = nearestSampleIndex(500.0);
    const std::size_t near1000 = nearestSampleIndex(1000.0);
    const double current100 = magnitudeDb(zrtDiagnostic.rectangular.values[near100]);
    const double hybrid100 = magnitudeDb(zrtDiagnostic.hybrid.values[near100]);
    const double simple100 = magnitudeDb(zrtDiagnostic.simple.values[near100]);
    const double current200 = magnitudeDb(zrtDiagnostic.rectangular.values[near200]);
    const double hybrid200 = magnitudeDb(zrtDiagnostic.hybrid.values[near200]);

    if (!require(current100 - hybrid100 > 0.70,
                 "ZRT sqrt(W*H) candidate must materially reduce the current ~100 Hz rise") ||
        !require(hybrid100 - simple100 > 0.05,
                 "ZRT sqrt(W*H) candidate must retain some rectangular contribution near 100 Hz") ||
        !require(current200 - hybrid200 > 0.80,
                 "ZRT sqrt(W*H) candidate must materially reduce the current ~200 Hz rise") ||
        !require(zrtDiagnostic.blendWeight.front() < 0.10,
                 "20 Hz sqrt(W*H) blend must remain strongly Simple-Baffle weighted") ||
        !require(zrtDiagnostic.blendWeight.back() > 0.98,
                 "top-of-grid sqrt(W*H) blend must approach the rectangular model")) {
        return 1;
    }

    // Width-anchored family: all exponents share the exact same fBS midpoint
    // and DC limit. n=1 must remain algebraically identical to Patch 244.
    constexpr double HeightInvariantWidthMm = 180.0;
    const double midpoint180Hz = simpleBaffleStepMidpointFrequencyHz(HeightInvariantWidthMm);
    const double sampleFrequencyHz = 100.237447;
    const double expectedN1 = sampleFrequencyHz / (sampleFrequencyHz + midpoint180Hz);
    if (!require(std::abs(baffleLfWidthAnchoredBlendWeight(HeightInvariantWidthMm,
                                                           midpoint180Hz,
                                                           1.0) - 0.5) < 1.0e-12,
                 "width n=1 must have w=0.5 at fBS") ||
        !require(std::abs(baffleLfWidthAnchoredBlendWeight(HeightInvariantWidthMm,
                                                           midpoint180Hz,
                                                           1.5) - 0.5) < 1.0e-12,
                 "width n=1.5 must have w=0.5 at fBS") ||
        !require(std::abs(baffleLfWidthAnchoredBlendWeight(HeightInvariantWidthMm,
                                                           midpoint180Hz,
                                                           2.0) - 0.5) < 1.0e-12,
                 "width n=2 must have w=0.5 at fBS") ||
        !require(baffleLfWidthAnchoredBlendWeight(HeightInvariantWidthMm, 0.0, 2.0) == 0.0,
                 "width family must tend exactly to zero at DC") ||
        !require(baffleLfWidthAnchoredBlendWeight(-1.0, 100.0, 2.0) == 0.0,
                 "width family must reject invalid width") ||
        !require(baffleLfWidthAnchoredBlendWeight(HeightInvariantWidthMm, 100.0, 0.0) == 0.0,
                 "width family must reject invalid exponent") ||
        !require(std::abs(baffleLfWidthAnchoredBlendWeight(HeightInvariantWidthMm,
                                                           sampleFrequencyHz) - expectedN1) < 1.0e-15,
                 "two-argument width helper must retain the Patch-244 n=1 law") ||
        !require(std::abs(baffleLfWidthAnchoredBlendWeight(HeightInvariantWidthMm,
                                                           sampleFrequencyHz,
                                                           1.0) - expectedN1) < 1.0e-15,
                 "generalized n=1 helper must retain the Patch-244 law")) {
        return 1;
    }

    // Controlled same-width pair: only cabinet height changes. sqrt(W*H)
    // reacts to the height change; all width-anchored exponents deliberately
    // remain invariant because their transition anchor is width-only.
    const BaffleSettings short180 = rectangularSettings(180.0, 280.0, 90.0, 120.0);
    const BaffleSettings tall180 = rectangularSettings(180.0, 1000.0, 90.0, 120.0);
    const BaffleLfHybridDiagnostic shortDiagnostic =
        calculateBaffleLfHybridDiagnostic(short180, 10.0);
    const BaffleLfHybridDiagnostic tallDiagnostic =
        calculateBaffleLfHybridDiagnostic(tall180, 10.0);
    if (!require(shortDiagnostic.valid && tallDiagnostic.valid,
                 "controlled height-pair diagnostics must be valid") ||
        !require(tallDiagnostic.blendWeight[near100] - shortDiagnostic.blendWeight[near100] > 0.05,
                 "sqrt(W*H) blend must expose material height dependence near 100 Hz") ||
        !require(std::abs(tallDiagnostic.widthAnchoredBlendWeight[near100] -
                          shortDiagnostic.widthAnchoredBlendWeight[near100]) < 1.0e-15,
                 "width n=1 blend must be height-invariant for equal widths") ||
        !require(std::abs(tallDiagnostic.widthAnchoredBlendWeightN15[near100] -
                          shortDiagnostic.widthAnchoredBlendWeightN15[near100]) < 1.0e-15,
                 "width n=1.5 blend must be height-invariant for equal widths") ||
        !require(std::abs(tallDiagnostic.widthAnchoredBlendWeightN2[near100] -
                          shortDiagnostic.widthAnchoredBlendWeightN2[near100]) < 1.0e-15,
                 "width n=2 blend must be height-invariant for equal widths") ||
        !require(std::abs(tallDiagnostic.simpleMidpointFrequencyHz -
                          shortDiagnostic.simpleMidpointFrequencyHz) < 1.0e-12,
                 "equal widths must have the same Simple Baffle Step midpoint")) {
        return 1;
    }

    // For the ZRT, 100/200 Hz are below fBS and must become progressively more
    // conservative with increasing n. Around/above fBS the ordering reverses.
    const double widthN1_100 = magnitudeDb(zrtDiagnostic.widthAnchoredHybrid.values[near100]);
    const double widthN15_100 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN15.values[near100]);
    const double widthN2_100 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN2.values[near100]);
    const double widthN1_200 = magnitudeDb(zrtDiagnostic.widthAnchoredHybrid.values[near200]);
    const double widthN15_200 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN15.values[near200]);
    const double widthN2_200 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN2.values[near200]);
    const double widthN1_500 = magnitudeDb(zrtDiagnostic.widthAnchoredHybrid.values[near500]);
    const double widthN15_500 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN15.values[near500]);
    const double widthN2_500 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN2.values[near500]);
    const double widthN1_1000 = magnitudeDb(zrtDiagnostic.widthAnchoredHybrid.values[near1000]);
    const double widthN15_1000 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN15.values[near1000]);
    const double widthN2_1000 = magnitudeDb(zrtDiagnostic.widthAnchoredHybridN2.values[near1000]);

    if (!require(widthN2_100 < widthN15_100 && widthN15_100 < widthN1_100 &&
                     widthN1_100 < hybrid100,
                 "ZRT 100 Hz family must become progressively more conservative from n=1 to n=2") ||
        !require(widthN2_200 < widthN15_200 && widthN15_200 < widthN1_200 &&
                     widthN1_200 < hybrid200,
                 "ZRT 200 Hz family must become progressively more conservative from n=1 to n=2") ||
        !require(widthN2_500 > widthN15_500 && widthN15_500 > widthN1_500,
                 "ZRT near/above fBS family ordering must reverse") ||
        !require(widthN2_1000 > widthN15_1000 && widthN15_1000 > widthN1_1000,
                 "ZRT above fBS higher exponents must approach Rectangular faster") ||
        !require(zrtDiagnostic.widthAnchoredBlendWeight.front() < 0.10 &&
                     zrtDiagnostic.widthAnchoredBlendWeightN15.front() < 0.05 &&
                     zrtDiagnostic.widthAnchoredBlendWeightN2.front() < 0.01,
                 "higher exponents must strongly suppress Rectangular contribution at 20 Hz") ||
        !require(zrtDiagnostic.widthAnchoredBlendWeight.back() > 0.95 &&
                     zrtDiagnostic.widthAnchoredBlendWeightN15.back() > 0.99 &&
                     zrtDiagnostic.widthAnchoredBlendWeightN2.back() > 0.999,
                 "all width candidates must converge toward Rectangular at the top of the grid")) {
        return 1;
    }

    const std::array<BaffleSettings, 9> additionalGeometries{{
        short180,
        rectangularSettings(200.0, 350.0, 100.0, 145.0),
        rectangularSettings(220.0, 900.0, 110.0, 210.0),
        rectangularSettings(300.0, 300.0, 150.0, 150.0),
        rectangularSettings(400.0, 300.0, 200.0, 150.0),
        rectangularSettings(250.0, 500.0, 78.0, 165.0),
        rectangularSettings(180.0, 1000.0, 90.0, 220.0),
        tall180,
        rectangularSettings(220.0, 300.0, 110.0, 210.0),
    }};
    const std::array<double, 9> diametersCm{{
        10.0, 12.0, 14.0, 15.0, 15.0, 12.0, 10.0, 10.0, 14.0
    }};

    for (std::size_t index = 0; index < additionalGeometries.size(); ++index) {
        if (!checkGeometry(additionalGeometries[index], diametersCm[index])) {
            return 1;
        }
    }

    BaffleSettings unsupported = zrt;
    unsupported.model = BaffleModel::SimpleBaffleStep;
    if (!require(!calculateBaffleLfHybridDiagnostic(unsupported, ZrtEffectiveDiameterCm).valid,
                 "diagnostic helper must reject non-rectangular input")) {
        return 1;
    }

    unsupported = zrt;
    unsupported.boundaryCondition = BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    if (!require(!calculateBaffleLfHybridDiagnostic(unsupported, ZrtEffectiveDiameterCm).valid,
                 "diagnostic helper must reject rigid-floor mode")) {
        return 1;
    }

    unsupported = zrt;
    unsupported.leftEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    unsupported.leftChamferSetbackMm = 20.0;
    if (!require(!calculateBaffleLfHybridDiagnostic(unsupported, ZrtEffectiveDiameterCm).valid,
                 "LF hybrid candidates must remain restricted to sharp-edge validation")) {
        return 1;
    }

    std::cout << "baffle LF hybrid production-promotion smoke test passed\n";
    return 0;
}
