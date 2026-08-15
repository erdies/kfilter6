/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorreflectionprocessing.h"

#include "floorsurfacemodel.h"

#include <cmath>

namespace
{
constexpr double MillimetresPerMetre = 1000.0;

bool finiteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool transferEquivalent(const FloorReflectionSettings& left,
                        const FloorReflectionSettings& right,
                        double leftBaffleHeightMm,
                        double leftDriverYmm,
                        double rightBaffleHeightMm,
                        double rightDriverYmm)
{
    if (left.enabled != right.enabled) {
        return false;
    }

    // Disabled Floor Reflection is neutral regardless of retained placement
    // metadata. Editing those values while disabled must not invalidate H_floor.
    if (!left.enabled) {
        return true;
    }

    return left.cabinetBottomAboveFloorMm == right.cabinetBottomAboveFloorMm &&
           left.listenerHeightAboveFloorMm == right.listenerHeightAboveFloorMm &&
           left.horizontalDistanceMm == right.horizontalDistanceMm &&
           left.surfacePreset == right.surfacePreset &&
           leftBaffleHeightMm == rightBaffleHeightMm &&
           leftDriverYmm == rightDriverYmm;
}

FloorReflectionResponse invalidProductResponse()
{
    FloorReflectionResponse response;
    response.status = FloorReflectionResponseStatus::InvalidParameters;
    return response;
}
}

FloorReflectionResponse calculateFloorReflectionProductResponse(
    const FloorReflectionSettings& settings,
    const BaffleSettings& baffleSettings)
{
    if (!settings.enabled) {
        return {};
    }

    if ((settings.surfacePreset != FloorSurfacePreset::HardRigid &&
         settings.surfacePreset != FloorSurfacePreset::MikiReference10mm100k) ||
        !finiteNonNegative(settings.cabinetBottomAboveFloorMm) ||
        !finiteNonNegative(settings.listenerHeightAboveFloorMm) ||
        !finiteNonNegative(settings.horizontalDistanceMm) ||
        !std::isfinite(baffleSettings.heightMm) ||
        !std::isfinite(baffleSettings.driverYmm) ||
        baffleSettings.heightMm <= 0.0 ||
        baffleSettings.driverYmm < 0.0 ||
        baffleSettings.driverYmm > baffleSettings.heightMm) {
        return invalidProductResponse();
    }

    const double sourceHeightMm = settings.cabinetBottomAboveFloorMm +
                                  baffleSettings.heightMm -
                                  baffleSettings.driverYmm;
    if (!finiteNonNegative(sourceHeightMm)) {
        return invalidProductResponse();
    }

    const FloorReflectionGeometry geometry{
        sourceHeightMm / MillimetresPerMetre,
        settings.listenerHeightAboveFloorMm / MillimetresPerMetre,
        settings.horizontalDistanceMm / MillimetresPerMetre};

    if (settings.surfacePreset == FloorSurfacePreset::HardRigid) {
        return calculateIdealRigidFloorReflectionResponse(geometry);
    }

    return calculateFloorReflectionResponseWithSurfaceModel(
        geometry,
        mikiPorousRigidBackingDefinition(
            KFilterMikiReferenceFloorThicknessM,
            KFilterMikiReferenceFloorFlowResistivityPaSPerM2));
}

std::complex<double> applyFloorReflectionResponseSample(
    const FloorReflectionResponse& response,
    std::size_t sampleIndex,
    const std::complex<double>& signal)
{
    if (response.status != FloorReflectionResponseStatus::Valid ||
        sampleIndex >= KFilterFrequencyCount) {
        return signal;
    }

    return signal * response.values[sampleIndex];
}

const FloorReflectionResponse& FloorReflectionResponseCache::responseFor(
    const FloorReflectionSettings& settings,
    const BaffleSettings& baffleSettings)
{
    if (!m_valid ||
        !transferEquivalent(m_cachedSettings, settings,
                            m_cachedBaffleHeightMm, m_cachedDriverYmm,
                            baffleSettings.heightMm, baffleSettings.driverYmm)) {
        m_response = calculateFloorReflectionProductResponse(settings, baffleSettings);
        m_cachedSettings = settings;
        m_cachedBaffleHeightMm = baffleSettings.heightMm;
        m_cachedDriverYmm = baffleSettings.driverYmm;
        m_valid = true;
        ++m_generation;
    }

    return m_response;
}

std::uint64_t FloorReflectionResponseCache::generation() const
{
    return m_generation;
}
