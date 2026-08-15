/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef FLOORREFLECTIONPROCESSING_H
#define FLOORREFLECTIONPROCESSING_H

#include "bafflemodel.h"
#include "floorreflectionmodel.h"
#include "floorreflectionresponse.h"

#include <complex>
#include <cstddef>
#include <cstdint>

// Product integration boundary introduced by Patch 226. Placement settings
// stay independent from the F0 image-source solver; only this layer derives the
// source height from the existing baffle geometry and selects the productive
// surface model. Patch 229 adds the experimental Miki reference preset while
// preserving HardRigid as the exact reference path.
FloorReflectionResponse calculateFloorReflectionProductResponse(
    const FloorReflectionSettings& settings,
    const BaffleSettings& baffleSettings);

// Invalid, unsupported or disabled Floor Reflection bypasses only H_floor.
std::complex<double> applyFloorReflectionResponseSample(
    const FloorReflectionResponse& response,
    std::size_t sampleIndex,
    const std::complex<double>& signal);

class FloorReflectionResponseCache
{
public:
    const FloorReflectionResponse& responseFor(
        const FloorReflectionSettings& settings,
        const BaffleSettings& baffleSettings);
    std::uint64_t generation() const;

private:
    bool m_valid = false;
    FloorReflectionSettings m_cachedSettings;
    double m_cachedBaffleHeightMm = 0.0;
    double m_cachedDriverYmm = 0.0;
    FloorReflectionResponse m_response;
    std::uint64_t m_generation = 0;
};

#endif // FLOORREFLECTIONPROCESSING_H
