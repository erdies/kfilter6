/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorreflectionmodel.h"

#include <cmath>
#include <iostream>

namespace
{
bool near(double left, double right)
{
    return std::abs(left - right) <= 1.0e-12;
}
}

int main()
{
    const FloorReflectionSettings defaults;
    if (defaults.enabled ||
        !near(defaults.cabinetBottomAboveFloorMm, 0.0) ||
        !near(defaults.listenerHeightAboveFloorMm, 1050.0) ||
        !near(defaults.horizontalDistanceMm, 2500.0) ||
        defaults.surfacePreset != FloorSurfacePreset::HardRigid) {
        std::cerr << "floor reflection model defaults mismatch\n";
        return 1;
    }

    FloorReflectionSettings editable = defaults;
    editable.enabled = true;
    editable.cabinetBottomAboveFloorMm = 275.0;
    editable.listenerHeightAboveFloorMm = 1125.0;
    editable.horizontalDistanceMm = 3200.0;
    editable.surfacePreset = FloorSurfacePreset::MikiReference10mm100k;

    if (!editable.enabled ||
        !near(editable.cabinetBottomAboveFloorMm, 275.0) ||
        !near(editable.listenerHeightAboveFloorMm, 1125.0) ||
        !near(editable.horizontalDistanceMm, 3200.0) ||
        editable.surfacePreset != FloorSurfacePreset::MikiReference10mm100k ||
        !near(KFilterMikiReferenceFloorThicknessM, 0.010) ||
        !near(KFilterMikiReferenceFloorFlowResistivityPaSPerM2, 100000.0)) {
        std::cerr << "floor reflection model mutation/reference preset mismatch\n";
        return 1;
    }

    std::cout << "floor reflection model smoke test passed\n";
    return 0;
}
