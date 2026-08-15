/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef FLOORREFLECTIONRESPONSE_H
#define FLOORREFLECTIONRESPONSE_H

#include "kfilterfrequencygrid.h"

#include <array>
#include <complex>
#include <cstddef>

constexpr double KFilterFloorReflectionSpeedOfSoundMPerS = 343.0;

enum class FloorReflectionResponseStatus
{
    Neutral = 0,
    Valid,
    InvalidParameters
};

// Stage F0 placement geometry for one specular reflection from an infinite
// horizontal floor. Heights and distance are measured in metres. This unit is
// deliberately independent of BaffleSettings, Driver, Qt and persistence.
struct FloorReflectionGeometry
{
    double sourceHeightM = 0.0;
    double listenerHeightM = 0.0;
    double horizontalDistanceM = 0.0;
};

// Derived image-source geometry. The incidence angle is measured from the
// floor normal, matching the later FloorSurfaceModel convention.
struct FloorReflectionPathGeometry
{
    double directDistanceM = 0.0;
    double imageDistanceM = 0.0;
    double pathDifferenceM = 0.0;
    double incidenceCosine = 1.0;
    double incidenceAngleRad = 0.0;
    bool valid = false;
};

struct FloorReflectionResponse
{
    std::array<std::complex<double>, KFilterFrequencyCount> values{};
    FloorReflectionPathGeometry geometry;
    FloorReflectionResponseStatus status = FloorReflectionResponseStatus::Neutral;

    bool plottable() const
    {
        return status == FloorReflectionResponseStatus::Valid;
    }
};

// Returns invalid geometry for non-finite/negative inputs or for a degenerate
// zero-length direct/image path. Source height 0 is intentionally valid and is
// an important rigid-boundary regression case.
FloorReflectionPathGeometry calculateFloorReflectionPathGeometry(
    const FloorReflectionGeometry& geometry);

// Low-level complex sample used by Stage F0 tests and reserved as the clean
// interface boundary for a future frequency-/angle-dependent surface model.
// The caller supplies Gamma for this one frequency; no material physics lives
// in this module.
std::complex<double> calculateFloorReflectionSample(
    const FloorReflectionPathGeometry& geometry,
    double frequencyHz,
    const std::complex<double>& reflectionCoefficient,
    double speedOfSoundMPerS = KFilterFloorReflectionSpeedOfSoundMPerS);

// Stage F0 production/reference response: ideal rigid floor, Gamma = +1 + j0,
// on the shared 150-point KFilter frequency grid.
FloorReflectionResponse calculateIdealRigidFloorReflectionResponse(
    const FloorReflectionGeometry& geometry,
    double speedOfSoundMPerS = KFilterFloorReflectionSpeedOfSoundMPerS);

// Stage F0 diagnostic helper for analytical regression tests. It deliberately
// accepts only one frequency-independent Gamma; later material models should
// calculate Gamma(f, theta) externally and use calculateFloorReflectionSample.
FloorReflectionResponse calculateFloorReflectionResponseWithConstantCoefficient(
    const FloorReflectionGeometry& geometry,
    const std::complex<double>& reflectionCoefficient,
    double speedOfSoundMPerS = KFilterFloorReflectionSpeedOfSoundMPerS);

#endif // FLOORREFLECTIONRESPONSE_H
