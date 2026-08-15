/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleresponse.h"
#include "kfilterfrequencygrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>

namespace
{
struct Geometry
{
    const char *name;
    double widthMm;
    double heightMm;
    double xFraction;
    double finitePistonDiameterCm;
};

struct DifferenceSummary
{
    double lowFrequencyDeltaDb = 0.0;
    double maximumShapeDeltaDb = 0.0;
    double maximumShapeFrequencyHz = 0.0;
};

double magnitudeDb(const std::complex<double>& value)
{
    return 20.0 * std::log10(std::abs(value));
}

DifferenceSummary summarizeDifference(const BaffleResponse& reference,
                                      const BaffleResponse& candidate)
{
    DifferenceSummary summary;
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    summary.lowFrequencyDeltaDb =
        magnitudeDb(candidate.values.front()) - magnitudeDb(reference.values.front());

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double deltaDb =
            magnitudeDb(candidate.values[sampleIndex]) -
            magnitudeDb(reference.values[sampleIndex]);
        const double shapeDeltaDb = std::abs(deltaDb - summary.lowFrequencyDeltaDb);
        if (shapeDeltaDb > summary.maximumShapeDeltaDb) {
            summary.maximumShapeDeltaDb = shapeDeltaDb;
            summary.maximumShapeFrequencyHz = frequencies[sampleIndex];
        }
    }
    return summary;
}

double maximumComplexDifference(const BaffleResponse& a, const BaffleResponse& b)
{
    double maximum = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        maximum = std::max(maximum, std::abs(a.values[sampleIndex] - b.values[sampleIndex]));
    }
    return maximum;
}
}

int main()
{
    constexpr std::array<Geometry, 4> Geometries{{
        {"compact",      200.0, 300.0, 0.50, 4.0},
        {"standmount",   230.0, 450.0, 0.39, 4.0},
        {"floorstander", 231.0, 965.0, 0.50, 4.0},
        {"wide",         400.0, 600.0, 0.35, 4.0}
    }};
    constexpr std::array<double, 3> YFractions{0.10, 0.50, 0.90};
    constexpr std::array<bool, 2> FinitePistonModes{false, true};

    std::cout << "KFilter Patch 220 floor image-geometry robustness sweep\n"
              << "All rows use Sharp rectangular geometry and N=200.\n"
              << "Point and M=73 finite-piston cases are both exercised.\n"
              << "C is the single-source unfolded geometry; the separately evaluated mirror\n"
              << "is reported ONLY to quantify finite numerical symmetry error.\n\n"
              << "geometry,mode,Y[%],X[mm],Y[mm],C-A@20[dB],C-A maxShape[dB],shapeFreq[Hz],real-mirror max|dH|\n";

    std::cout << std::fixed << std::setprecision(6);
    for (const Geometry& geometry : Geometries) {
        for (bool finitePiston : FinitePistonModes) {
            for (double yFraction : YFractions) {
                BaffleSettings settings;
                settings.enabled = true;
                settings.model = BaffleModel::RectangularEdgeDiffraction;
                settings.widthMm = geometry.widthMm;
                settings.heightMm = geometry.heightMm;
                settings.driverXmm = geometry.widthMm * geometry.xFraction;
                settings.driverYmm = geometry.heightMm * yFraction;
                settings.edgeSourceCount = 200;

                const double diameterCm = finitePiston ? geometry.finitePistonDiameterCm : 0.0;
                const BaffleRectangularRigidFloorDiagnostic diagnostic =
                    calculateBaffleRectangularRigidFloorDiagnostic(settings, diameterCm);
                if (diagnostic.freeField.status != BaffleResponseStatus::Valid ||
                    diagnostic.imageGeometryNormalized.status != BaffleResponseStatus::Valid ||
                    diagnostic.imageGeometryRealSource.status != BaffleResponseStatus::Valid ||
                    diagnostic.imageGeometryMirrorSource.status != BaffleResponseStatus::Valid) {
                    std::cerr << "diagnostic failed for " << geometry.name << '\n';
                    return 1;
                }

                const DifferenceSummary summary =
                    summarizeDifference(diagnostic.freeField,
                                        diagnostic.imageGeometryNormalized);
                const double mirrorMismatch =
                    maximumComplexDifference(diagnostic.imageGeometryRealSource,
                                             diagnostic.imageGeometryMirrorSource);

                std::cout << geometry.name << ','
                          << (finitePiston ? "M73" : "point") << ','
                          << yFraction * 100.0 << ','
                          << settings.driverXmm << ','
                          << settings.driverYmm << ','
                          << summary.lowFrequencyDeltaDb << ','
                          << summary.maximumShapeDeltaDb << ','
                          << summary.maximumShapeFrequencyHz << ','
                          << mirrorMismatch << '\n';
            }
        }
    }

    std::cout << "\nPoint-source mirror-symmetry convergence reference\n"
              << "N,max real-mirror |dH|\n";
    for (std::size_t edgeCount : {50u, 100u, 200u, 400u, 800u}) {
        BaffleSettings settings;
        settings.enabled = true;
        settings.model = BaffleModel::RectangularEdgeDiffraction;
        settings.widthMm = 231.0;
        settings.heightMm = 965.0;
        settings.driverXmm = settings.widthMm * 0.35;
        settings.driverYmm = settings.heightMm * 0.90;
        settings.edgeSourceCount = edgeCount;

        const BaffleRectangularRigidFloorDiagnostic diagnostic =
            calculateBaffleRectangularRigidFloorDiagnostic(settings, 0.0);
        if (diagnostic.imageGeometryRealSource.status != BaffleResponseStatus::Valid ||
            diagnostic.imageGeometryMirrorSource.status != BaffleResponseStatus::Valid) {
            std::cerr << "convergence diagnostic failed at N=" << edgeCount << '\n';
            return 1;
        }
        std::cout << edgeCount << ','
                  << maximumComplexDifference(diagnostic.imageGeometryRealSource,
                                              diagnostic.imageGeometryMirrorSource)
                  << '\n';
    }

    return 0;
}
