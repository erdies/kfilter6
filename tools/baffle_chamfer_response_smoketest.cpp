/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "bafflechamferresponse.h"
#include "baffleresponse.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>

namespace
{
bool require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "baffle chamfer response smoke test failed: " << message << '\n';
        return false;
    }
    return true;
}

bool nearComplex(const std::complex<double>& actual,
                 const std::complex<double>& expected,
                 double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

bool responsesExactlyEqual(const BaffleResponse& first, const BaffleResponse& second)
{
    if (first.status != second.status) {
        return false;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        if (first.values[index] != second.values[index]) {
            return false;
        }
    }
    return true;
}

bool allFinite(const BaffleResponse& response)
{
    for (const std::complex<double>& value : response.values) {
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag())) {
            return false;
        }
    }
    return true;
}

double magnitudeDb(const std::complex<double>& value)
{
    return 20.0 * std::log10(std::abs(value));
}

double maxMagnitudeDifferenceDb(const BaffleResponse& first, const BaffleResponse& second)
{
    double maximum = 0.0;
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        maximum = std::max(
            maximum,
            std::abs(magnitudeDb(first.values[index]) - magnitudeDb(second.values[index])));
    }
    return maximum;
}

BaffleSettings zrtSettings()
{
    BaffleSettings settings;
    settings.enabled = true;
    settings.model = BaffleModel::RectangularEdgeDiffraction;
    settings.widthMm = 231.0;
    settings.heightMm = 965.0;
    settings.driverXmm = 115.5;
    settings.driverYmm = 228.6;
    settings.edgeSourceCount = 200;
    return settings;
}
}

int main()
{
    const BaffleSettings zrt = zrtSettings();
    constexpr double ZrtEffectiveDiameterCm = 13.81976597885342;

    // Stage 3A hard invariant: zero chamfer delegates to the untouched
    // Patch-211 path exactly, for both point-source and M=73 finite-piston use.
    const BaffleChamfer45Parameters sharp{};
    const BaffleResponse rawSharpFinite =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(zrt, ZrtEffectiveDiameterCm);
    const BaffleResponse legacyPoint = calculateBaffleResponse(zrt, 0.0);
    const BaffleResponse stage3aPointSharp =
        calculateBaffleResponseWithChamfer45(zrt, sharp, 0.0);
    const BaffleResponse legacyFinite = calculateBaffleResponse(zrt, ZrtEffectiveDiameterCm);
    const BaffleResponse stage3aFiniteSharp =
        calculateBaffleResponseWithChamfer45(zrt, sharp, ZrtEffectiveDiameterCm);
    if (!require(responsesExactlyEqual(stage3aPointSharp, legacyPoint),
                 "zero-chamfer point source must equal Patch-211 exactly") ||
        !require(responsesExactlyEqual(stage3aFiniteSharp, legacyFinite),
                 "zero-chamfer finite piston must equal Patch-211 exactly")) {
        return 1;
    }

    // Independent Stage-3A Python far-field reference for the ZRT geometry,
    // 30 mm left 45-degree chamfer. These values validate the C123 H1+H2+H3
    // kernel, including the analytic R->infinity receiver limit, and its
    // normalization against the original outer 90-degree sharp edge.
    const BaffleChamfer45SideCorrection c30 =
        calculateBaffleChamfer45SideCorrection(0.1155, 0.2286, 0.965, 0.030);
    if (!require(c30.valid, "30 mm C123 reference correction must be valid")) {
        return 1;
    }
    struct GoldenCorrection
    {
        std::size_t index;
        std::complex<double> value;
    };
    const std::array<GoldenCorrection, 6> correctionGolden{{
        {0,   {1.0222835350427726,  0.003975263186542920}},
        {50,  {1.0102650571995946,  0.042381302457162310}},
        {75,  {0.9308280703646888,  0.132720201537934350}},
        {100, {0.2550242572137050,  0.539221139881324800}},
        {125, {-0.6369130579092527, -0.120178838967285480}},
        {149, {-0.5576271705879191, -0.767431937473756200}}
    }};
    for (const GoldenCorrection& golden : correctionGolden) {
        if (!require(nearComplex(c30.values[golden.index], golden.value, 2.0e-12),
                     "C123 must match the independent analytic-far-field numerical reference")) {
            return 1;
        }
    }

    // Exercise the Stage-2 adaptive quadrature boundary: 5 mm on the 965 mm
    // ZRT height selects N=116 and must still reproduce the independent model.
    const BaffleChamfer45SideCorrection c5 =
        calculateBaffleChamfer45SideCorrection(0.1155, 0.2286, 0.965, 0.005);
    if (!require(c5.valid, "5 mm minimum production chamfer correction must be valid") ||
        !require(nearComplex(c5.values[100],
                             {0.9616677885688603, 0.07288646776004402},
                             2.0e-12),
                 "5 mm adaptive-quadrature correction must match analytic-far-field reference")) {
        return 1;
    }

    BaffleChamfer45Parameters symmetric30;
    symmetric30.leftSetbackMm = 30.0;
    symmetric30.rightSetbackMm = 30.0;
    const BaffleResponse chamferPoint =
        calculateBaffleResponseWithChamfer45(zrt, symmetric30, 0.0);
    const BaffleResponse chamferFinite =
        calculateBaffleResponseWithChamfer45(zrt, symmetric30, ZrtEffectiveDiameterCm);
    BaffleSettings configured30 = zrt;
    configured30.leftEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    configured30.leftChamferSetbackMm = 30.0;
    configured30.rightEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    configured30.rightChamferSetbackMm = 30.0;
    const BaffleResponse rawChamferPoint =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(configured30, 0.0);
    const BaffleResponse rawChamferFinite =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(
            configured30, ZrtEffectiveDiameterCm);
    const BaffleResponse configuredPoint = calculateBaffleResponse(configured30, 0.0);
    const BaffleResponse configuredFinite =
        calculateBaffleResponse(configured30, ZrtEffectiveDiameterCm);
    if (!require(chamferPoint.status == BaffleResponseStatus::Valid,
                 "30 mm point-source chamfer response must be valid") ||
        !require(chamferFinite.status == BaffleResponseStatus::Valid,
                 "30 mm finite-piston chamfer response must be valid") ||
        !require(allFinite(chamferPoint) && allFinite(chamferFinite),
                 "valid chamfer responses must remain finite") ||
        !require(responsesExactlyEqual(configuredPoint, chamferPoint),
                 "BaffleSettings Chamfer45 point source must use the validated Stage-3A engine") ||
        !require(responsesExactlyEqual(configuredFinite, chamferFinite),
                 "BaffleSettings Chamfer45 finite piston must use the productive Stage-3A plus LF-hybrid path")) {
        return 1;
    }

    // Patch 246 leaves the raw Stage-3A chamfer engine available for golden
    // validation, while the productive result rescales magnitude only.
    const std::size_t near100 = 35; // 100.237447 Hz on the shared 150-point grid.
    if (!require(std::abs(std::arg(chamferFinite.values[near100]) -
                          std::arg(rawChamferFinite.values[near100])) < 1.0e-12,
                 "productive chamfer LF hybrid must preserve raw chamfer phase") ||
        !require(magnitudeDb(chamferFinite.values[near100]) <
                     magnitudeDb(rawChamferFinite.values[near100]),
                 "productive chamfer LF hybrid must reduce the ZRT raw ~100 Hz rise")) {
        return 1;
    }

    // Full-response goldens are from the independent Stage-2 Python model and
    // deliberately validate the unblended raw chamfer reference.
    const std::array<GoldenCorrection, 6> pointGolden{{
        {0,   {0.9867273383780124,  0.07782938206892974}},
        {50,  {1.3243255340409520,  0.56587877216380070}},
        {75,  {2.2017755153528630,  0.67107159790082060}},
        {100, {2.2966266813619467,  0.01234032189253882}},
        {125, {2.1150367173920386, -0.10092009638119814}},
        {149, {1.8519637799942240, -0.07790528708335234}}
    }};
    const std::array<GoldenCorrection, 6> finiteGolden{{
        {0,   {0.9867287329629382,  0.07646828202635110}},
        {50,  {1.3231989353809106,  0.55046088361766390}},
        {75,  {2.1553993630844372,  0.61895345679101930}},
        {100, {2.1152976319137070,  0.03230645122899467}},
        {125, {2.0052550170888073, -0.00757315876283117}},
        {149, {1.9991319740910696,  0.00087522040545128}}
    }};
    for (const GoldenCorrection& golden : pointGolden) {
        if (!require(nearComplex(rawChamferPoint.values[golden.index], golden.value, 3.0e-12),
                     "point-source chamfer response must match analytic-far-field reference")) {
            return 1;
        }
    }
    for (const GoldenCorrection& golden : finiteGolden) {
        if (!require(nearComplex(rawChamferFinite.values[golden.index], golden.value, 3.0e-12),
                     "finite-piston chamfer response must match analytic-far-field centre-C123 reference")) {
            return 1;
        }
    }

    const double finiteEffectDb = maxMagnitudeDifferenceDb(rawChamferFinite, rawSharpFinite);
    if (!require(finiteEffectDb > 0.44 && finiteEffectDb < 0.49,
                 "30 mm ZRT finite-piston chamfer effect must remain near the analytic-far-field 0.46 dB reference")) {
        return 1;
    }

    // The finite piston must fit on the remaining flat front surface.  When it
    // does not, the local fallback is the chamfered point-source response, not
    // the old Sharp response and not a global baffle bypass.
    BaffleChamfer45Parameters symmetric20;
    symmetric20.leftSetbackMm = 20.0;
    symmetric20.rightSetbackMm = 20.0;
    const BaffleResponse chamfer20Point =
        calculateBaffleResponseWithChamfer45(zrt, symmetric20, 0.0);
    const BaffleResponse chamfer20Oversized =
        calculateBaffleResponseWithChamfer45(zrt, symmetric20, 20.0);
    if (!require(responsesExactlyEqual(chamfer20Oversized, chamfer20Point),
                 "chamfer finite-piston edge-touch must fall back exactly to chamfer point source")) {
        return 1;
    }

    // Version-1 bounded model: Sharp is a distinct exact state; positive
    // chamfer widths below 5 mm and geometries that consume the driver centre
    // are rejected instead of being empirically blended toward Sharp.
    for (double invalidWidth : {-1.0,
                                1.0,
                                4.999,
                                std::numeric_limits<double>::quiet_NaN()}) {
        BaffleChamfer45Parameters invalid;
        invalid.leftSetbackMm = invalidWidth;
        const BaffleResponse response =
            calculateBaffleResponseWithChamfer45(zrt, invalid, 0.0);
        if (!require(response.status == BaffleResponseStatus::InvalidParameters,
                     "invalid/sub-5-mm positive chamfer must be rejected")) {
            return 1;
        }
    }

    BaffleChamfer45Parameters consumesCentre;
    consumesCentre.leftSetbackMm = 115.5;
    const BaffleResponse invalidCentre =
        calculateBaffleResponseWithChamfer45(zrt, consumesCentre, 0.0);
    if (!require(invalidCentre.status == BaffleResponseStatus::InvalidParameters,
                 "driver centre must remain on the flat front surface")) {
        return 1;
    }

    BaffleSettings simple = zrt;
    simple.model = BaffleModel::SimpleBaffleStep;
    const BaffleResponse unsupported =
        calculateBaffleResponseWithChamfer45(simple, symmetric30, 0.0);
    if (!require(unsupported.status == BaffleResponseStatus::UnsupportedModel,
                 "Chamfer45 must remain specific to Rectangular Edge Diffraction")) {
        return 1;
    }

    BaffleSettings disabled = zrt;
    disabled.enabled = false;
    const BaffleResponse neutral =
        calculateBaffleResponseWithChamfer45(disabled, symmetric30, 0.0);
    if (!require(neutral.status == BaffleResponseStatus::Neutral,
                 "disabled baffle processing must remain neutral with transient chamfer parameters")) {
        return 1;
    }

    std::cout << "baffle chamfer response smoke test passed\n";
    return 0;
}
