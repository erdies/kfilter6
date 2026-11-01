/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef IMPEDANCEFIT_H
#define IMPEDANCEFIT_H

#include "driver.h"
#include "impedancereferencecurve.h"

#include <QString>

struct KFilterImpedanceFitOptions
{
    double minimumFrequencyHz = 0.0;
    double maximumFrequencyHz = 0.0;
    /** Rdc is normally measured separately and therefore kept fixed. */
    bool fitRdc = false;
};

struct KFilterImpedanceFitResult
{
    bool success = false;
    QString errorMessage;

    double rdcOhm = 0.0;
    double lspH = 0.0;
    double r2Ohm = 0.0;
    double l2H = 0.0;

    /** RMS of |Zmodel - Zmeasured| / |Zmeasured| over the fit band, in percent. */
    double initialRmsPercent = 0.0;
    double finalRmsPercent = 0.0;
    int pointCount = 0;
    int iterations = 0;
    /** True when the residual used magnitude only because the ZMA has no phase. */
    bool magnitudeOnly = false;
};

/**
 * Default fit band: from 2 x Fs (or the first ZMA frequency when Fs is 0)
 * up to the last ZMA frequency, clipped to the ZMA range.
 */
void defaultImpedanceFitBand(const driver& drv,
                             const KFilterImpedanceReferenceCurve& zma,
                             double& minimumFrequencyHz,
                             double& maximumFrequencyHz);

/**
 * Fit the LR2 voice coil (Lsp, R2, L2 and optionally Rdc) of drv to a measured
 * impedance. All other driver and enclosure values stay fixed and are part of
 * the model, so the resonance tail does not bias the coil values.
 *
 * Levenberg-Marquardt on the complex impedance error relative to |Zmeasured|
 * (magnitude only when the ZMA has no phase), with logarithmic parameters to
 * keep them positive. Start values come from the effective inductance
 * Im{Zcoil}/w of the measurement and from the driver's current values; the
 * better start wins.
 */
KFilterImpedanceFitResult fitVoiceCoilToImpedance(const driver& drv,
                                                  const KFilterImpedanceReferenceCurve& zma,
                                                  const KFilterImpedanceFitOptions& options);

#endif // IMPEDANCEFIT_H
