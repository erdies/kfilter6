/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef FLOORSURFACEMODEL_H
#define FLOORSURFACEMODEL_H

#include "floorreflectionresponse.h"

#include <complex>

enum class FloorSurfaceModelType
{
    Rigid = 0,
    MikiPorousRigidBacking
};

enum class FloorSurfaceSampleStatus
{
    Valid = 0,
    InvalidParameters
};

// Low-level, Qt-independent surface definition. This remains deliberately
// separate from the persisted FloorSurfacePreset enum: product presets map to
// explicit material definitions in floorreflectionprocessing.cpp.
struct FloorSurfaceDefinition
{
    FloorSurfaceModelType modelType = FloorSurfaceModelType::Rigid;

    // Used only by MikiPorousRigidBacking.
    double thicknessM = 0.0;
    double flowResistivityPaSPerM2 = 0.0;
};

struct FloorSurfaceSample
{
    FloorSurfaceSampleStatus status = FloorSurfaceSampleStatus::InvalidParameters;

    // Miki characteristic impedance normalized by rho_0*c_0.
    std::complex<double> normalizedCharacteristicImpedance{};

    // Miki propagation constant gamma = alpha + j*beta in 1/m for the
    // exp(+j*w*t) convention used by Miki (1990). KFilter's propagation term
    // exp(-j*k*Delta_r) is consistent with that convention.
    std::complex<double> propagationConstantPerM{};

    // Surface impedance normalized by rho_0*c_0 for a porous layer backed by
    // an acoustically rigid wall.
    std::complex<double> normalizedSurfaceImpedance{};

    // Complex pressure reflection coefficient at the requested incidence
    // angle (angle measured from the floor normal).
    std::complex<double> reflectionCoefficient{1.0, 0.0};

    // Plane-wave energy absorption coefficient implied by Gamma.
    double absorptionCoefficient = 0.0;

    // Diagnostic only: extreme extrapolation can produce tiny passive-boundary
    // violations even though the Miki characteristic-impedance fit itself was
    // designed for physical realizability. Product presets must later define a
    // validity policy before this model is enabled in the normal signal path.
    bool passivityWarning = false;

    // Miki's independent variable f/sigma in SI/MKS units. The 1990 paper
    // notes that the earlier Delany-Bazley model carried an extrapolation
    // warning below f/sigma = 0.01. Miki's modified model stays physically
    // well behaved there, but predictive accuracy was not fully verified.
    double frequencyToFlowResistivityRatio = 0.0;
    bool belowLegacyValidatedRatio = false;
};

FloorSurfaceDefinition rigidFloorSurfaceDefinition();
FloorSurfaceDefinition mikiPorousRigidBackingDefinition(
    double thicknessM,
    double flowResistivityPaSPerM2);

// Miki (1990), "Acoustical properties of porous materials - Modifications of
// Delany-Bazley models -", J. Acoust. Soc. Jpn. (E) 11(1), 19-24,
// DOI 10.1250/ast.11.19. The oblique-incidence reflection step assumes a
// locally reacting boundary.
FloorSurfaceSample calculateFloorSurfaceSample(
    const FloorSurfaceDefinition& surface,
    double frequencyHz,
    double incidenceCosine,
    double speedOfSoundMPerS = KFilterFloorReflectionSpeedOfSoundMPerS);

// Convenience combination of the validated image-source geometry with a
// frequency-/angle-dependent surface model. Patch 229 also reuses this exact
// helper for the productive Miki reference preset.
FloorReflectionResponse calculateFloorReflectionResponseWithSurfaceModel(
    const FloorReflectionGeometry& geometry,
    const FloorSurfaceDefinition& surface,
    double speedOfSoundMPerS = KFilterFloorReflectionSpeedOfSoundMPerS);

#endif // FLOORSURFACEMODEL_H
