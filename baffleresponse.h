/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLERESPONSE_H
#define BAFFLERESPONSE_H

#include "bafflemodel.h"
#include "kfilterfrequencygrid.h"

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>

enum class BaffleResponseStatus
{
    Neutral = 0,
    Valid,
    UnsupportedModel,
    InvalidParameters
};

struct BaffleResponse
{
    std::array<std::complex<double>, KFilterFrequencyCount> values{};
    BaffleResponseStatus status = BaffleResponseStatus::Neutral;

    bool plottable() const
    {
        return status == BaffleResponseStatus::Valid;
    }
};

// Engineering midpoint used by the Stage-1 Simple Baffle Step model.
// Width is specified in millimetres; invalid widths return 0 Hz.
double simpleBaffleStepMidpointFrequencyHz(double widthMm);

// effectiveDriverDiameterCm is transient driver data (driver::Dm), not part of
// BaffleSettings. Rectangular Edge Diffraction uses it for the finite-piston
// source when the resulting disk fits fully inside the baffle. Invalid,
// missing or oversized diameters fall back exactly to the point-source path.
// Simple Baffle Step deliberately ignores the diameter.
BaffleResponse calculateBaffleResponse(const BaffleSettings& settings,
                                       double effectiveDriverDiameterCm = 0.0);

// Invalid or currently unsupported baffle settings bypass only the baffle stage.
std::complex<double> applyBaffleResponseSample(
    const BaffleResponse& response,
    std::size_t sampleIndex,
    const std::complex<double>& signal);

class BaffleResponseCache
{
public:
    const BaffleResponse& responseFor(const BaffleSettings& settings,
                                      double effectiveDriverDiameterCm = 0.0);
    std::uint64_t generation() const;

private:
    bool m_valid = false;
    BaffleSettings m_cachedSettings;
    double m_cachedEffectiveDriverDiameterCm = 0.0;
    BaffleResponse m_response;
    std::uint64_t m_generation = 0;
};

#endif // BAFFLERESPONSE_H
