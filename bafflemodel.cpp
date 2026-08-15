/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "bafflemodel.h"

namespace
{
bool edgeTreatmentTransferEquivalent(BaffleSideEdgeTreatment treatment,
                                     double setbackMm,
                                     BaffleSideEdgeTreatment otherTreatment,
                                     double otherSetbackMm)
{
    if (treatment != otherTreatment) {
        return false;
    }
    if (treatment == BaffleSideEdgeTreatment::Sharp) {
        return true;
    }
    return setbackMm == otherSetbackMm;
}
}

bool BaffleSettings::transferEquivalent(const BaffleSettings& other) const
{
    if (enabled != other.enabled) {
        return false;
    }

    // Disabled baffle processing is neutral regardless of stored geometry.
    if (!enabled) {
        return true;
    }

    if (model != other.model) {
        return false;
    }

    switch (model) {
    case BaffleModel::SimpleBaffleStep:
        return widthMm == other.widthMm;

    case BaffleModel::RectangularEdgeDiffraction:
        return boundaryCondition == other.boundaryCondition &&
               widthMm == other.widthMm &&
               heightMm == other.heightMm &&
               driverXmm == other.driverXmm &&
               driverYmm == other.driverYmm &&
               edgeSourceCount == other.edgeSourceCount &&
               edgeTreatmentTransferEquivalent(leftEdgeTreatment,
                                               leftChamferSetbackMm,
                                               other.leftEdgeTreatment,
                                               other.leftChamferSetbackMm) &&
               edgeTreatmentTransferEquivalent(rightEdgeTreatment,
                                               rightChamferSetbackMm,
                                               other.rightEdgeTreatment,
                                               other.rightChamferSetbackMm);
    }

    return false;
}
