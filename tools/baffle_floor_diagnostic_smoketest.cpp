/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleresponse.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>

namespace
{
double maxComplexDifference(const BaffleResponse& a, const BaffleResponse& b)
{
    double maximum = 0.0;
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        maximum = std::max(maximum, std::abs(a.values[i] - b.values[i]));
    }
    return maximum;
}

double magnitudeDb(const std::complex<double>& value)
{
    return 20.0 * std::log10(std::abs(value));
}
}

int main()
{
    BaffleSettings settings;
    settings.enabled = true;
    settings.model = BaffleModel::RectangularEdgeDiffraction;
    settings.widthMm = 231.0;
    settings.heightMm = 965.0;
    settings.driverXmm = settings.widthMm / 2.0;
    settings.driverYmm = settings.heightMm * 0.90;
    settings.edgeSourceCount = 200;

    constexpr double EffectiveDiameterCm = 13.81976597885342;
    const BaffleResponse production = calculateBaffleResponse(settings, EffectiveDiameterCm);
    const BaffleResponse rawFreeField =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(
            settings, EffectiveDiameterCm);
    const BaffleRectangularBottomEdgeDiagnostic patch216 =
        calculateBaffleRectangularBottomEdgeDiagnostic(settings, EffectiveDiameterCm);
    const BaffleRectangularRigidFloorDiagnostic floor =
        calculateBaffleRectangularRigidFloorDiagnostic(settings, EffectiveDiameterCm);

    if (production.status != BaffleResponseStatus::Valid ||
        patch216.freeField.status != BaffleResponseStatus::Valid ||
        patch216.bottomEdgeOmitted.status != BaffleResponseStatus::Valid ||
        floor.freeField.status != BaffleResponseStatus::Valid ||
        floor.bottomEdgeOmitted.status != BaffleResponseStatus::Valid ||
        floor.imageGeometryRealSource.status != BaffleResponseStatus::Valid ||
        floor.imageGeometryMirrorSource.status != BaffleResponseStatus::Valid ||
        floor.imageGeometryNormalized.status != BaffleResponseStatus::Valid ||
        floor.imageGeometryRaw.status != BaffleResponseStatus::Valid) {
        std::cerr << "floor diagnostic did not return valid reference responses\n";
        return 1;
    }

    if (maxComplexDifference(rawFreeField, floor.freeField) > 1.0e-12 ||
        maxComplexDifference(patch216.freeField, floor.freeField) > 1.0e-12 ||
        maxComplexDifference(patch216.bottomEdgeOmitted, floor.bottomEdgeOmitted) > 1.0e-12) {
        std::cerr << "Patch-219 diagnostic changed a Patch-216/raw reference path\n";
        return 1;
    }

    // Patch 246 changes only the productive Free-field magnitude; the old
    // floor-boundary diagnostics deliberately remain on the unblended raw
    // rectangular engine. Verify that the promoted hybrid keeps raw phase.
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        const double productionMagnitude = std::abs(production.values[i]);
        const double rawMagnitude = std::abs(rawFreeField.values[i]);
        if (productionMagnitude <= 0.0 || rawMagnitude <= 0.0 ||
            std::abs(production.values[i] / productionMagnitude -
                     rawFreeField.values[i] / rawMagnitude) > 2.0e-12) {
            std::cerr << "Patch-246 Free-field hybrid changed raw rectangular phase\n";
            return 1;
        }
    }

    // Patch 219 deliberately defines candidate C from ONE source in the
    // unfolded geometry. The independently evaluated mirror remains diagnostic
    // only and must not bias C through finite quadrature asymmetry.
    if (maxComplexDifference(floor.imageGeometryNormalized,
                             floor.imageGeometryRealSource) > 1.0e-15) {
        std::cerr << "normalized image geometry is no longer the single-source unfolded response\n";
        return 1;
    }

    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        if (std::abs(floor.imageGeometryRaw.values[i] -
                     2.0 * floor.imageGeometryNormalized.values[i]) > 1.0e-12) {
            std::cerr << "raw rigid-floor image pair is not exactly 2x normalized response\n";
            return 1;
        }
    }

    // For the deliberately near-floor reference placement, the unfolded image
    // geometry must preserve the LF normalization much better than the simple
    // Patch-216 bottom-edge deletion candidate. This is a regression guard for
    // this investigation geometry, not a universal acoustic law.
    const double bottomLfDelta = std::abs(
        magnitudeDb(floor.bottomEdgeOmitted.values.front()) -
        magnitudeDb(floor.freeField.values.front()));
    const double imageLfDelta = std::abs(
        magnitudeDb(floor.imageGeometryNormalized.values.front()) -
        magnitudeDb(floor.freeField.values.front()));
    if (!(imageLfDelta < bottomLfDelta * 0.10)) {
        std::cerr << "image geometry no longer improves the reference LF normalization\n";
        return 1;
    }

    // The separately evaluated mirror source exposes finite edge quadrature
    // asymmetry. For a point source it must converge strongly as N increases.
    BaffleSettings convergence = settings;
    convergence.driverXmm = convergence.widthMm * 0.35;
    convergence.edgeSourceCount = 100;
    const BaffleRectangularRigidFloorDiagnostic coarse =
        calculateBaffleRectangularRigidFloorDiagnostic(convergence, 0.0);
    convergence.edgeSourceCount = 800;
    const BaffleRectangularRigidFloorDiagnostic fine =
        calculateBaffleRectangularRigidFloorDiagnostic(convergence, 0.0);
    if (coarse.imageGeometryRealSource.status != BaffleResponseStatus::Valid ||
        coarse.imageGeometryMirrorSource.status != BaffleResponseStatus::Valid ||
        fine.imageGeometryRealSource.status != BaffleResponseStatus::Valid ||
        fine.imageGeometryMirrorSource.status != BaffleResponseStatus::Valid) {
        std::cerr << "mirror symmetry convergence reference failed\n";
        return 1;
    }
    const double coarseMismatch = maxComplexDifference(coarse.imageGeometryRealSource,
                                                       coarse.imageGeometryMirrorSource);
    const double fineMismatch = maxComplexDifference(fine.imageGeometryRealSource,
                                                     fine.imageGeometryMirrorSource);
    if (!(fineMismatch < coarseMismatch * 0.20)) {
        std::cerr << "point-source mirror quadrature did not converge as expected\n";
        return 1;
    }

    BaffleSettings unsupported = settings;
    unsupported.model = BaffleModel::SimpleBaffleStep;
    if (calculateBaffleRectangularRigidFloorDiagnostic(unsupported, EffectiveDiameterCm)
            .freeField.status != BaffleResponseStatus::UnsupportedModel) {
        std::cerr << "simple baffle step was not rejected by floor diagnostic\n";
        return 1;
    }

    unsupported = settings;
    unsupported.leftEdgeTreatment = BaffleSideEdgeTreatment::Chamfer45;
    unsupported.leftChamferSetbackMm = 20.0;
    if (calculateBaffleRectangularRigidFloorDiagnostic(unsupported, EffectiveDiameterCm)
            .freeField.status != BaffleResponseStatus::UnsupportedModel) {
        std::cerr << "chamfer was not rejected by floor diagnostic\n";
        return 1;
    }

    unsupported = settings;
    unsupported.enabled = false;
    if (calculateBaffleRectangularRigidFloorDiagnostic(unsupported, EffectiveDiameterCm)
            .freeField.status != BaffleResponseStatus::Neutral) {
        std::cerr << "disabled baffle did not remain neutral in floor diagnostic\n";
        return 1;
    }

    std::cout << "baffle floor diagnostic smoke test passed\n";
    return 0;
}
