/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLEMODEL_H
#define BAFFLEMODEL_H

#include <cstddef>

enum class BaffleModel
{
    SimpleBaffleStep = 0,
    RectangularEdgeDiffraction
};

enum class BaffleSideEdgeTreatment
{
    Sharp = 0,
    Chamfer45
};

enum class BaffleBoundaryCondition
{
    FreeField = 0,
    RigidFloorContactDiffractionOnly
};

struct BaffleSettings
{
    bool enabled = false;
    BaffleModel model = BaffleModel::SimpleBaffleStep;
    double widthMm = 200.0;
    double heightMm = 0.0;
    double driverXmm = 0.0;
    double driverYmm = 0.0;
    BaffleBoundaryCondition boundaryCondition = BaffleBoundaryCondition::FreeField;
    bool showResponseInPlot = false;
    std::size_t edgeSourceCount = 200;

    // Version-1 chamfer support is deliberately restricted to the two vertical
    // side edges.  The setback is measured on the front-baffle plane.  Width
    // values are retained while an edge is Sharp so toggling the UI treatment
    // does not destroy the previous construction value; transferEquivalent()
    // intentionally ignores an inactive width.
    BaffleSideEdgeTreatment leftEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
    double leftChamferSetbackMm = 20.0;
    BaffleSideEdgeTreatment rightEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
    double rightChamferSetbackMm = 20.0;

    // Compare only parameters that can change the calculated transfer function.
    // Diagnostic visibility and inactive chamfer-width edit history are
    // intentionally excluded from cache invalidation.
    bool transferEquivalent(const BaffleSettings& other) const;
};

#endif // BAFFLEMODEL_H
