/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorsurfacemodel.h"

#include <cmath>
#include <complex>
#include <iostream>
#include <limits>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;

bool close(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

bool closeComplex(const std::complex<double>& actual,
                  const std::complex<double>& expected,
                  double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

int fail(const char* message)
{
    std::cerr << "floor surface model smoke test failed: " << message << '\n';
    return 1;
}
}

int main()
{
    const FloorSurfaceSample rigid = calculateFloorSurfaceSample(
        rigidFloorSurfaceDefinition(), 1000.0, 0.5);
    if (rigid.status != FloorSurfaceSampleStatus::Valid ||
        rigid.reflectionCoefficient != std::complex<double>(1.0, 0.0) ||
        rigid.absorptionCoefficient != 0.0) {
        return fail("rigid reference is not Gamma=+1");
    }

    // Published Miki reference geometry from Fig. 3 / Eq. (35):
    // sigma=100000 Pa*s/m^2, l=10 mm. The fixed numerical values below were
    // independently evaluated from Miki Eqs. (29)-(35) at 1 kHz.
    const FloorSurfaceDefinition porous =
        mikiPorousRigidBackingDefinition(0.010, 100000.0);
    const FloorSurfaceSample normal = calculateFloorSurfaceSample(
        porous, 1000.0, 1.0);
    if (normal.status != FloorSurfaceSampleStatus::Valid) {
        return fail("Miki normal-incidence sample rejected");
    }

    if (!closeComplex(normal.normalizedCharacteristicImpedance,
                      {2.285576840343843, -1.965096027382731}, 1.0e-12) ||
        !closeComplex(normal.propagationConstantPerM,
                      {50.466795714268592, 52.698829091073129}, 1.0e-12) ||
        !closeComplex(normal.normalizedSurfaceImpedance,
                      {0.953405570670971, -4.080051857934519}, 1.0e-12) ||
        !closeComplex(normal.reflectionCoefficient,
                      {0.809075679846174, -0.398781051341495}, 1.0e-12) ||
        !close(normal.absorptionCoefficient, 0.186370217372423, 1.0e-12)) {
        return fail("Miki 1-kHz reference values changed");
    }

    const double incidenceCosine = std::cos(54.701506235929 * Pi / 180.0);
    const FloorSurfaceSample oblique = calculateFloorSurfaceSample(
        porous, 1000.0, incidenceCosine);
    if (oblique.status != FloorSurfaceSampleStatus::Valid ||
        !closeComplex(oblique.reflectionCoefficient,
                      {0.610500389044510, -0.592093393333797}, 1.0e-12) ||
        !close(std::abs(oblique.reflectionCoefficient), 0.850461822454735, 1.0e-12) ||
        !close(oblique.absorptionCoefficient, 0.276714688546971, 1.0e-12)) {
        return fail("oblique-incidence Miki reference changed");
    }

    // Passive-material invariant inside the legacy f/sigma >= 0.01 region.
    // Below that ratio Patch 228 deliberately reports extrapolation instead of
    // silently clamping the complex coefficient.
    for (double cosine : {1.0, 0.8660254037844386, 0.5, 0.1736481776669304}) {
        for (double frequency = 1000.0; frequency <= 20000.0; frequency *= 1.07) {
            const FloorSurfaceSample sample = calculateFloorSurfaceSample(
                porous, frequency, cosine);
            if (sample.status != FloorSurfaceSampleStatus::Valid ||
                sample.passivityWarning ||
                std::abs(sample.reflectionCoefficient) > 1.0 + 1.0e-10 ||
                sample.absorptionCoefficient < -1.0e-10 ||
                sample.absorptionCoefficient > 1.0 + 1.0e-10) {
                return fail("passivity invariant failed in reference range");
            }
        }
    }

    const FloorSurfaceSample twentyHz = calculateFloorSurfaceSample(
        porous, 20.0, 1.0);
    if (twentyHz.status != FloorSurfaceSampleStatus::Valid ||
        !twentyHz.belowLegacyValidatedRatio ||
        !twentyHz.passivityWarning) {
        return fail("extreme low-frequency extrapolation warning missing");
    }

    const FloorSurfaceSample lowRatio = calculateFloorSurfaceSample(
        porous, 317.0, incidenceCosine);
    if (!lowRatio.belowLegacyValidatedRatio ||
        !(lowRatio.frequencyToFlowResistivityRatio < 0.01)) {
        return fail("low f/sigma diagnostic flag missing");
    }

    const FloorSurfaceSample invalidThickness = calculateFloorSurfaceSample(
        mikiPorousRigidBackingDefinition(0.0, 100000.0), 1000.0, 1.0);
    const FloorSurfaceSample invalidSigma = calculateFloorSurfaceSample(
        mikiPorousRigidBackingDefinition(0.010, -1.0), 1000.0, 1.0);
    const FloorSurfaceSample invalidAngle = calculateFloorSurfaceSample(
        porous, 1000.0, 1.1);
    const FloorSurfaceSample invalidFrequency = calculateFloorSurfaceSample(
        porous, 0.0, 1.0);
    const FloorSurfaceSample invalidNan = calculateFloorSurfaceSample(
        porous, 1000.0, std::numeric_limits<double>::quiet_NaN());

    if (invalidThickness.status != FloorSurfaceSampleStatus::InvalidParameters ||
        invalidSigma.status != FloorSurfaceSampleStatus::InvalidParameters ||
        invalidAngle.status != FloorSurfaceSampleStatus::InvalidParameters ||
        invalidFrequency.status != FloorSurfaceSampleStatus::InvalidParameters ||
        invalidNan.status != FloorSurfaceSampleStatus::InvalidParameters) {
        return fail("invalid parameters accepted");
    }

    const FloorReflectionGeometry geometry{0.72, 1.05, 2.50};
    const FloorReflectionResponse rigidResponse =
        calculateFloorReflectionResponseWithSurfaceModel(
            geometry, rigidFloorSurfaceDefinition());
    const FloorReflectionResponse existingRigid =
        calculateIdealRigidFloorReflectionResponse(geometry);

    if (rigidResponse.status != FloorReflectionResponseStatus::Valid ||
        existingRigid.status != FloorReflectionResponseStatus::Valid) {
        return fail("rigid response regression setup invalid");
    }
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        if (!closeComplex(rigidResponse.values[i], existingRigid.values[i], 1.0e-13)) {
            return fail("surface-model rigid path differs from Stage F0");
        }
    }

    const FloorReflectionResponse porousResponse =
        calculateFloorReflectionResponseWithSurfaceModel(geometry, porous);
    if (porousResponse.status != FloorReflectionResponseStatus::Valid) {
        return fail("porous floor response could not be calculated");
    }

    std::cout << "floor surface Miki model smoke test passed\n";
    return 0;
}
