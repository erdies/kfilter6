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


struct BaffleChamfer45Parameters
{
    // Front-baffle setback of the left/right straight 45-degree chamfer.
    // 0 mm means the original sharp Patch-211 edge on that side. Stage 3A
    // deliberately keeps these transient DSP parameters out of BaffleSettings;
    // UI/persistence integration is deferred to Stage 3B.
    double leftSetbackMm = 0.0;
    double rightSetbackMm = 0.0;
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

// Patch 216 investigation-only result. The freeField response is recomposed
// from the same four edge components used by the diagnostic candidate. The
// bottomEdgeOmitted response removes only the normal free lower-edge term.
// Neither response is cached, applied to drivers, shown in the UI or persisted.
struct BaffleRectangularBottomEdgeDiagnostic
{
    BaffleResponse freeField;
    BaffleResponse bottomEdgeOmitted;
};

// Patch 217/219 investigation-only rigid-floor/image-geometry result. The
// image geometry mirrors the entire rectangular baffle and source across a
// floor plane coincident with the lower cabinet edge, producing a doubled-
// height baffle. For an ideal rigid plane the image source is in phase. In the
// exact unfolded continuum geometry the real and mirror source responses are
// identical by symmetry. Patch 219 therefore uses one unfolded real-source
// response as the normalized diffraction-shape candidate and keeps the
// independently evaluated mirror-source response only as a numerical symmetry
// diagnostic. This avoids averaging finite contour/piston quadrature asymmetry
// into the candidate. imageGeometryRaw is exactly 2x the normalized response
// and represents the coherent pair before removing the +6.0206 dB factor.
// Patch 220 promotes imageGeometryNormalized to the optional production
// Rigid-floor boundary response for Sharp rectangular geometry. The remaining
// fields stay diagnostic-only cross-checks.
struct BaffleRectangularRigidFloorDiagnostic
{
    BaffleResponse freeField;
    BaffleResponse bottomEdgeOmitted;
    BaffleResponse imageGeometryRealSource;
    BaffleResponse imageGeometryMirrorSource;
    BaffleResponse imageGeometryNormalized;
    BaffleResponse imageGeometryRaw;
};

BaffleRectangularRigidFloorDiagnostic calculateBaffleRectangularRigidFloorDiagnostic(
    const BaffleSettings& settings,
    double effectiveDriverDiameterCm = 0.0);

// Engineering midpoint used by the Stage-1 Simple Baffle Step model.
// Width is specified in millimetres; invalid widths return 0 Hz.
double simpleBaffleStepMidpointFrequencyHz(double widthMm);

// Diagnostic/reference access to the validated raw Free-field Rectangular
// engine before the Patch-246 LF magnitude hybrid is applied. Sharp and
// Chamfer45 side treatments are supported; rigid-floor mode is deliberately
// excluded. This keeps finite-piston/chamfer goldens and model investigations
// independent from the productive LF trust law.
BaffleResponse calculateBaffleUnblendedRectangularResponseForDiagnostic(
    const BaffleSettings& settings,
    double effectiveDriverDiameterCm = 0.0);

// effectiveDriverDiameterCm is transient driver data (driver::Dm), not part of
// BaffleSettings. Free-field Rectangular Edge Diffraction first evaluates the
// validated raw rectangular/finite-piston/chamfer engine, then Patch 246 blends
// only its magnitude toward Simple Baffle Step with the width-anchored n=2 law
// below/through the baffle-step region while preserving the raw phase. The
// transient effective diameter selects the finite-piston source when the
// resulting disk fits fully inside the baffle. Invalid,
// missing or oversized diameters fall back exactly to the point-source path.
// Simple Baffle Step deliberately ignores the diameter. For Rectangular Edge
// Diffraction, BaffleBoundaryCondition::RigidFloorContactDiffractionOnly uses
// the normalized unfolded image geometry: it changes diffraction shape only
// and deliberately excludes the +6.02 dB rigid-boundary gain and any
// receiver-dependent floor-bounce interference. Patch 220 supports this
// production boundary only with Sharp side edges.
BaffleResponse calculateBaffleResponse(const BaffleSettings& settings,
                                       double effectiveDriverDiameterCm = 0.0);

// Investigation-only Sharp rectangular diagnostic for the idealized question
// "what changes if the normal free bottom-edge contribution is omitted?"
// This is deliberately not a rigid-floor production model. Side chamfers are
// rejected so the first experiment isolates only the lower-edge hypothesis.
BaffleRectangularBottomEdgeDiagnostic calculateBaffleRectangularBottomEdgeDiagnostic(
    const BaffleSettings& settings,
    double effectiveDriverDiameterCm = 0.0);

// Explicit computational entry point retained for regression/reference tests.
// Stage 3B normally selects the same left/right straight 45-degree chamfer
// engine through BaffleSettings. Patch 246 applies the same productive LF
// magnitude hybrid here as through calculateBaffleResponse(). If both setbacks
// are 0, this delegates to calculateBaffleResponse(); callers should use Sharp
// settings in that case. Positive setbacks below 5 mm are intentionally
// rejected by the bounded H1+H2+H3 production approximation.
BaffleResponse calculateBaffleResponseWithChamfer45(
    const BaffleSettings& settings,
    const BaffleChamfer45Parameters& chamfer,
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
