/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLE_LF_HYBRID_DIAGNOSTIC_MODEL_H
#define BAFFLE_LF_HYBRID_DIAGNOSTIC_MODEL_H

#include "baffleresponse.h"

#include <array>

struct BaffleLfHybridDiagnostic
{
    BaffleResponse simple;
    // Unblended raw Sharp Rectangular/finite-piston reference. Patch 246 keeps
    // this separate from the productive Free-field LF magnitude hybrid.
    BaffleResponse rectangular;

    // Patch-242 candidate retained unchanged for direct regression/comparison.
    BaffleResponse hybrid;
    std::array<double, KFilterFrequencyCount> blendWeight{};
    double effectiveLengthM = 0.0;

    // Patch-244 candidate retained as n=1. Patch 245 added n=1.5 and n=2.
    // Patch 246 promotes n=2 to the productive Free-field Rectangular path;
    // all three remain here for direct regression/comparison.
    BaffleResponse widthAnchoredHybrid;
    std::array<double, KFilterFrequencyCount> widthAnchoredBlendWeight{};
    BaffleResponse widthAnchoredHybridN15;
    std::array<double, KFilterFrequencyCount> widthAnchoredBlendWeightN15{};
    BaffleResponse widthAnchoredHybridN2;
    std::array<double, KFilterFrequencyCount> widthAnchoredBlendWeightN2{};
    double simpleMidpointFrequencyHz = 0.0;

    bool valid = false;
};

// Diagnostic-only Patch-242 LF blend candidate. This helper deliberately lives
// under tools/ and is not part of kfilter_core or the productive Baffle
// response. It retains the current rectangular/finite-piston phase and blends
// only its magnitude toward the established width-only Simple Baffle Step at
// low frequency:
//
//   Le = sqrt(W * H)
//   x  = 2*pi*f*Le/c
//   w  = x / (x + pi)
//   Dhybrid = Dsimple + w * (Drectangular - Dsimple)   [dB]
//
// Therefore w -> 0 at LF and w -> 1 at HF. Patch 245 keeps this formula
// unchanged so it can be compared against the width-anchored family.
double baffleLfHybridBlendWeight(double widthMm,
                                 double heightMm,
                                 double frequencyHz);

// Patch-244/245 width-anchored family retained for diagnostics. Patch 246 uses
// the n=2 member of this exact law in the productive Free-field Rectangular
// path. The transition is tied directly to the Simple Baffle Step midpoint:
//
//   fBS = simpleBaffleStepMidpointFrequencyHz(W) = 115 / W[m]
//   r   = f / fBS
//   wN  = r^n / (1 + r^n)
//   DN  = Dsimple + wN * (Drectangular - Dsimple)      [dB]
//
// n=1 is exactly the Patch-244 law w=f/(f+fBS). Patch 245 compares n=1,
// n=1.5 and n=2; Patch 246 selects n=2 for production. All have w=0.5
// exactly at f=fBS. Increasing n keeps the
// candidate closer to Simple below fBS and moves it toward Rectangular faster
// above fBS. The blend weight remains independent of cabinet height.
double baffleLfWidthAnchoredBlendWeight(double widthMm,
                                        double frequencyHz,
                                        double exponent);

// Backward-compatible diagnostic helper for the Patch-244 n=1 candidate.
double baffleLfWidthAnchoredBlendWeight(double widthMm,
                                        double frequencyHz);

BaffleLfHybridDiagnostic calculateBaffleLfHybridDiagnostic(
    const BaffleSettings& rectangularSettings,
    double effectiveDriverDiameterCm = 0.0);

#endif // BAFFLE_LF_HYBRID_DIAGNOSTIC_MODEL_H
