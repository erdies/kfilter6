/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef FLOORREFLECTIONMODEL_H
#define FLOORREFLECTIONMODEL_H

// Product-level floor-surface selector. HardRigid remains the validated
// reference case. Patch 229 adds one explicitly experimental Miki porous-layer
// reference preset so the normal KFilter signal path can be compared against
// the Patch-228 diagnostic without pretending to model a specific carpet.
enum class FloorSurfacePreset
{
    HardRigid = 0,
    MikiReference10mm100k
};

constexpr double KFilterMikiReferenceFloorThicknessM = 0.010;
constexpr double KFilterMikiReferenceFloorFlowResistivityPaSPerM2 = 100000.0;

// Persisted per-driver placement metadata. Patch 226 applies these settings
// through the separate floorreflectionprocessing layer; source height remains
// derived from cabinet/baffle geometry rather than stored redundantly.
struct FloorReflectionSettings
{
    bool enabled = false;
    double cabinetBottomAboveFloorMm = 0.0;
    double listenerHeightAboveFloorMm = 1050.0;
    double horizontalDistanceMm = 2500.0;
    FloorSurfacePreset surfacePreset = FloorSurfacePreset::HardRigid;
};

#endif // FLOORREFLECTIONMODEL_H
