/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleresponse.h"
#include "floorreflectionresponse.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>

namespace
{
constexpr double SixDb = 20.0 * std::log10(2.0);

double maxComplexDifference(const BaffleResponse& a, const BaffleResponse& b)
{
    double maximum = 0.0;
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        maximum = std::max(maximum, std::abs(a.values[i] - b.values[i]));
    }
    return maximum;
}
}

int main()
{
    BaffleSettings freeSettings;
    freeSettings.enabled = true;
    freeSettings.model = BaffleModel::RectangularEdgeDiffraction;
    freeSettings.widthMm = 231.0;
    freeSettings.heightMm = 965.0;
    freeSettings.driverXmm = freeSettings.widthMm / 2.0;
    freeSettings.driverYmm = 245.0; // source height = 720 mm above rigid floor
    freeSettings.boundaryCondition = BaffleBoundaryCondition::FreeField;
    freeSettings.edgeSourceCount = 200;

    BaffleSettings rigidSettings = freeSettings;
    rigidSettings.boundaryCondition =
        BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;

    constexpr double EffectiveDiameterCm = 13.81976597885342;
    const BaffleResponse freeBaffle = calculateBaffleResponse(freeSettings,
                                                               EffectiveDiameterCm);
    const BaffleResponse rigidBaffle = calculateBaffleResponse(rigidSettings,
                                                                EffectiveDiameterCm);
    const BaffleRectangularRigidFloorDiagnostic baffleDiagnostic =
        calculateBaffleRectangularRigidFloorDiagnostic(freeSettings,
                                                       EffectiveDiameterCm);

    const double sourceHeightM =
        (freeSettings.heightMm - freeSettings.driverYmm) / 1000.0;
    if (std::abs(sourceHeightM - 0.72) > 1.0e-15) {
        std::cerr << "source-height derivation from Baffle Y changed\n";
        return 1;
    }

    const FloorReflectionGeometry placement{sourceHeightM, 1.05, 2.50};
    const FloorReflectionResponse floor =
        calculateIdealRigidFloorReflectionResponse(placement);
    const FloorReflectionResponse noReflection =
        calculateFloorReflectionResponseWithConstantCoefficient(
            placement, {0.0, 0.0});

    if (freeBaffle.status != BaffleResponseStatus::Valid ||
        rigidBaffle.status != BaffleResponseStatus::Valid ||
        baffleDiagnostic.imageGeometryNormalized.status != BaffleResponseStatus::Valid ||
        baffleDiagnostic.imageGeometryRaw.status != BaffleResponseStatus::Valid ||
        floor.status != FloorReflectionResponseStatus::Valid ||
        noReflection.status != FloorReflectionResponseStatus::Valid) {
        std::cerr << "Stage-F2 reference response is not valid\n";
        return 1;
    }

    // The production rigid-floor baffle path must be exactly the normalized
    // diffraction shape from Patch 220/221. This is the central no-double-count
    // contract before H_floor is multiplied in a separate placement stage.
    if (maxComplexDifference(rigidBaffle,
                             baffleDiagnostic.imageGeometryNormalized) > 1.0e-12) {
        std::cerr << "rigid-floor production response is no longer normalized image geometry\n";
        return 1;
    }

    double maximumRawExtraGainErrorDb = 0.0;
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        const std::complex<double> combined = rigidBaffle.values[i] * floor.values[i];
        const std::complex<double> bypassCombined =
            rigidBaffle.values[i] * noReflection.values[i];
        const std::complex<double> deliberatelyDoubleCounted =
            baffleDiagnostic.imageGeometryRaw.values[i] * floor.values[i];

        // Gamma=0 is a neutral placement stage and must leave H_baffle exactly.
        if (std::abs(bypassCombined - rigidBaffle.values[i]) > 1.0e-12) {
            std::cerr << "neutral floor placement changed rigid diffraction\n";
            return 1;
        }

        // The old/raw coherent image pair already contains factor 2. Using it
        // together with H_floor would therefore add exactly another +6.0206 dB.
        if (std::abs(combined) <= 0.0 || std::abs(deliberatelyDoubleCounted) <= 0.0) {
            std::cerr << "unexpected exact zero in Stage-F2 reference\n";
            return 1;
        }
        const double extraGainDb = 20.0 * std::log10(
            std::abs(deliberatelyDoubleCounted) / std::abs(combined));
        maximumRawExtraGainErrorDb = std::max(
            maximumRawExtraGainErrorDb, std::abs(extraGainDb - SixDb));
    }

    if (maximumRawExtraGainErrorDb > 1.0e-10) {
        std::cerr << "raw image pair is no longer exactly +6.0206 dB over normalized combination\n";
        return 1;
    }

    // Stage F2 is diagnostic only: verify that the established free-field path
    // itself has not been modified while introducing this cross-stage test.
    BaffleSettings freeAgain = rigidSettings;
    freeAgain.boundaryCondition = BaffleBoundaryCondition::FreeField;
    const BaffleResponse freeRegression = calculateBaffleResponse(freeAgain,
                                                                   EffectiveDiameterCm);
    if (freeRegression.status != BaffleResponseStatus::Valid ||
        maxComplexDifference(freeBaffle, freeRegression) > 1.0e-15) {
        std::cerr << "free-field baffle regression changed\n";
        return 1;
    }

    std::cout << "floor/baffle Stage-F2 combination smoke test passed\n";
    return 0;
}
