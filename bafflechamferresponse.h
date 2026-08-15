/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLECHAMFERRESPONSE_H
#define BAFFLECHAMFERRESPONSE_H

#include "kfilterfrequencygrid.h"

#include <array>
#include <complex>

struct BaffleChamfer45SideCorrection
{
    std::array<std::complex<double>, KFilterFrequencyCount> values{};
    bool valid = false;
};

// Stage-3A acoustic kernel for one straight 45-degree vertical chamfer.
// distanceFromOriginalEdgeM is the driver-centre distance from the original
// sharp cabinet side edge. setbackM is the front-baffle setback of the 45°
// chamfer. The correction is normalized against the original sharp 90° edge,
// so it contains both wedge/multiple-diffraction physics and the changed path.
//
// The production approximation sums diffraction orders H1+H2+H3.  It is
// intended for constructionally meaningful chamfers; callers must enforce the
// Version-1 minimum setback of 5 mm.
BaffleChamfer45SideCorrection calculateBaffleChamfer45SideCorrection(
    double distanceFromOriginalEdgeM,
    double sourceYM,
    double edgeHeightM,
    double setbackM);

#endif // BAFFLECHAMFERRESPONSE_H
