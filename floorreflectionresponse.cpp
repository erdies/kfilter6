/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorreflectionresponse.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double TwoPi = 2.0 * Pi;
constexpr std::complex<double> Unity{1.0, 0.0};

bool finiteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool validCoefficient(const std::complex<double>& coefficient)
{
    return std::isfinite(coefficient.real()) && std::isfinite(coefficient.imag());
}

FloorReflectionResponse invalidResponse(const FloorReflectionPathGeometry& geometry)
{
    FloorReflectionResponse response;
    response.geometry = geometry;
    response.values.fill(Unity);
    response.status = FloorReflectionResponseStatus::InvalidParameters;
    return response;
}
}

FloorReflectionPathGeometry calculateFloorReflectionPathGeometry(
    const FloorReflectionGeometry& geometry)
{
    FloorReflectionPathGeometry result;

    if (!finiteNonNegative(geometry.sourceHeightM) ||
        !finiteNonNegative(geometry.listenerHeightM) ||
        !finiteNonNegative(geometry.horizontalDistanceM)) {
        return result;
    }

    const double directVerticalM = geometry.listenerHeightM - geometry.sourceHeightM;
    const double imageVerticalM = geometry.listenerHeightM + geometry.sourceHeightM;

    result.directDistanceM = std::hypot(geometry.horizontalDistanceM, directVerticalM);
    result.imageDistanceM = std::hypot(geometry.horizontalDistanceM, imageVerticalM);

    if (!std::isfinite(result.directDistanceM) ||
        !std::isfinite(result.imageDistanceM) ||
        result.directDistanceM <= 0.0 ||
        result.imageDistanceM <= 0.0) {
        return result;
    }

    result.pathDifferenceM = result.imageDistanceM - result.directDistanceM;
    if (!std::isfinite(result.pathDifferenceM)) {
        return result;
    }

    const double rawCosine = imageVerticalM / result.imageDistanceM;
    if (!std::isfinite(rawCosine)) {
        return result;
    }

    result.incidenceCosine = std::clamp(rawCosine, 0.0, 1.0);
    result.incidenceAngleRad = std::acos(result.incidenceCosine);
    result.valid = std::isfinite(result.incidenceAngleRad);
    return result;
}

std::complex<double> calculateFloorReflectionSample(
    const FloorReflectionPathGeometry& geometry,
    double frequencyHz,
    const std::complex<double>& reflectionCoefficient,
    double speedOfSoundMPerS)
{
    if (!geometry.valid ||
        !std::isfinite(frequencyHz) || frequencyHz < 0.0 ||
        !validCoefficient(reflectionCoefficient) ||
        !std::isfinite(speedOfSoundMPerS) || speedOfSoundMPerS <= 0.0) {
        return Unity;
    }

    const double distanceRatio = geometry.directDistanceM / geometry.imageDistanceM;
    const double phaseRad = -TwoPi * frequencyHz * geometry.pathDifferenceM / speedOfSoundMPerS;
    const std::complex<double> propagation = std::polar(1.0, phaseRad);
    return Unity + reflectionCoefficient * distanceRatio * propagation;
}

FloorReflectionResponse calculateFloorReflectionResponseWithConstantCoefficient(
    const FloorReflectionGeometry& geometry,
    const std::complex<double>& reflectionCoefficient,
    double speedOfSoundMPerS)
{
    const FloorReflectionPathGeometry path = calculateFloorReflectionPathGeometry(geometry);
    if (!path.valid ||
        !validCoefficient(reflectionCoefficient) ||
        !std::isfinite(speedOfSoundMPerS) || speedOfSoundMPerS <= 0.0) {
        return invalidResponse(path);
    }

    FloorReflectionResponse response;
    response.geometry = path;
    response.status = FloorReflectionResponseStatus::Valid;

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        response.values[sampleIndex] = calculateFloorReflectionSample(
            path, frequencies[sampleIndex], reflectionCoefficient, speedOfSoundMPerS);
    }

    return response;
}

FloorReflectionResponse calculateIdealRigidFloorReflectionResponse(
    const FloorReflectionGeometry& geometry,
    double speedOfSoundMPerS)
{
    return calculateFloorReflectionResponseWithConstantCoefficient(
        geometry, Unity, speedOfSoundMPerS);
}
