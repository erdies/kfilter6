/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorreflectionprocessing.h"
#include "floorsurfacemodel.h"

#include <cmath>
#include <complex>
#include <iostream>
#include <limits>

namespace
{
bool require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

bool near(double left, double right, double tolerance = 1.0e-12)
{
    return std::abs(left - right) <= tolerance;
}
}

int main()
{
    FloorReflectionSettings settings;
    BaffleSettings baffle;
    baffle.heightMm = 965.0;
    baffle.driverYmm = 245.0;

    // Disabled state must be an exact neutral bypass, even with otherwise valid
    // geometry retained for later editing.
    FloorReflectionResponse response =
        calculateFloorReflectionProductResponse(settings, baffle);
    if (!require(response.status == FloorReflectionResponseStatus::Neutral,
                 "disabled Floor Reflection was not neutral")) {
        return 1;
    }
    const std::complex<double> probe{0.75, -0.25};
    if (!require(applyFloorReflectionResponseSample(response, 75, probe) == probe,
                 "disabled Floor Reflection changed the signal")) {
        return 1;
    }

    settings.enabled = true;
    response = calculateFloorReflectionProductResponse(settings, baffle);
    const FloorReflectionResponse expected =
        calculateIdealRigidFloorReflectionResponse({0.72, 1.05, 2.50});
    if (!require(response.status == FloorReflectionResponseStatus::Valid,
                 "valid product Floor Reflection was rejected") ||
        !require(near(response.geometry.directDistanceM, expected.geometry.directDistanceM) &&
                 near(response.geometry.imageDistanceM, expected.geometry.imageDistanceM) &&
                 near(response.geometry.pathDifferenceM, expected.geometry.pathDifferenceM),
                 "source-height derivation did not reproduce the F0 reference geometry")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        if (!require(std::abs(response.values[index] - expected.values[index]) <= 1.0e-12,
                     "product response differs from ideal-rigid F0 reference")) {
            return 1;
        }
    }

    const std::complex<double> applied =
        applyFloorReflectionResponseSample(response, 75, probe);
    if (!require(std::abs(applied - probe * response.values[75]) <= 1.0e-12,
                 "H_floor complex multiplication mismatch")) {
        return 1;
    }

    // Patch 229: the productive Miki reference must be exactly the same
    // low-level material model that Patch 228 validated diagnostically.
    settings.surfacePreset = FloorSurfacePreset::MikiReference10mm100k;
    const FloorReflectionResponse porousProduct =
        calculateFloorReflectionProductResponse(settings, baffle);
    const FloorReflectionResponse porousReference =
        calculateFloorReflectionResponseWithSurfaceModel(
            {0.72, 1.05, 2.50},
            mikiPorousRigidBackingDefinition(
                KFilterMikiReferenceFloorThicknessM,
                KFilterMikiReferenceFloorFlowResistivityPaSPerM2));
    if (!require(porousProduct.status == FloorReflectionResponseStatus::Valid &&
                 porousReference.status == FloorReflectionResponseStatus::Valid,
                 "productive Miki reference was rejected")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        if (!require(std::abs(porousProduct.values[index] - porousReference.values[index]) <= 1.0e-12,
                     "productive Miki reference differs from Patch-228 material core")) {
            return 1;
        }
    }
    if (!require(std::abs(porousProduct.values[60] - expected.values[60]) > 1.0e-3,
                 "Miki reference unexpectedly collapsed to rigid response")) {
        return 1;
    }
    settings.surfacePreset = FloorSurfacePreset::HardRigid;

    // Cabinet elevation contributes directly to source height.
    settings.cabinetBottomAboveFloorMm = 120.0;
    response = calculateFloorReflectionProductResponse(settings, baffle);
    const FloorReflectionResponse elevated =
        calculateIdealRigidFloorReflectionResponse({0.84, 1.05, 2.50});
    if (!require(response.status == FloorReflectionResponseStatus::Valid &&
                 near(response.geometry.pathDifferenceM, elevated.geometry.pathDifferenceM),
                 "cabinet-bottom elevation was not included in source height")) {
        return 1;
    }

    // Floor reflection is placement physics and must not be gated by Baffle
    // processing itself or by its FreeField/RigidFloorContact selection.
    settings.cabinetBottomAboveFloorMm = 0.0;
    baffle.enabled = false;
    baffle.boundaryCondition = BaffleBoundaryCondition::FreeField;
    const FloorReflectionResponse independentFree =
        calculateFloorReflectionProductResponse(settings, baffle);
    baffle.enabled = true;
    baffle.boundaryCondition = BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    const FloorReflectionResponse independentRigid =
        calculateFloorReflectionProductResponse(settings, baffle);
    if (!require(independentFree.status == FloorReflectionResponseStatus::Valid &&
                 independentRigid.status == FloorReflectionResponseStatus::Valid,
                 "Floor Reflection was incorrectly gated by Baffle enable/boundary state")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        if (!require(std::abs(independentFree.values[index] - independentRigid.values[index]) <= 1.0e-12,
                     "Baffle boundary condition leaked into H_floor")) {
            return 1;
        }
    }

    // Product integration requires configured vertical baffle geometry because
    // source height is derived rather than stored redundantly.
    baffle.heightMm = 0.0;
    response = calculateFloorReflectionProductResponse(settings, baffle);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters &&
                 applyFloorReflectionResponseSample(response, 75, probe) == probe,
                 "missing baffle height did not bypass H_floor safely")) {
        return 1;
    }

    baffle.heightMm = 965.0;
    baffle.driverYmm = 1000.0;
    response = calculateFloorReflectionProductResponse(settings, baffle);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "driver below cabinet bottom was accepted")) {
        return 1;
    }

    baffle.driverYmm = 245.0;
    settings.listenerHeightAboveFloorMm = std::numeric_limits<double>::quiet_NaN();
    response = calculateFloorReflectionProductResponse(settings, baffle);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "non-finite placement metadata was accepted")) {
        return 1;
    }

    settings = FloorReflectionSettings{};
    settings.enabled = true;
    settings.surfacePreset = static_cast<FloorSurfacePreset>(999);
    response = calculateFloorReflectionProductResponse(settings, baffle);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "unsupported surface preset was accepted")) {
        return 1;
    }

    // Cache keys include only transfer-relevant placement + vertical geometry.
    settings = FloorReflectionSettings{};
    FloorReflectionResponseCache cache;
    (void)cache.responseFor(settings, baffle);
    const std::uint64_t neutralGeneration = cache.generation();
    settings.horizontalDistanceMm = 3333.0;
    (void)cache.responseFor(settings, baffle);
    if (!require(cache.generation() == neutralGeneration,
                 "disabled metadata edit invalidated neutral cache")) {
        return 1;
    }
    settings.enabled = true;
    (void)cache.responseFor(settings, baffle);
    const std::uint64_t enabledGeneration = cache.generation();
    settings.horizontalDistanceMm = 3000.0;
    (void)cache.responseFor(settings, baffle);
    if (!require(cache.generation() == enabledGeneration + 1,
                 "enabled placement edit did not invalidate cache")) {
        return 1;
    }
    const std::uint64_t placementGeneration = cache.generation();
    settings.surfacePreset = FloorSurfacePreset::MikiReference10mm100k;
    (void)cache.responseFor(settings, baffle);
    if (!require(cache.generation() == placementGeneration + 1,
                 "surface-preset edit did not invalidate cache")) {
        return 1;
    }

    std::cout << "floor reflection product-processing smoke test passed\n";
    return 0;
}
