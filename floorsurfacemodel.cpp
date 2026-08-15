/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorsurfacemodel.h"

#include "kfilterfrequencygrid.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double TwoPi = 2.0 * Pi;
constexpr double LegacyValidatedRatioBoundary = 0.01;
constexpr double NumericalTolerance = 1.0e-12;
constexpr std::complex<double> Unity{1.0, 0.0};

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool finiteInUnitInterval(double value)
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool finiteComplex(const std::complex<double>& value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
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

FloorSurfaceDefinition rigidFloorSurfaceDefinition()
{
    return {};
}

FloorSurfaceDefinition mikiPorousRigidBackingDefinition(
    double thicknessM,
    double flowResistivityPaSPerM2)
{
    FloorSurfaceDefinition result;
    result.modelType = FloorSurfaceModelType::MikiPorousRigidBacking;
    result.thicknessM = thicknessM;
    result.flowResistivityPaSPerM2 = flowResistivityPaSPerM2;
    return result;
}

FloorSurfaceSample calculateFloorSurfaceSample(
    const FloorSurfaceDefinition& surface,
    double frequencyHz,
    double incidenceCosine,
    double speedOfSoundMPerS)
{
    FloorSurfaceSample result;

    if (!finitePositive(frequencyHz) ||
        !finiteInUnitInterval(incidenceCosine) ||
        !finitePositive(speedOfSoundMPerS)) {
        return result;
    }

    if (surface.modelType == FloorSurfaceModelType::Rigid) {
        result.status = FloorSurfaceSampleStatus::Valid;
        result.reflectionCoefficient = Unity;
        result.absorptionCoefficient = 0.0;
        return result;
    }

    if (surface.modelType != FloorSurfaceModelType::MikiPorousRigidBacking ||
        !finitePositive(surface.thicknessM) ||
        !finitePositive(surface.flowResistivityPaSPerM2)) {
        return result;
    }

    const double ratio = frequencyHz / surface.flowResistivityPaSPerM2;
    if (!finitePositive(ratio)) {
        return result;
    }

    result.frequencyToFlowResistivityRatio = ratio;
    result.belowLegacyValidatedRatio = ratio < LegacyValidatedRatioBoundary;

    // Miki 1990, Eqs. (29)-(31), normalized characteristic impedance:
    //   Zc/(rho0*c0) = R + jX
    //   R = 1 + 0.070 (f/sigma)^-0.632
    //   X =    -0.107 (f/sigma)^-0.632
    const double impedancePower = std::pow(ratio, -0.632);
    result.normalizedCharacteristicImpedance = {
        1.0 + 0.070 * impedancePower,
        -0.107 * impedancePower};

    // Miki 1990, Eqs. (32)-(34), propagation constant gamma=alpha+j*beta:
    //   alpha = w/c0 * 0.160 (f/sigma)^-0.618
    //   beta  = w/c0 * [1 + 0.109 (f/sigma)^-0.618]
    const double propagationPower = std::pow(ratio, -0.618);
    const double omegaOverC = TwoPi * frequencyHz / speedOfSoundMPerS;
    result.propagationConstantPerM = {
        omegaOverC * 0.160 * propagationPower,
        omegaOverC * (1.0 + 0.109 * propagationPower)};

    // Miki 1990, Eq. (35), rigidly backed porous layer:
    //   Zs = Zc * coth(gamma*l)
    // Here all impedances remain normalized by rho0*c0.
    const std::complex<double> tanhTerm =
        std::tanh(result.propagationConstantPerM * surface.thicknessM);
    if (!finiteComplex(tanhTerm) || std::abs(tanhTerm) <= NumericalTolerance) {
        return FloorSurfaceSample{};
    }

    result.normalizedSurfaceImpedance =
        result.normalizedCharacteristicImpedance / tanhTerm;
    if (!finiteComplex(result.normalizedSurfaceImpedance)) {
        return FloorSurfaceSample{};
    }

    // Locally reacting oblique-incidence pressure reflection coefficient:
    //   Gamma = (Zs*cos(theta) - Z0) / (Zs*cos(theta) + Z0)
    // Using normalized z_s=Zs/Z0 gives the form below.
    const std::complex<double> projectedImpedance =
        result.normalizedSurfaceImpedance * incidenceCosine;
    const std::complex<double> denominator = projectedImpedance + Unity;
    if (!finiteComplex(denominator) || std::abs(denominator) <= NumericalTolerance) {
        return FloorSurfaceSample{};
    }

    result.reflectionCoefficient = (projectedImpedance - Unity) / denominator;
    if (!finiteComplex(result.reflectionCoefficient)) {
        return FloorSurfaceSample{};
    }

    const double rawAbsorption = 1.0 - std::norm(result.reflectionCoefficient);
    if (!std::isfinite(rawAbsorption)) {
        return FloorSurfaceSample{};
    }

    result.absorptionCoefficient = rawAbsorption;
    result.passivityWarning = rawAbsorption < -1.0e-9 || rawAbsorption > 1.0 + 1.0e-9;
    result.status = FloorSurfaceSampleStatus::Valid;
    return result;
}

FloorReflectionResponse calculateFloorReflectionResponseWithSurfaceModel(
    const FloorReflectionGeometry& geometry,
    const FloorSurfaceDefinition& surface,
    double speedOfSoundMPerS)
{
    const FloorReflectionPathGeometry path = calculateFloorReflectionPathGeometry(geometry);
    if (!path.valid || !finitePositive(speedOfSoundMPerS)) {
        return invalidResponse(path);
    }

    FloorReflectionResponse response;
    response.geometry = path;
    response.status = FloorReflectionResponseStatus::Valid;

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const FloorSurfaceSample surfaceSample = calculateFloorSurfaceSample(
            surface,
            frequencies[sampleIndex],
            path.incidenceCosine,
            speedOfSoundMPerS);
        if (surfaceSample.status != FloorSurfaceSampleStatus::Valid) {
            return invalidResponse(path);
        }

        response.values[sampleIndex] = calculateFloorReflectionSample(
            path,
            frequencies[sampleIndex],
            surfaceSample.reflectionCoefficient,
            speedOfSoundMPerS);
    }

    return response;
}
