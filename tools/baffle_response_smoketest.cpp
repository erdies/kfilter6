/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleresponse.h"
#include "kfilterfrequencygrid.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "baffle response smoke test failed: " << message << '\n';
        return false;
    }
    return true;
}

bool near(double actual, double expected, double tolerance = 1.0e-10)
{
    return std::abs(actual - expected) <= tolerance;
}

bool nearComplex(const std::complex<double>& actual,
                 const std::complex<double>& expected,
                 double tolerance = 1.0e-10)
{
    return std::abs(actual - expected) <= tolerance;
}

bool allUnity(const BaffleResponse& response)
{
    for (const std::complex<double>& value : response.values) {
        if (!nearComplex(value, {1.0, 0.0})) {
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

bool responsesNear(const BaffleResponse& actual,
                   const BaffleResponse& expected,
                   double tolerance = 1.0e-10)
{
    if (actual.status != expected.status) {
        return false;
    }
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        if (!nearComplex(actual.values[sampleIndex], expected.values[sampleIndex], tolerance)) {
            return false;
        }
    }
    return true;
}

bool phaseDirectionsNear(const BaffleResponse& first,
                         const BaffleResponse& second,
                         double tolerance = 2.0e-12)
{
    if (first.status != BaffleResponseStatus::Valid ||
        second.status != BaffleResponseStatus::Valid) {
        return false;
    }
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double firstMagnitude = std::abs(first.values[sampleIndex]);
        const double secondMagnitude = std::abs(second.values[sampleIndex]);
        if (firstMagnitude <= 0.0 || secondMagnitude <= 0.0) {
            return false;
        }
        const std::complex<double> firstDirection = first.values[sampleIndex] / firstMagnitude;
        const std::complex<double> secondDirection = second.values[sampleIndex] / secondMagnitude;
        if (std::abs(firstDirection - secondDirection) > tolerance) {
            return false;
        }
    }
    return true;
}

double maxMagnitudeDifferenceDb(const BaffleResponse& first,
                                const BaffleResponse& second)
{
    double maximum = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        maximum = std::max(maximum,
                           std::abs(magnitudeDb(first.values[sampleIndex]) -
                                    magnitudeDb(second.values[sampleIndex])));
    }
    return maximum;
}

double magnitudeRangeDb(const BaffleResponse& response,
                        double minimumFrequencyHz,
                        double maximumFrequencyHz)
{
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    double minimumDb = std::numeric_limits<double>::infinity();
    double maximumDb = -std::numeric_limits<double>::infinity();
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        if (frequencies[sampleIndex] < minimumFrequencyHz ||
            frequencies[sampleIndex] > maximumFrequencyHz) {
            continue;
        }
        const double db = magnitudeDb(response.values[sampleIndex]);
        minimumDb = std::min(minimumDb, db);
        maximumDb = std::max(maximumDb, db);
    }
    return maximumDb - minimumDb;
}

BaffleResponse finitePistonReferenceFromPointSources(const BaffleSettings& settings,
                                                      double effectiveDriverDiameterCm,
                                                      std::size_t sourceCount)
{
    constexpr double Pi = 3.141592653589793238462643383279502884;
    const double goldenAngle = Pi * (3.0 - std::sqrt(5.0));
    const double radiusMm = effectiveDriverDiameterCm * 5.0;

    BaffleResponse response;
    response.status = BaffleResponseStatus::Valid;
    response.values.fill({0.0, 0.0});

    for (std::size_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) {
        const double radius = radiusMm *
            std::sqrt((static_cast<double>(sourceIndex) + 0.5) /
                      static_cast<double>(sourceCount));
        const double theta = static_cast<double>(sourceIndex) * goldenAngle;
        BaffleSettings pointSettings = settings;
        pointSettings.driverXmm += radius * std::cos(theta);
        pointSettings.driverYmm += radius * std::sin(theta);
        const BaffleResponse pointResponse = calculateBaffleResponse(pointSettings, 0.0);
        if (pointResponse.status != BaffleResponseStatus::Valid) {
            return pointResponse;
        }
        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            response.values[sampleIndex] += pointResponse.values[sampleIndex];
        }
    }

    const double inverseSourceCount = 1.0 / static_cast<double>(sourceCount);
    for (std::complex<double>& value : response.values) {
        value *= inverseSourceCount;
    }
    return response;
}

BaffleSettings rectangularSettings()
{
    BaffleSettings settings;
    settings.enabled = true;
    settings.model = BaffleModel::RectangularEdgeDiffraction;
    settings.widthMm = 231.0;
    settings.heightMm = 900.0;
    settings.driverXmm = 90.0;
    settings.driverYmm = 310.0;
    settings.edgeSourceCount = 200;
    return settings;
}
}

int main()
{
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    constexpr std::size_t MidpointIndex = 75;
    const double alignedWidthMm = 115000.0 / frequencies[MidpointIndex];
    const double sqrt2 = std::sqrt(2.0);

    BaffleSettings settings;
    BaffleResponse response = calculateBaffleResponse(settings);
    if (!require(response.status == BaffleResponseStatus::Neutral,
                 "disabled baffle must be neutral") ||
        !require(!response.plottable(), "neutral baffle must not be plottable") ||
        !require(allUnity(response), "disabled baffle must produce unity response")) {
        return 1;
    }

    // Stage 1 remains unchanged by the Stage-2 implementation.
    settings.enabled = true;
    settings.model = BaffleModel::SimpleBaffleStep;
    settings.widthMm = alignedWidthMm;
    response = calculateBaffleResponse(settings);
    if (!require(response.status == BaffleResponseStatus::Valid,
                 "valid Simple Baffle Step must be supported") ||
        !require(response.plottable(), "valid baffle response must be plottable") ||
        !require(allFinite(response), "valid baffle response must be finite") ||
        !require(near(simpleBaffleStepMidpointFrequencyHz(alignedWidthMm),
                      frequencies[MidpointIndex],
                      1.0e-10),
                 "115/W midpoint must align with selected grid frequency") ||
        !require(near(std::abs(response.values[MidpointIndex]), sqrt2, 2.0e-10),
                 "Simple Baffle Step midpoint magnitude must be +3.0103 dB") ||
        !require(nearComplex(response.values[MidpointIndex],
                             {4.0 / 3.0, sqrt2 / 3.0},
                             3.0e-10),
                 "Simple Baffle Step midpoint complex value is incorrect")) {
        return 1;
    }

    const double midpointHz = simpleBaffleStepMidpointFrequencyHz(settings.widthMm);
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double ratio = frequencies[sampleIndex] / midpointHz;
        const std::complex<double> expectedNumerator{1.0, sqrt2 * ratio};
        const std::complex<double> expectedDenominator{1.0, ratio / sqrt2};
        const std::complex<double> expected = expectedNumerator / expectedDenominator;
        if (!require(nearComplex(response.values[sampleIndex], expected, 5.0e-10),
                     "Simple Baffle Step does not match first-order shelf reference")) {
            return 1;
        }
    }

    const BaffleResponse simpleWithoutDiameter = response;
    const BaffleResponse simpleWithDiameter = calculateBaffleResponse(settings, 17.0);
    if (!require(responsesNear(simpleWithDiameter, simpleWithoutDiameter, 0.0),
                 "Simple Baffle Step must ignore effective driver diameter exactly")) {
        return 1;
    }

    settings.widthMm = 231.0;
    response = calculateBaffleResponse(settings);
    const double lowMagnitude = std::abs(response.values.front());
    const double highMagnitude = std::abs(response.values.back());
    if (!require(near(simpleBaffleStepMidpointFrequencyHz(231.0), 115.0 / 0.231, 1.0e-12),
                 "231 mm midpoint must use the exact 115/W engineering convention") ||
        !require(lowMagnitude > 1.0 && lowMagnitude < 1.01,
                 "231 mm Simple Baffle Step must be near 0 dB at 20 Hz") ||
        !require(highMagnitude > 1.99 && highMagnitude < 2.0,
                 "231 mm Simple Baffle Step must approach +6.02 dB at high frequency") ||
        !require(near(simpleBaffleStepMidpointFrequencyHz(400.0),
                      simpleBaffleStepMidpointFrequencyHz(200.0) / 2.0,
                      1.0e-12),
                 "doubling baffle width must halve the Simple-Baffle midpoint")) {
        return 1;
    }

    for (double invalidWidth : {0.0,
                                -1.0,
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity()}) {
        settings.widthMm = invalidWidth;
        response = calculateBaffleResponse(settings);
        if (!require(response.status == BaffleResponseStatus::InvalidParameters,
                     "invalid Simple-Baffle width must report InvalidParameters") ||
            !require(allUnity(response), "invalid Simple Baffle Step must safely bypass to unity") ||
            !require(simpleBaffleStepMidpointFrequencyHz(invalidWidth) == 0.0,
                     "invalid width must not produce a midpoint frequency")) {
            return 1;
        }
    }

    // Stage 2: 231 x 900 mm, offset driver, 200 proportional edge sources.
    BaffleSettings rectangular = rectangularSettings();
    const BaffleResponse rectangular200 = calculateBaffleResponse(rectangular);
    if (!require(rectangular200.status == BaffleResponseStatus::Valid,
                 "valid Rectangular Edge Diffraction geometry must be supported") ||
        !require(rectangular200.plottable(), "valid rectangular response must be plottable") ||
        !require(allFinite(rectangular200), "rectangular response must be finite") ||
        !require(std::abs(magnitudeDb(rectangular200.values.front())) < 0.15,
                 "rectangular response must remain close to the 0 dB LF reference at 20 Hz")) {
        return 1;
    }

    for (const std::complex<double>& value : rectangular200.values) {
        const double magnitude = std::abs(value);
        if (!require(magnitude >= 1.0 - 1.0e-12 && magnitude <= 3.0 + 1.0e-12,
                     "normalized rectangular response must obey the |2-edgeSum| magnitude bounds")) {
            return 1;
        }
    }

    // Patch 194: Dm is the transient effective piston diameter. Missing,
    // invalid, non-finite or edge-touching disks must use the exact Patch-193
    // point-source response rather than bypassing the whole Baffle stage.
    for (double fallbackDiameterCm : {0.0,
                                      -1.0,
                                      std::numeric_limits<double>::quiet_NaN(),
                                      std::numeric_limits<double>::infinity(),
                                      18.0}) {
        const BaffleResponse fallback =
            calculateBaffleResponse(rectangular, fallbackDiameterCm);
        if (!require(responsesNear(fallback, rectangular200, 0.0),
                     "invalid/edge-touching Dm must fall back exactly to the point-source response")) {
            return 1;
        }
    }

    const BaffleResponse almostPoint = calculateBaffleResponse(rectangular, 1.0e-7);
    if (!require(almostPoint.status == BaffleResponseStatus::Valid,
                 "very small finite piston must remain valid") ||
        !require(maxMagnitudeDifferenceDb(almostPoint, rectangular200) < 1.0e-7,
                 "very small finite piston must converge to the point-source response")) {
        return 1;
    }

    // Offline finite-piston validation geometry from the Patch-193 supplement.
    BaffleSettings zrt = rectangular;
    zrt.heightMm = 965.0;
    zrt.driverXmm = 115.5;
    zrt.driverYmm = 228.6;
    constexpr double ZrtEffectiveDiameterCm = 13.81976597885342; // 2*sqrt(150/pi) cm
    const BaffleResponse zrtPoint = calculateBaffleResponse(zrt, 0.0);
    const BaffleResponse zrtFinite = calculateBaffleResponse(zrt, ZrtEffectiveDiameterCm);
    if (!require(zrtFinite.status == BaffleResponseStatus::Valid,
                 "valid ZRT finite piston must produce a valid response") ||
        !require(allFinite(zrtFinite),
                 "valid ZRT finite piston must be finite on all 150 points") ||
        !require(std::abs(magnitudeDb(zrtFinite.values.front()) -
                          magnitudeDb(zrtPoint.values.front())) < 0.02,
                 "finite piston must preserve the Stage-2 LF reference") ||
        !require(magnitudeRangeDb(zrtFinite, 2000.0, 10000.0) <
                     magnitudeRangeDb(zrtPoint, 2000.0, 10000.0) * 0.25,
                 "finite piston must strongly reduce ZRT high-frequency diffraction ripple")) {
        return 1;
    }

    // Product contract retained from the former LF-candidate diagnostic test:
    // Free-field Rectangular magnitude uses the accepted width-anchored n=2
    // law, while the raw rectangular phase remains unchanged. Historical
    // sqrt(W*H), n=1 and n=1.5 candidates are intentionally not regression
    // contracts anymore.
    const BaffleResponse zrtRawPoint =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(zrt, 0.0);
    BaffleSettings zrtSimpleSettings = zrt;
    zrtSimpleSettings.model = BaffleModel::SimpleBaffleStep;
    const BaffleResponse zrtSimple = calculateBaffleResponse(zrtSimpleSettings, 0.0);
    const double zrtMidpointHz = simpleBaffleStepMidpointFrequencyHz(zrt.widthMm);
    if (!require(zrtRawPoint.status == BaffleResponseStatus::Valid &&
                     zrtSimple.status == BaffleResponseStatus::Valid &&
                     zrtMidpointHz > 0.0,
                 "productive LF-hybrid reference setup must be valid")) {
        return 1;
    }
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double rawMagnitude = std::abs(zrtRawPoint.values[sampleIndex]);
        const double simpleMagnitude = std::abs(zrtSimple.values[sampleIndex]);
        if (!require(rawMagnitude > 0.0 && simpleMagnitude > 0.0,
                     "productive LF-hybrid reference magnitude must be positive")) {
            return 1;
        }
        const double ratio = frequencies[sampleIndex] / zrtMidpointHz;
        const double squaredRatio = ratio * ratio;
        const double weight = squaredRatio / (1.0 + squaredRatio);
        const double expectedDb =
            20.0 * std::log10(simpleMagnitude) +
            weight * (20.0 * std::log10(rawMagnitude) -
                      20.0 * std::log10(simpleMagnitude));
        const double expectedMagnitude = std::pow(10.0, expectedDb / 20.0);
        const std::complex<double> expected =
            zrtRawPoint.values[sampleIndex] * (expectedMagnitude / rawMagnitude);
        if (!require(nearComplex(zrtPoint.values[sampleIndex], expected, 5.0e-10),
                     "Free-field Rectangular response changed the productive width-anchored n=2 law")) {
            return 1;
        }
    }

    // M=73 production sampling is compared with an independent M=145 coherent
    // average assembled only from the public Patch-193 point-source path.
    const BaffleResponse zrtFinite145 =
        finitePistonReferenceFromPointSources(zrt, ZrtEffectiveDiameterCm, 145);
    if (!require(zrtFinite145.status == BaffleResponseStatus::Valid,
                 "M=145 finite-piston reference must be valid") ||
        !require(maxMagnitudeDifferenceDb(zrtFinite, zrtFinite145) < 0.03,
                 "M=73 finite-piston response must be converged within 0.03 dB")) {
        return 1;
    }

    // Exact scale invariant on KFilter's logarithmic grid:
    // scaling every length by the grid ratio maps f[i] to f[i+1].
    BaffleSettings scaled = rectangular;
    scaled.widthMm *= KFilterFrequencyStep;
    scaled.heightMm *= KFilterFrequencyStep;
    scaled.driverXmm *= KFilterFrequencyStep;
    scaled.driverYmm *= KFilterFrequencyStep;
    const BaffleResponse scaledResponse = calculateBaffleResponse(scaled);
    if (!require(scaledResponse.status == BaffleResponseStatus::Valid,
                 "scaled rectangular geometry must remain valid")) {
        return 1;
    }
    for (std::size_t sampleIndex = 0; sampleIndex + 1 < KFilterFrequencyCount; ++sampleIndex) {
        if (!require(nearComplex(scaledResponse.values[sampleIndex],
                                 rectangular200.values[sampleIndex + 1],
                                 2.0e-9),
                     "rectangular diffraction must scale exactly with geometry and frequency")) {
            return 1;
        }
    }

    // N=200 is the production default. N=400 must already be a close numerical
    // refinement over the whole KFilter grid for the offset reference case.
    BaffleSettings rectangular400Settings = rectangular;
    rectangular400Settings.edgeSourceCount = 400;
    const BaffleResponse rectangular400 = calculateBaffleResponse(rectangular400Settings);
    double maxConvergenceDifferenceDb = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        maxConvergenceDifferenceDb = std::max(
            maxConvergenceDifferenceDb,
            std::abs(magnitudeDb(rectangular200.values[sampleIndex]) -
                     magnitudeDb(rectangular400.values[sampleIndex])));
    }
    if (!require(rectangular400.status == BaffleResponseStatus::Valid,
                 "N=400 rectangular refinement must be valid") ||
        !require(maxConvergenceDifferenceDb < 0.05,
                 "N=200 -> N=400 rectangular response must be numerically converged")) {
        return 1;
    }

    // Position must affect the complex diffraction response without changing
    // the LF normalization contract.
    BaffleSettings symmetric = rectangular;
    symmetric.driverXmm = symmetric.widthMm / 2.0;
    symmetric.driverYmm = symmetric.heightMm / 2.0;
    const BaffleResponse symmetricResponse = calculateBaffleResponse(symmetric);
    bool positionChangedResponse = false;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        if (std::abs(symmetricResponse.values[sampleIndex] - rectangular200.values[sampleIndex]) > 1.0e-5) {
            positionChangedResponse = true;
            break;
        }
    }
    if (!require(symmetricResponse.status == BaffleResponseStatus::Valid,
                 "symmetric rectangular geometry must be valid") ||
        !require(positionChangedResponse,
                 "changing driver position must change rectangular diffraction") ||
        !require(std::abs(magnitudeDb(symmetricResponse.values.front())) < 0.15,
                 "driver offset must not destroy the rectangular LF reference")) {
        return 1;
    }

    // Patch 216 investigation: expose the four-edge decomposition. Patch 246
    // keeps this diagnostic on the unblended raw Sharp engine so its geometry
    // reference remains stable while the productive Free-field response gains
    // the LF magnitude hybrid. Omitting only the bottom edge must
    // break the top/bottom mirror symmetry of the candidate response.
    BaffleSettings floorUpper = rectangular;
    floorUpper.driverXmm = floorUpper.widthMm / 2.0;
    floorUpper.driverYmm = floorUpper.heightMm * 0.10;
    BaffleSettings floorLower = floorUpper;
    floorLower.driverYmm = floorLower.heightMm - floorUpper.driverYmm;

    const BaffleResponse floorUpperProduction = calculateBaffleResponse(floorUpper);
    const BaffleResponse floorLowerProduction = calculateBaffleResponse(floorLower);
    const BaffleResponse floorUpperRaw =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(floorUpper);
    const BaffleResponse floorLowerRaw =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(floorLower);
    const BaffleRectangularBottomEdgeDiagnostic floorUpperDiagnostic =
        calculateBaffleRectangularBottomEdgeDiagnostic(floorUpper);
    const BaffleRectangularBottomEdgeDiagnostic floorLowerDiagnostic =
        calculateBaffleRectangularBottomEdgeDiagnostic(floorLower);
    if (!require(responsesNear(floorUpperDiagnostic.freeField,
                               floorUpperRaw, 2.0e-12),
                 "bottom-edge diagnostic free-field side must preserve the unblended Sharp reference") ||
        !require(responsesNear(floorUpperRaw, floorLowerRaw, 2.0e-12),
                 "unblended free-field rectangular reference must retain vertical mirror symmetry") ||
        !require(responsesNear(floorUpperProduction, floorLowerProduction, 2.0e-12),
                 "Patch-246 productive hybrid must retain vertical mirror symmetry") ||
        !require(phaseDirectionsNear(floorUpperProduction, floorUpperRaw),
                 "Patch-246 productive LF hybrid must preserve the raw rectangular phase") ||
        !require(maxMagnitudeDifferenceDb(floorUpperDiagnostic.bottomEdgeOmitted,
                                          floorLowerDiagnostic.bottomEdgeOmitted) > 0.05,
                 "omitting only the bottom edge must break vertical mirror symmetry") ||
        !require(maxMagnitudeDifferenceDb(floorUpperDiagnostic.freeField,
                                          floorUpperDiagnostic.bottomEdgeOmitted) > 0.05,
                 "bottom-edge omission candidate must measurably differ from free field")) {
        return 1;
    }

    constexpr double FloorDiagnosticDiameterCm = 13.0;
    const BaffleResponse floorUpperFiniteProduction =
        calculateBaffleResponse(floorUpper, FloorDiagnosticDiameterCm);
    const BaffleResponse floorUpperFiniteRaw =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(
            floorUpper, FloorDiagnosticDiameterCm);
    const BaffleRectangularBottomEdgeDiagnostic floorUpperFiniteDiagnostic =
        calculateBaffleRectangularBottomEdgeDiagnostic(
            floorUpper, FloorDiagnosticDiameterCm);
    if (!require(responsesNear(floorUpperFiniteDiagnostic.freeField,
                               floorUpperFiniteRaw, 2.0e-12),
                 "finite-piston bottom-edge diagnostic must preserve the unblended Sharp reference") ||
        !require(phaseDirectionsNear(floorUpperFiniteProduction, floorUpperFiniteRaw),
                 "finite-piston Patch-246 hybrid must preserve the raw rectangular phase") ||
        !require(allFinite(floorUpperFiniteDiagnostic.bottomEdgeOmitted),
                 "finite-piston bottom-edge omission diagnostic must remain finite")) {
        return 1;
    }

    // Patch 220 production promotion: the optional rigid-floor boundary must
    // reproduce the normalized unfolded image-geometry diagnostic exactly,
    // while Free field remains the unchanged default.
    BaffleSettings rigidFloor = floorLower;
    rigidFloor.boundaryCondition =
        BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    const BaffleResponse rigidFloorProduction =
        calculateBaffleResponse(rigidFloor, FloorDiagnosticDiameterCm);
    const BaffleResponse floorLowerFiniteProduction =
        calculateBaffleResponse(floorLower, FloorDiagnosticDiameterCm);
    const BaffleRectangularRigidFloorDiagnostic rigidFloorReference =
        calculateBaffleRectangularRigidFloorDiagnostic(
            floorLower, FloorDiagnosticDiameterCm);
    if (!require(rigidFloorProduction.status == BaffleResponseStatus::Valid,
                 "rigid-floor production response must be valid for Sharp rectangular geometry") ||
        !require(responsesNear(rigidFloorProduction,
                               rigidFloorReference.imageGeometryNormalized, 2.0e-12),
                 "rigid-floor production response must match the normalized image diagnostic") ||
        !require(maxMagnitudeDifferenceDb(rigidFloorProduction, floorLowerFiniteProduction) > 0.20,
                 "rigid-floor production mode must measurably differ from Free field near the floor")) {
        return 1;
    }

    BaffleSettings floorChamfer = floorUpper;
    floorChamfer.leftEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    floorChamfer.leftChamferSetbackMm = 20.0;
    const BaffleRectangularBottomEdgeDiagnostic unsupportedFloorChamfer =
        calculateBaffleRectangularBottomEdgeDiagnostic(floorChamfer);
    if (!require(unsupportedFloorChamfer.freeField.status == BaffleResponseStatus::UnsupportedModel &&
                 unsupportedFloorChamfer.bottomEdgeOmitted.status == BaffleResponseStatus::UnsupportedModel,
                 "Patch-216 bottom-edge diagnostic must deliberately reject chamfer geometry")) {
        return 1;
    }
    floorChamfer.boundaryCondition =
        BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    const BaffleResponse unsupportedFloorChamferProduction =
        calculateBaffleResponse(floorChamfer);
    if (!require(unsupportedFloorChamferProduction.status == BaffleResponseStatus::UnsupportedModel,
                 "Patch-220 rigid-floor production mode must reject chamfer geometry") ||
        !require(allUnity(unsupportedFloorChamferProduction),
                 "unsupported rigid-floor chamfer geometry must safely bypass to unity")) {
        return 1;
    }

    // Stage-2 safety: invalid geometry bypasses only H_baffle.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int invalidCase = 0; invalidCase < 10; ++invalidCase) {
        BaffleSettings invalid = rectangular;
        switch (invalidCase) {
        case 0: invalid.widthMm = 0.0; break;
        case 1: invalid.heightMm = 0.0; break;
        case 2: invalid.driverXmm = 0.0; break;
        case 3: invalid.driverXmm = invalid.widthMm; break;
        case 4: invalid.driverYmm = 0.0; break;
        case 5: invalid.driverYmm = invalid.heightMm; break;
        case 6: invalid.driverXmm = nan; break;
        case 7: invalid.heightMm = nan; break;
        case 8: invalid.edgeSourceCount = 3; break;
        case 9: invalid.edgeSourceCount = 100001; break;
        }
        const BaffleResponse invalidResponse = calculateBaffleResponse(invalid);
        if (!require(invalidResponse.status == BaffleResponseStatus::InvalidParameters,
                     "invalid rectangular geometry must report InvalidParameters") ||
            !require(allUnity(invalidResponse),
                     "invalid rectangular geometry must safely bypass to unity")) {
            return 1;
        }
    }

    const std::complex<double> input{2.0, -3.0};
    const std::complex<double> expectedApplied = input * rectangular200.values[MidpointIndex];
    if (!require(nearComplex(applyBaffleResponseSample(rectangular200, MidpointIndex, input),
                             expectedApplied),
                 "valid rectangular sample must multiply the complex signal") ||
        !require(nearComplex(applyBaffleResponseSample(rectangular200, KFilterFrequencyCount, input),
                             input),
                 "out-of-range baffle sample must bypass")) {
        return 1;
    }

    BaffleResponse invalidResponse = rectangular200;
    invalidResponse.status = BaffleResponseStatus::InvalidParameters;
    if (!require(nearComplex(applyBaffleResponseSample(invalidResponse, MidpointIndex, input), input),
                 "invalid baffle response must bypass only its own stage")) {
        return 1;
    }

    // Cache semantics: disabled settings are neutral; Stage 1 ignores Stage-2
    // geometry and Dm; Stage 2 invalidates on geometry and effective-Dm changes.
    BaffleResponseCache cache;
    BaffleSettings cachedSettings;
    cache.responseFor(cachedSettings);
    const std::uint64_t initialGeneration = cache.generation();
    cachedSettings.widthMm = 333.0;
    cache.responseFor(cachedSettings);
    if (!require(cache.generation() == initialGeneration,
                 "disabled geometry changes must not invalidate neutral cache")) {
        return 1;
    }

    cachedSettings.enabled = true;
    cache.responseFor(cachedSettings, 11.0);
    const std::uint64_t enabledGeneration = cache.generation();
    if (!require(enabledGeneration == initialGeneration + 1,
                 "enabling baffle processing must invalidate cache")) {
        return 1;
    }

    cachedSettings.showResponseInPlot = true;
    cachedSettings.heightMm = 900.0;
    cachedSettings.driverXmm = 100.0;
    cachedSettings.driverYmm = 300.0;
    cachedSettings.edgeSourceCount = 400;
    cache.responseFor(cachedSettings, 17.0);
    if (!require(cache.generation() == enabledGeneration,
                 "Stage-1 cache must ignore visibility, Dm and unused Stage-2 geometry")) {
        return 1;
    }

    cachedSettings.widthMm = 334.0;
    cache.responseFor(cachedSettings, 17.0);
    const std::uint64_t widthGeneration = cache.generation();
    if (!require(widthGeneration == enabledGeneration + 1,
                 "Stage-1 width change must invalidate cache")) {
        return 1;
    }

    cachedSettings.boundaryCondition =
        BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    cache.responseFor(cachedSettings, 17.0);
    if (!require(cache.generation() == widthGeneration,
                 "Simple Baffle Step cache must ignore stored rectangular boundary history")) {
        return 1;
    }
    cachedSettings.boundaryCondition = BaffleBoundaryCondition::FreeField;

    cachedSettings.model = BaffleModel::RectangularEdgeDiffraction;
    constexpr double CachedDiameterCm = 13.0;
    const BaffleResponse& rectangularCached = cache.responseFor(cachedSettings, CachedDiameterCm);
    const std::uint64_t rectangularGeneration = cache.generation();
    if (!require(rectangularGeneration == widthGeneration + 1,
                 "baffle model change must invalidate cache") ||
        !require(rectangularCached.status == BaffleResponseStatus::Valid,
                 "cache must calculate the Stage-2 rectangular response")) {
        return 1;
    }

    cachedSettings.showResponseInPlot = false;
    cache.responseFor(cachedSettings, CachedDiameterCm);
    if (!require(cache.generation() == rectangularGeneration,
                 "Stage-2 diagnostic visibility must not invalidate transfer cache")) {
        return 1;
    }

    cachedSettings.driverXmm += 1.0;
    cache.responseFor(cachedSettings, CachedDiameterCm);
    const std::uint64_t positionGeneration = cache.generation();
    if (!require(positionGeneration == rectangularGeneration + 1,
                 "Stage-2 driver position must invalidate transfer cache")) {
        return 1;
    }

    cachedSettings.edgeSourceCount = 200;
    cache.responseFor(cachedSettings, CachedDiameterCm);
    const std::uint64_t edgeCountGeneration = cache.generation();
    if (!require(edgeCountGeneration == positionGeneration + 1,
                 "Stage-2 edge-source count must invalidate transfer cache")) {
        return 1;
    }

    // Stage 3B cache semantics: inactive stored chamfer widths are edit history
    // only. Selecting Chamfer45 changes the transfer and an active setback
    // change must then invalidate it.
    cachedSettings.leftChamferSetbackMm = 30.0;
    cache.responseFor(cachedSettings, CachedDiameterCm);
    if (!require(cache.generation() == edgeCountGeneration,
                 "Sharp side must ignore inactive stored chamfer width")) {
        return 1;
    }
    cachedSettings.leftEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    cache.responseFor(cachedSettings, CachedDiameterCm);
    const std::uint64_t chamferGeneration = cache.generation();
    if (!require(chamferGeneration == edgeCountGeneration + 1,
                 "selecting Chamfer45 must invalidate the rectangular transfer cache")) {
        return 1;
    }
    cachedSettings.leftChamferSetbackMm = 35.0;
    cache.responseFor(cachedSettings, CachedDiameterCm);
    const std::uint64_t chamferWidthGeneration = cache.generation();
    if (!require(chamferWidthGeneration == chamferGeneration + 1,
                 "active chamfer width change must invalidate the transfer cache")) {
        return 1;
    }

    cache.responseFor(cachedSettings, CachedDiameterCm + 1.0);
    if (!require(cache.generation() == chamferWidthGeneration + 1,
                 "Stage-2/3B effective Dm change must invalidate transfer cache")) {
        return 1;
    }
    const std::uint64_t diameterGeneration = cache.generation();

    cachedSettings.leftEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
    cache.responseFor(cachedSettings, CachedDiameterCm + 1.0);
    const std::uint64_t sharpGeneration = cache.generation();
    if (!require(sharpGeneration == diameterGeneration + 1,
                 "returning from Chamfer45 to Sharp must invalidate the transfer cache")) {
        return 1;
    }

    cachedSettings.boundaryCondition =
        BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    const BaffleResponse& rigidFloorCached =
        cache.responseFor(cachedSettings, CachedDiameterCm + 1.0);
    if (!require(cache.generation() == sharpGeneration + 1,
                 "changing rectangular boundary condition must invalidate the transfer cache") ||
        !require(rigidFloorCached.status == BaffleResponseStatus::Valid,
                 "cache must calculate the Sharp rigid-floor response")) {
        return 1;
    }

    std::cout << "baffle response smoke test passed\n";
    return 0;
}
