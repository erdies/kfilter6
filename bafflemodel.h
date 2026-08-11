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

struct BaffleSettings
{
    bool enabled = false;
    BaffleModel model = BaffleModel::SimpleBaffleStep;
    double widthMm = 200.0;
    double heightMm = 0.0;
    double driverXmm = 0.0;
    double driverYmm = 0.0;
    bool showResponseInPlot = false;
    std::size_t edgeSourceCount = 200;

    // Compare only parameters that can change the calculated transfer function.
    // Diagnostic visibility is intentionally excluded from cache invalidation.
    bool transferEquivalent(const BaffleSettings& other) const;
};

#endif // BAFFLEMODEL_H
