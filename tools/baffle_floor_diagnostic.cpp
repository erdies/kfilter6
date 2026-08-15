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
double magnitudeDb(const std::complex<double>& value)
{
    return 20.0 * std::log10(std::abs(value));
}

struct DifferenceSummary
{
    double lowFrequencyDeltaDb = 0.0;
    double maximumAbsoluteDeltaDb = 0.0;
    double maximumFrequencyHz = 0.0;
    double maximumShapeDeltaDb = 0.0;
    double maximumShapeFrequencyHz = 0.0;
};

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
        const double absoluteDeltaDb = std::abs(deltaDb);
        if (absoluteDeltaDb > summary.maximumAbsoluteDeltaDb) {
            summary.maximumAbsoluteDeltaDb = absoluteDeltaDb;
            summary.maximumFrequencyHz = frequencies[sampleIndex];
        }

        const double shapeDeltaDb =
            std::abs(deltaDb - summary.lowFrequencyDeltaDb);
        if (shapeDeltaDb > summary.maximumShapeDeltaDb) {
            summary.maximumShapeDeltaDb = shapeDeltaDb;
            summary.maximumShapeFrequencyHz = frequencies[sampleIndex];
        }
    }
    return summary;
}

double maximumPairGainDeviationDb(const BaffleResponse& normalized,
                                  const BaffleResponse& raw)
{
    constexpr double CoherentPairGainDb = 6.020599913279624;
    double maximumDeviationDb = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double gainDb =
            magnitudeDb(raw.values[sampleIndex]) - magnitudeDb(normalized.values[sampleIndex]);
        maximumDeviationDb =
            std::max(maximumDeviationDb, std::abs(gainDb - CoherentPairGainDb));
    }
    return maximumDeviationDb;
}

double maximumComplexDifference(const BaffleResponse& a, const BaffleResponse& b)
{
    double maximum = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        maximum = std::max(maximum, std::abs(a.values[sampleIndex] - b.values[sampleIndex]));
    }
    return maximum;
}

double maximumMagnitudeDbDifference(const BaffleResponse& a, const BaffleResponse& b)
{
    double maximum = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        maximum = std::max(maximum,
                           std::abs(magnitudeDb(a.values[sampleIndex]) -
                                    magnitudeDb(b.values[sampleIndex])));
    }
    return maximum;
}
}

int main()
{
    BaffleSettings settings;
    settings.enabled = true;
    settings.model = BaffleModel::RectangularEdgeDiffraction;
    settings.widthMm = 231.0;
    settings.heightMm = 965.0;
    settings.driverXmm = settings.widthMm / 2.0;
    settings.edgeSourceCount = 200;

    constexpr double EffectiveDiameterCm = 13.81976597885342;
    constexpr std::array<double, 5> YFractions{0.10, 0.25, 0.50, 0.75, 0.90};

    const double unfoldedHeightMm = 2.0 * settings.heightMm;
    const std::size_t unfoldedEdgeSourceCount = static_cast<std::size_t>(std::llround(
        static_cast<double>(settings.edgeSourceCount) *
        (settings.widthMm + unfoldedHeightMm) /
        (settings.widthMm + settings.heightMm)));

    std::cout << "KFilter Patch 220 rigid-floor/image-geometry diagnostic\n"
              << "A = current free field\n"
              << "B = Patch-216 bottom-edge omission candidate\n"
              << "C = normalized unfolded rigid-floor image geometry\n"
              << "Craw = exact 2x coherent image pair used only for normalization sanity\n"
              << "Mirror = independently evaluated mirrored source, retained only as a numerical symmetry diagnostic\n"
              << "NOTE: C now uses ONE unfolded source. The mirror calculation is not averaged into C,\n"
              << "      so finite edge/piston quadrature asymmetry cannot bias the candidate.\n"
              << "Geometry A/B: 231 x 965 mm, X=115.5 mm, N=200, Dm="
              << EffectiveDiameterCm << " cm\n"
              << "Geometry C:   231 x " << unfoldedHeightMm
              << " mm, N=" << unfoldedEdgeSourceCount
              << " (edge-source density preserved approximately)\n\n"
              << "Y[%],Y[mm],"
              << "B-A@20[dB],B-A maxShape[dB],B-A shapeFreq[Hz],"
              << "C-A@20[dB],C-A maxAbs[dB],C-A maxFreq[Hz],"
              << "C-A maxShape[dB],C-A shapeFreq[Hz],"
              << "C-B@20[dB],C-B maxShape[dB],C-B shapeFreq[Hz],"
              << "Craw/C gain@20[dB],max gain error[dB],"
              << "real-mirror max|dH|,old-average/new max[dB]\n";

    std::cout << std::fixed << std::setprecision(6);
    for (double fraction : YFractions) {
        settings.driverYmm = settings.heightMm * fraction;
        const BaffleRectangularRigidFloorDiagnostic diagnostic =
            calculateBaffleRectangularRigidFloorDiagnostic(settings, EffectiveDiameterCm);
        if (diagnostic.freeField.status != BaffleResponseStatus::Valid ||
            diagnostic.bottomEdgeOmitted.status != BaffleResponseStatus::Valid ||
            diagnostic.imageGeometryRealSource.status != BaffleResponseStatus::Valid ||
            diagnostic.imageGeometryMirrorSource.status != BaffleResponseStatus::Valid ||
            diagnostic.imageGeometryNormalized.status != BaffleResponseStatus::Valid ||
            diagnostic.imageGeometryRaw.status != BaffleResponseStatus::Valid) {
            std::cerr << "diagnostic failed at Y=" << settings.driverYmm << " mm\n";
            return 1;
        }

        const DifferenceSummary bottomVsFree =
            summarizeDifference(diagnostic.freeField, diagnostic.bottomEdgeOmitted);
        const DifferenceSummary imageVsFree =
            summarizeDifference(diagnostic.freeField, diagnostic.imageGeometryNormalized);
        const DifferenceSummary imageVsBottom =
            summarizeDifference(diagnostic.bottomEdgeOmitted,
                                diagnostic.imageGeometryNormalized);
        const double rawGain20Db =
            magnitudeDb(diagnostic.imageGeometryRaw.values.front()) -
            magnitudeDb(diagnostic.imageGeometryNormalized.values.front());
        const double maximumGainErrorDb =
            maximumPairGainDeviationDb(diagnostic.imageGeometryNormalized,
                                       diagnostic.imageGeometryRaw);
        const double mirrorMismatch =
            maximumComplexDifference(diagnostic.imageGeometryRealSource,
                                     diagnostic.imageGeometryMirrorSource);

        BaffleResponse oldPatch217Average = diagnostic.imageGeometryNormalized;
        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            oldPatch217Average.values[sampleIndex] =
                0.5 * (diagnostic.imageGeometryRealSource.values[sampleIndex] +
                       diagnostic.imageGeometryMirrorSource.values[sampleIndex]);
        }
        const double oldAverageVsNewDb =
            maximumMagnitudeDbDifference(oldPatch217Average,
                                         diagnostic.imageGeometryNormalized);

        std::cout << fraction * 100.0 << ','
                  << settings.driverYmm << ','
                  << bottomVsFree.lowFrequencyDeltaDb << ','
                  << bottomVsFree.maximumShapeDeltaDb << ','
                  << bottomVsFree.maximumShapeFrequencyHz << ','
                  << imageVsFree.lowFrequencyDeltaDb << ','
                  << imageVsFree.maximumAbsoluteDeltaDb << ','
                  << imageVsFree.maximumFrequencyHz << ','
                  << imageVsFree.maximumShapeDeltaDb << ','
                  << imageVsFree.maximumShapeFrequencyHz << ','
                  << imageVsBottom.lowFrequencyDeltaDb << ','
                  << imageVsBottom.maximumShapeDeltaDb << ','
                  << imageVsBottom.maximumShapeFrequencyHz << ','
                  << rawGain20Db << ','
                  << maximumGainErrorDb << ','
                  << mirrorMismatch << ','
                  << oldAverageVsNewDb << '\n';
    }

    return 0;
}
