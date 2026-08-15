/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleresponse.h"
#include "bafflechamferresponse.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double TwoPi = 2.0 * Pi;
constexpr double GoldenAngleRad = Pi * (3.0 - 2.236067977499789696409173668731276235);
constexpr double SimpleBaffleStepFrequencyConstantHzM = 115.0;
constexpr double SpeedOfSoundMPerS = 343.0;
constexpr std::size_t MinimumRectangularEdgeSourceCount = 4;
constexpr std::size_t MaximumRectangularEdgeSourceCount = 100000;
constexpr std::size_t FinitePistonSourceCount = 73;
constexpr double MinimumChamferSetbackM = 0.005;
constexpr std::complex<double> Unity{1.0, 0.0};

struct EdgePoint
{
    double xM = 0.0;
    double yM = 0.0;
};

struct SourcePoint
{
    double xM = 0.0;
    double yM = 0.0;
};

struct WeightedEdgeSource
{
    double distanceM = 0.0;
    double weight = 0.0;
};

using ComplexResponseArray =
    std::array<std::complex<double>, KFilterFrequencyCount>;

struct RectangularEdgeComponents
{
    ComplexResponseArray top{};
    ComplexResponseArray right{};
    ComplexResponseArray bottom{};
    ComplexResponseArray left{};
};

std::array<SourcePoint, FinitePistonSourceCount> finitePistonSamples(
    double centerXM,
    double centerYM,
    double radiusM);

BaffleResponse neutralResponse(BaffleResponseStatus status)
{
    BaffleResponse response;
    response.values.fill(Unity);
    response.status = status;
    return response;
}

bool finiteComplex(const std::complex<double>& value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

bool validRectangularSettings(const BaffleSettings& settings)
{
    return std::isfinite(settings.widthMm) &&
           std::isfinite(settings.heightMm) &&
           std::isfinite(settings.driverXmm) &&
           std::isfinite(settings.driverYmm) &&
           settings.widthMm > 0.0 &&
           settings.heightMm > 0.0 &&
           settings.driverXmm > 0.0 &&
           settings.driverXmm < settings.widthMm &&
           settings.driverYmm > 0.0 &&
           settings.driverYmm < settings.heightMm &&
           settings.edgeSourceCount >= MinimumRectangularEdgeSourceCount &&
           settings.edgeSourceCount <= MaximumRectangularEdgeSourceCount;
}

std::array<std::size_t, 4> proportionalEdgeCounts(std::size_t totalCount,
                                                   double widthM,
                                                   double heightM)
{
    std::array<std::size_t, 4> counts{1, 1, 1, 1};
    if (totalCount <= MinimumRectangularEdgeSourceCount) {
        return counts;
    }

    const std::array<double, 4> lengths{widthM, heightM, widthM, heightM};
    const double perimeter = 2.0 * (widthM + heightM);
    const std::size_t remaining = totalCount - MinimumRectangularEdgeSourceCount;

    std::array<double, 4> remainders{};
    std::size_t assigned = 0;
    for (std::size_t edge = 0; edge < lengths.size(); ++edge) {
        const double exactAdditional =
            static_cast<double>(remaining) * lengths[edge] / perimeter;
        const std::size_t additional = static_cast<std::size_t>(std::floor(exactAdditional));
        counts[edge] += additional;
        assigned += additional;
        remainders[edge] = exactAdditional - static_cast<double>(additional);
    }

    std::size_t leftovers = remaining - assigned;
    while (leftovers > 0) {
        std::size_t bestEdge = 0;
        for (std::size_t edge = 1; edge < remainders.size(); ++edge) {
            if (remainders[edge] > remainders[bestEdge]) {
                bestEdge = edge;
            }
        }
        ++counts[bestEdge];
        remainders[bestEdge] = -1.0;
        --leftovers;
    }

    return counts;
}

std::vector<EdgePoint> rectangularEdgePoints(double widthM,
                                               double heightM,
                                               std::size_t edgeSourceCount)
{
    const std::array<std::size_t, 4> counts =
        proportionalEdgeCounts(edgeSourceCount, widthM, heightM);

    std::vector<EdgePoint> points;
    points.reserve(edgeSourceCount);

    // Clockwise contour. Each edge includes its start corner and excludes its
    // end corner, so every physical corner occurs exactly once.
    for (std::size_t n = 0; n < counts[0]; ++n) {
        const double t = static_cast<double>(n) / static_cast<double>(counts[0]);
        points.push_back({widthM * t, 0.0});
    }
    for (std::size_t n = 0; n < counts[1]; ++n) {
        const double t = static_cast<double>(n) / static_cast<double>(counts[1]);
        points.push_back({widthM, heightM * t});
    }
    for (std::size_t n = 0; n < counts[2]; ++n) {
        const double t = static_cast<double>(n) / static_cast<double>(counts[2]);
        points.push_back({widthM * (1.0 - t), heightM});
    }
    for (std::size_t n = 0; n < counts[3]; ++n) {
        const double t = static_cast<double>(n) / static_cast<double>(counts[3]);
        points.push_back({0.0, heightM * (1.0 - t)});
    }

    return points;
}

bool buildRectangularEdgeSources(const std::vector<EdgePoint>& points,
                                 double sourceXM,
                                 double sourceYM,
                                 std::vector<WeightedEdgeSource>& sources)
{
    if (points.size() < MinimumRectangularEdgeSourceCount ||
        !std::isfinite(sourceXM) || !std::isfinite(sourceYM)) {
        return false;
    }

    sources.clear();
    sources.reserve(points.size());
    double angularSum = 0.0;

    for (std::size_t index = 0; index < points.size(); ++index) {
        const EdgePoint& current = points[index];
        const EdgePoint& next = points[(index + 1) % points.size()];

        const double currentX = current.xM - sourceXM;
        const double currentY = current.yM - sourceYM;
        const double nextX = next.xM - sourceXM;
        const double nextY = next.yM - sourceYM;
        const double distance = std::hypot(currentX, currentY);
        const double nextDistance = std::hypot(nextX, nextY);
        if (!std::isfinite(distance) || !std::isfinite(nextDistance) ||
            distance <= 0.0 || nextDistance <= 0.0) {
            return false;
        }

        const double cross = currentX * nextY - currentY * nextX;
        const double dot = currentX * nextX + currentY * nextY;
        const double phi = std::atan2(std::abs(cross), dot);
        if (!std::isfinite(phi) || phi <= 0.0) {
            return false;
        }

        sources.push_back({distance, phi / TwoPi});
        angularSum += phi;
    }

    // A source strictly inside a closed rectangular contour must see exactly
    // one full revolution. Reject malformed numerical geometry rather than
    // silently renormalizing a broken contour.
    return std::isfinite(angularSum) && std::abs(angularSum - TwoPi) <= 1.0e-9;
}

bool calculateRectangularResponseForSource(const std::vector<EdgePoint>& edgePoints,
                                           double sourceXM,
                                           double sourceYM,
                                           BaffleResponse& response)
{
    std::vector<WeightedEdgeSource> sources;
    if (!buildRectangularEdgeSources(edgePoints, sourceXM, sourceYM, sources)) {
        return false;
    }

    response = neutralResponse(BaffleResponseStatus::Valid);
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double waveNumber = TwoPi * frequencies[sampleIndex] / SpeedOfSoundMPerS;
        std::complex<double> edgeSum{0.0, 0.0};

        for (const WeightedEdgeSource& source : sources) {
            const double phase = -waveNumber * source.distanceM;
            const std::complex<double> delayedEdge{std::cos(phase), std::sin(phase)};
            edgeSum += source.weight * delayedEdge;
        }

        const std::complex<double> value = std::complex<double>{2.0, 0.0} - edgeSum;
        if (!finiteComplex(value)) {
            return false;
        }
        response.values[sampleIndex] = value;
    }

    return true;
}

bool calculateRectangularEdgeComponentsForSource(
    const std::vector<EdgePoint>& edgePoints,
    const std::array<std::size_t, 4>& edgeCounts,
    double sourceXM,
    double sourceYM,
    RectangularEdgeComponents& components)
{
    std::vector<WeightedEdgeSource> sources;
    if (!buildRectangularEdgeSources(edgePoints, sourceXM, sourceYM, sources) ||
        sources.size() != edgePoints.size()) {
        return false;
    }

    std::size_t countedSources = 0;
    for (std::size_t count : edgeCounts) {
        countedSources += count;
    }
    if (countedSources != sources.size()) {
        return false;
    }

    components = RectangularEdgeComponents{};
    const std::array<std::size_t, 4> edgeEnds{
        edgeCounts[0],
        edgeCounts[0] + edgeCounts[1],
        edgeCounts[0] + edgeCounts[1] + edgeCounts[2],
        countedSources
    };
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double waveNumber = TwoPi * frequencies[sampleIndex] / SpeedOfSoundMPerS;
        for (std::size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex) {
            const WeightedEdgeSource& source = sources[sourceIndex];
            const double phase = -waveNumber * source.distanceM;
            const std::complex<double> contribution =
                source.weight * std::complex<double>{std::cos(phase), std::sin(phase)};

            if (sourceIndex < edgeEnds[0]) {
                components.top[sampleIndex] += contribution;
            } else if (sourceIndex < edgeEnds[1]) {
                components.right[sampleIndex] += contribution;
            } else if (sourceIndex < edgeEnds[2]) {
                components.bottom[sampleIndex] += contribution;
            } else {
                components.left[sampleIndex] += contribution;
            }
        }
    }

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        if (!finiteComplex(components.top[sampleIndex]) ||
            !finiteComplex(components.right[sampleIndex]) ||
            !finiteComplex(components.bottom[sampleIndex]) ||
            !finiteComplex(components.left[sampleIndex])) {
            return false;
        }
    }
    return true;
}

void addRectangularEdgeComponents(RectangularEdgeComponents& target,
                                  const RectangularEdgeComponents& source)
{
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        target.top[sampleIndex] += source.top[sampleIndex];
        target.right[sampleIndex] += source.right[sampleIndex];
        target.bottom[sampleIndex] += source.bottom[sampleIndex];
        target.left[sampleIndex] += source.left[sampleIndex];
    }
}

void scaleRectangularEdgeComponents(RectangularEdgeComponents& components, double scale)
{
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        components.top[sampleIndex] *= scale;
        components.right[sampleIndex] *= scale;
        components.bottom[sampleIndex] *= scale;
        components.left[sampleIndex] *= scale;
    }
}

BaffleResponse composeRectangularResponse(
    const RectangularEdgeComponents& components,
    const BaffleChamfer45SideCorrection* leftCorrection,
    const BaffleChamfer45SideCorrection* rightCorrection,
    bool includeBottomEdge = true)
{
    BaffleResponse response = neutralResponse(BaffleResponseStatus::Valid);
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const std::complex<double> left =
            leftCorrection ? leftCorrection->values[sampleIndex] * components.left[sampleIndex]
                           : components.left[sampleIndex];
        const std::complex<double> right =
            rightCorrection ? rightCorrection->values[sampleIndex] * components.right[sampleIndex]
                            : components.right[sampleIndex];
        const std::complex<double> bottom = includeBottomEdge
            ? components.bottom[sampleIndex]
            : std::complex<double>{0.0, 0.0};
        const std::complex<double> value =
            std::complex<double>{2.0, 0.0} - components.top[sampleIndex] - right - bottom - left;
        if (!finiteComplex(value)) {
            return neutralResponse(BaffleResponseStatus::InvalidParameters);
        }
        response.values[sampleIndex] = value;
    }
    return response;
}

bool validChamfer45Parameters(const BaffleSettings& settings,
                              const BaffleChamfer45Parameters& chamfer)
{
    if (!std::isfinite(chamfer.leftSetbackMm) ||
        !std::isfinite(chamfer.rightSetbackMm) ||
        chamfer.leftSetbackMm < 0.0 || chamfer.rightSetbackMm < 0.0) {
        return false;
    }

    const double leftSetbackM = chamfer.leftSetbackMm / 1000.0;
    const double rightSetbackM = chamfer.rightSetbackMm / 1000.0;
    const bool leftActive = leftSetbackM > 0.0;
    const bool rightActive = rightSetbackM > 0.0;
    if ((leftActive && leftSetbackM < MinimumChamferSetbackM) ||
        (rightActive && rightSetbackM < MinimumChamferSetbackM)) {
        return false;
    }

    const double widthM = settings.widthMm / 1000.0;
    const double sourceXM = settings.driverXmm / 1000.0;
    return leftSetbackM + rightSetbackM < widthM &&
           sourceXM > leftSetbackM &&
           sourceXM < widthM - rightSetbackM;
}

bool finitePistonFitsChamfer(const BaffleSettings& settings,
                             const BaffleChamfer45Parameters& chamfer,
                             double effectiveDriverDiameterCm)
{
    if (!std::isfinite(effectiveDriverDiameterCm) || effectiveDriverDiameterCm <= 0.0) {
        return false;
    }

    const double radiusM = effectiveDriverDiameterCm / 200.0;
    const double widthM = settings.widthMm / 1000.0;
    const double heightM = settings.heightMm / 1000.0;
    const double sourceXM = settings.driverXmm / 1000.0;
    const double sourceYM = settings.driverYmm / 1000.0;
    const double leftSetbackM = chamfer.leftSetbackMm / 1000.0;
    const double rightSetbackM = chamfer.rightSetbackMm / 1000.0;
    const double minimumFlatFrontDistanceM = std::min(
        {sourceXM - leftSetbackM,
         widthM - rightSetbackM - sourceXM,
         sourceYM,
         heightM - sourceYM});

    return std::isfinite(radiusM) && radiusM > 0.0 &&
           std::isfinite(minimumFlatFrontDistanceM) &&
           radiusM < minimumFlatFrontDistanceM;
}

bool calculateChamferCorrections(const BaffleSettings& settings,
                                 const BaffleChamfer45Parameters& chamfer,
                                 BaffleChamfer45SideCorrection& leftCorrection,
                                 BaffleChamfer45SideCorrection& rightCorrection,
                                 const BaffleChamfer45SideCorrection*& leftPtr,
                                 const BaffleChamfer45SideCorrection*& rightPtr)
{
    const double widthM = settings.widthMm / 1000.0;
    const double heightM = settings.heightMm / 1000.0;
    const double sourceXM = settings.driverXmm / 1000.0;
    const double sourceYM = settings.driverYmm / 1000.0;
    const double leftSetbackM = chamfer.leftSetbackMm / 1000.0;
    const double rightSetbackM = chamfer.rightSetbackMm / 1000.0;

    leftPtr = nullptr;
    rightPtr = nullptr;
    if (leftSetbackM > 0.0) {
        leftCorrection = calculateBaffleChamfer45SideCorrection(
            sourceXM, sourceYM, heightM, leftSetbackM);
        if (!leftCorrection.valid) {
            return false;
        }
        leftPtr = &leftCorrection;
    }
    if (rightSetbackM > 0.0) {
        rightCorrection = calculateBaffleChamfer45SideCorrection(
            widthM - sourceXM, sourceYM, heightM, rightSetbackM);
        if (!rightCorrection.valid) {
            return false;
        }
        rightPtr = &rightCorrection;
    }
    return true;
}

BaffleResponse calculateRectangularChamfer45Response(
    const BaffleSettings& settings,
    const BaffleChamfer45Parameters& chamfer,
    double effectiveDriverDiameterCm)
{
    if (!validRectangularSettings(settings) || !validChamfer45Parameters(settings, chamfer)) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    const double widthM = settings.widthMm / 1000.0;
    const double heightM = settings.heightMm / 1000.0;
    const double sourceXM = settings.driverXmm / 1000.0;
    const double sourceYM = settings.driverYmm / 1000.0;
    const std::array<std::size_t, 4> edgeCounts =
        proportionalEdgeCounts(settings.edgeSourceCount, widthM, heightM);
    const std::vector<EdgePoint> edgePoints =
        rectangularEdgePoints(widthM, heightM, settings.edgeSourceCount);
    if (edgePoints.size() != settings.edgeSourceCount) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    BaffleChamfer45SideCorrection leftCorrection;
    BaffleChamfer45SideCorrection rightCorrection;
    const BaffleChamfer45SideCorrection* leftPtr = nullptr;
    const BaffleChamfer45SideCorrection* rightPtr = nullptr;
    if (!calculateChamferCorrections(
            settings, chamfer, leftCorrection, rightCorrection, leftPtr, rightPtr)) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    RectangularEdgeComponents pointComponents;
    if (!calculateRectangularEdgeComponentsForSource(
            edgePoints, edgeCounts, sourceXM, sourceYM, pointComponents)) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }
    const BaffleResponse pointResponse =
        composeRectangularResponse(pointComponents, leftPtr, rightPtr);
    if (pointResponse.status != BaffleResponseStatus::Valid) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    if (!finitePistonFitsChamfer(settings, chamfer, effectiveDriverDiameterCm)) {
        return pointResponse;
    }

    const double radiusM = effectiveDriverDiameterCm / 200.0;
    const std::array<SourcePoint, FinitePistonSourceCount> pistonPoints =
        finitePistonSamples(sourceXM, sourceYM, radiusM);
    RectangularEdgeComponents averagedComponents;
    for (const SourcePoint& pistonPoint : pistonPoints) {
        RectangularEdgeComponents sourceComponents;
        if (!calculateRectangularEdgeComponentsForSource(
                edgePoints, edgeCounts, pistonPoint.xM, pistonPoint.yM, sourceComponents)) {
            return pointResponse;
        }
        addRectangularEdgeComponents(averagedComponents, sourceComponents);
    }
    scaleRectangularEdgeComponents(
        averagedComponents, 1.0 / static_cast<double>(FinitePistonSourceCount));

    const BaffleResponse finiteResponse =
        composeRectangularResponse(averagedComponents, leftPtr, rightPtr);
    return finiteResponse.status == BaffleResponseStatus::Valid ? finiteResponse : pointResponse;
}

bool finitePistonFits(const BaffleSettings& settings, double effectiveDriverDiameterCm)
{
    if (!std::isfinite(effectiveDriverDiameterCm) || effectiveDriverDiameterCm <= 0.0) {
        return false;
    }

    const double radiusM = effectiveDriverDiameterCm / 200.0;
    const double widthM = settings.widthMm / 1000.0;
    const double heightM = settings.heightMm / 1000.0;
    const double sourceXM = settings.driverXmm / 1000.0;
    const double sourceYM = settings.driverYmm / 1000.0;
    const double minimumEdgeDistanceM = std::min(
        {sourceXM, widthM - sourceXM, sourceYM, heightM - sourceYM});

    // The supplement defines touching an edge as a point-source fallback.
    return std::isfinite(radiusM) && radiusM > 0.0 &&
           std::isfinite(minimumEdgeDistanceM) &&
           radiusM < minimumEdgeDistanceM;
}

std::array<SourcePoint, FinitePistonSourceCount> finitePistonSamples(double centerXM,
                                                                     double centerYM,
                                                                     double radiusM)
{
    std::array<SourcePoint, FinitePistonSourceCount> points{};
    for (std::size_t index = 0; index < FinitePistonSourceCount; ++index) {
        const double radialFraction =
            std::sqrt((static_cast<double>(index) + 0.5) /
                      static_cast<double>(FinitePistonSourceCount));
        const double radius = radiusM * radialFraction;
        const double theta = static_cast<double>(index) * GoldenAngleRad;
        points[index] = {
            centerXM + radius * std::cos(theta),
            centerYM + radius * std::sin(theta)
        };
    }
    return points;
}

BaffleResponse calculateSimpleBaffleStepResponse(const BaffleSettings& settings)
{
    const double midpointHz = simpleBaffleStepMidpointFrequencyHz(settings.widthMm);
    if (midpointHz <= 0.0) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    const double omega0 = TwoPi * midpointHz;
    const double sqrt2 = std::sqrt(2.0);
    const double omegaZero = omega0 / sqrt2;
    const double omegaPole = omega0 * sqrt2;

    BaffleResponse response = neutralResponse(BaffleResponseStatus::Valid);
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double frequencyHz = frequencies[sampleIndex];
        const double omega = TwoPi * frequencyHz;
        const std::complex<double> numerator{1.0, omega / omegaZero};
        const std::complex<double> denominator{1.0, omega / omegaPole};
        const std::complex<double> value = numerator / denominator;

        if (!finiteComplex(value)) {
            return neutralResponse(BaffleResponseStatus::InvalidParameters);
        }

        response.values[sampleIndex] = value;
    }

    return response;
}

// Patch 246 production LF magnitude hybrid for Free-field Rectangular Edge
// Diffraction.  The raw rectangular/finite-piston/chamfer solver remains the
// phase reference and retains all geometry dependence.  Only magnitude is
// blended toward the established width-only Simple Baffle Step below its
// midpoint using the selected Patch-245 n=2 trust law:
//
//   fBS = 115 / W[m]
//   r   = f / fBS
//   w   = r^2 / (1 + r^2)
//   D   = Dsimple + w * (Draw - Dsimple)   [dB]
//
// Scaling the raw complex sample by a positive real factor preserves its phase.
BaffleResponse applyFreeFieldRectangularLfMagnitudeHybrid(
    const BaffleSettings& settings,
    const BaffleResponse& rawResponse)
{
    if (rawResponse.status != BaffleResponseStatus::Valid) {
        return rawResponse;
    }

    const double midpointHz = simpleBaffleStepMidpointFrequencyHz(settings.widthMm);
    if (!std::isfinite(midpointHz) || midpointHz <= 0.0) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    const BaffleResponse simpleResponse = calculateSimpleBaffleStepResponse(settings);
    if (simpleResponse.status != BaffleResponseStatus::Valid) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    BaffleResponse hybridResponse = rawResponse;
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double frequencyHz = frequencies[sampleIndex];
        const double ratio = frequencyHz / midpointHz;
        const double ratioSquared = ratio * ratio;
        const double weight = ratioSquared / (1.0 + ratioSquared);
        const double simpleMagnitude = std::abs(simpleResponse.values[sampleIndex]);
        const double rawMagnitude = std::abs(rawResponse.values[sampleIndex]);

        if (!std::isfinite(weight) || weight < 0.0 || weight >= 1.0 ||
            !std::isfinite(simpleMagnitude) || simpleMagnitude <= 0.0 ||
            !std::isfinite(rawMagnitude) || rawMagnitude <= 0.0) {
            return neutralResponse(BaffleResponseStatus::InvalidParameters);
        }

        const double simpleDb = 20.0 * std::log10(simpleMagnitude);
        const double rawDb = 20.0 * std::log10(rawMagnitude);
        const double hybridDb = simpleDb + weight * (rawDb - simpleDb);
        const double hybridMagnitude = std::pow(10.0, hybridDb / 20.0);
        const double scale = hybridMagnitude / rawMagnitude;
        const std::complex<double> value = rawResponse.values[sampleIndex] * scale;

        if (!std::isfinite(hybridDb) || !std::isfinite(hybridMagnitude) ||
            hybridMagnitude <= 0.0 || !std::isfinite(scale) || scale <= 0.0 ||
            !finiteComplex(value)) {
            return neutralResponse(BaffleResponseStatus::InvalidParameters);
        }

        hybridResponse.values[sampleIndex] = value;
    }

    return hybridResponse;
}

BaffleResponse calculateRectangularEdgeDiffractionRawResponse(const BaffleSettings& settings,
                                                            double effectiveDriverDiameterCm)
{
    if (!validRectangularSettings(settings)) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    const double widthM = settings.widthMm / 1000.0;
    const double heightM = settings.heightMm / 1000.0;
    const double sourceXM = settings.driverXmm / 1000.0;
    const double sourceYM = settings.driverYmm / 1000.0;
    const std::vector<EdgePoint> edgePoints =
        rectangularEdgePoints(widthM, heightM, settings.edgeSourceCount);
    if (edgePoints.size() != settings.edgeSourceCount) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    // Always establish the exact Patch-193 point-source response first. It is
    // both the normal response for missing/unsuitable Dm and the local fallback
    // if anything inside the finite-piston refinement fails.
    BaffleResponse pointResponse;
    if (!calculateRectangularResponseForSource(edgePoints, sourceXM, sourceYM, pointResponse)) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    if (!finitePistonFits(settings, effectiveDriverDiameterCm)) {
        return pointResponse;
    }

    const double radiusM = effectiveDriverDiameterCm / 200.0;
    const std::array<SourcePoint, FinitePistonSourceCount> pistonPoints =
        finitePistonSamples(sourceXM, sourceYM, radiusM);

    BaffleResponse finiteResponse = neutralResponse(BaffleResponseStatus::Valid);
    finiteResponse.values.fill({0.0, 0.0});

    for (const SourcePoint& pistonPoint : pistonPoints) {
        BaffleResponse sourceResponse;
        if (!calculateRectangularResponseForSource(
                edgePoints, pistonPoint.xM, pistonPoint.yM, sourceResponse)) {
            return pointResponse;
        }

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            finiteResponse.values[sampleIndex] += sourceResponse.values[sampleIndex];
        }
    }

    const double inverseSourceCount = 1.0 / static_cast<double>(FinitePistonSourceCount);
    for (std::complex<double>& value : finiteResponse.values) {
        value *= inverseSourceCount;
        if (!finiteComplex(value)) {
            return pointResponse;
        }
    }

    return finiteResponse;
}

bool chamfer45ParametersFromSettings(const BaffleSettings& settings,
                                     BaffleChamfer45Parameters& chamfer)
{
    chamfer = BaffleChamfer45Parameters{};

    switch (settings.leftEdgeTreatment) {
    case BaffleSideEdgeTreatment::Sharp:
        break;
    case BaffleSideEdgeTreatment::Chamfer45:
        chamfer.leftSetbackMm = settings.leftChamferSetbackMm;
        break;
    default:
        return false;
    }

    switch (settings.rightEdgeTreatment) {
    case BaffleSideEdgeTreatment::Sharp:
        break;
    case BaffleSideEdgeTreatment::Chamfer45:
        chamfer.rightSetbackMm = settings.rightChamferSetbackMm;
        break;
    default:
        return false;
    }

    return true;
}

bool makeRigidFloorUnfoldedSettings(const BaffleSettings& settings,
                                    BaffleSettings& unfolded)
{
    if (!validRectangularSettings(settings) ||
        settings.leftEdgeTreatment != BaffleSideEdgeTreatment::Sharp ||
        settings.rightEdgeTreatment != BaffleSideEdgeTreatment::Sharp) {
        return false;
    }

    unfolded = settings;
    unfolded.boundaryCondition = BaffleBoundaryCondition::FreeField;
    unfolded.heightMm = 2.0 * settings.heightMm;
    if (!std::isfinite(unfolded.heightMm) || unfolded.heightMm <= settings.heightMm) {
        return false;
    }

    // Preserve approximately the original contour-source spacing after the
    // lower boundary has been unfolded into a doubled-height rectangle.
    const double originalPerimeterScale = settings.widthMm + settings.heightMm;
    const double unfoldedPerimeterScale = settings.widthMm + unfolded.heightMm;
    const double scaledCount = static_cast<double>(settings.edgeSourceCount) *
                               unfoldedPerimeterScale / originalPerimeterScale;
    if (!std::isfinite(scaledCount)) {
        return false;
    }

    const std::size_t unfoldedCount = static_cast<std::size_t>(std::llround(scaledCount));
    unfolded.edgeSourceCount = std::clamp(unfoldedCount,
                                          MinimumRectangularEdgeSourceCount,
                                          MaximumRectangularEdgeSourceCount);
    return true;
}

BaffleResponse calculateRectangularRigidFloorContactResponse(
    const BaffleSettings& settings,
    double effectiveDriverDiameterCm)
{
    if (settings.boundaryCondition !=
        BaffleBoundaryCondition::RigidFloorContactDiffractionOnly) {
        return neutralResponse(BaffleResponseStatus::UnsupportedModel);
    }

    BaffleSettings unfolded;
    if (!makeRigidFloorUnfoldedSettings(settings, unfolded)) {
        return neutralResponse(
            settings.leftEdgeTreatment == BaffleSideEdgeTreatment::Sharp &&
                    settings.rightEdgeTreatment == BaffleSideEdgeTreatment::Sharp
                ? BaffleResponseStatus::InvalidParameters
                : BaffleResponseStatus::UnsupportedModel);
    }

    // Keep the existing finite-piston fallback contract tied to the physical
    // cabinet. The unfolded mirror geometry must not make an oversized Dm
    // appear to fit merely because space was added below the floor plane.
    const double unfoldedDiameterCm =
        finitePistonFits(settings, effectiveDriverDiameterCm)
            ? effectiveDriverDiameterCm
            : 0.0;

    // The normalized engineering mode intentionally returns only the
    // diffraction-shape change. It does not add the coherent +6.02 dB rigid
    // boundary gain and has no listening-position floor-bounce path.
    return calculateRectangularEdgeDiffractionRawResponse(unfolded, unfoldedDiameterCm);
}

double rectangularDiameterCacheKey(const BaffleSettings& settings,
                                   double effectiveDriverDiameterCm)
{
    if (!settings.enabled || settings.model != BaffleModel::RectangularEdgeDiffraction ||
        !std::isfinite(effectiveDriverDiameterCm) || effectiveDriverDiameterCm <= 0.0) {
        return 0.0;
    }
    return effectiveDriverDiameterCm;
}
}

double simpleBaffleStepMidpointFrequencyHz(double widthMm)
{
    if (!std::isfinite(widthMm) || widthMm <= 0.0) {
        return 0.0;
    }

    const double widthM = widthMm / 1000.0;
    const double midpointHz = SimpleBaffleStepFrequencyConstantHzM / widthM;
    return std::isfinite(midpointHz) && midpointHz > 0.0 ? midpointHz : 0.0;
}

BaffleResponse calculateBaffleUnblendedRectangularResponseForDiagnostic(
    const BaffleSettings& settings,
    double effectiveDriverDiameterCm)
{
    if (!settings.enabled) {
        return neutralResponse(BaffleResponseStatus::Neutral);
    }
    if (settings.model != BaffleModel::RectangularEdgeDiffraction ||
        settings.boundaryCondition != BaffleBoundaryCondition::FreeField) {
        return neutralResponse(BaffleResponseStatus::UnsupportedModel);
    }

    BaffleChamfer45Parameters chamfer;
    if (!chamfer45ParametersFromSettings(settings, chamfer)) {
        return neutralResponse(BaffleResponseStatus::InvalidParameters);
    }

    if (chamfer.leftSetbackMm == 0.0 && chamfer.rightSetbackMm == 0.0) {
        return calculateRectangularEdgeDiffractionRawResponse(
            settings, effectiveDriverDiameterCm);
    }
    return calculateRectangularChamfer45Response(
        settings, chamfer, effectiveDriverDiameterCm);
}

BaffleResponse calculateBaffleResponse(const BaffleSettings& settings,
                                       double effectiveDriverDiameterCm)
{
    if (!settings.enabled) {
        return neutralResponse(BaffleResponseStatus::Neutral);
    }

    switch (settings.model) {
    case BaffleModel::SimpleBaffleStep:
        return calculateSimpleBaffleStepResponse(settings);

    case BaffleModel::RectangularEdgeDiffraction: {
        if (settings.boundaryCondition ==
            BaffleBoundaryCondition::RigidFloorContactDiffractionOnly) {
            return calculateRectangularRigidFloorContactResponse(
                settings, effectiveDriverDiameterCm);
        }
        if (settings.boundaryCondition != BaffleBoundaryCondition::FreeField) {
            return neutralResponse(BaffleResponseStatus::UnsupportedModel);
        }

        BaffleChamfer45Parameters chamfer;
        if (!chamfer45ParametersFromSettings(settings, chamfer)) {
            return neutralResponse(BaffleResponseStatus::InvalidParameters);
        }

        // Patch 246 retains the validated Sharp/finite-piston/chamfer engine as
        // an unblended raw reference, then applies the width-anchored n=2 LF
        // magnitude trust law only to the productive Free-field response.
        // The raw rectangular phase is preserved exactly.
        BaffleResponse rawResponse;
        if (chamfer.leftSetbackMm == 0.0 && chamfer.rightSetbackMm == 0.0) {
            rawResponse = calculateRectangularEdgeDiffractionRawResponse(
                settings, effectiveDriverDiameterCm);
        } else {
            rawResponse = calculateRectangularChamfer45Response(
                settings, chamfer, effectiveDriverDiameterCm);
        }
        return applyFreeFieldRectangularLfMagnitudeHybrid(settings, rawResponse);
    }
    }

    return neutralResponse(BaffleResponseStatus::UnsupportedModel);
}

BaffleRectangularBottomEdgeDiagnostic calculateBaffleRectangularBottomEdgeDiagnostic(
    const BaffleSettings& settings,
    double effectiveDriverDiameterCm)
{
    const auto neutralDiagnostic = [](BaffleResponseStatus status) {
        BaffleRectangularBottomEdgeDiagnostic diagnostic;
        diagnostic.freeField = neutralResponse(status);
        diagnostic.bottomEdgeOmitted = neutralResponse(status);
        return diagnostic;
    };

    if (!settings.enabled) {
        return neutralDiagnostic(BaffleResponseStatus::Neutral);
    }
    if (settings.model != BaffleModel::RectangularEdgeDiffraction) {
        return neutralDiagnostic(BaffleResponseStatus::UnsupportedModel);
    }
    if (settings.leftEdgeTreatment != BaffleSideEdgeTreatment::Sharp ||
        settings.rightEdgeTreatment != BaffleSideEdgeTreatment::Sharp) {
        return neutralDiagnostic(BaffleResponseStatus::UnsupportedModel);
    }
    if (!validRectangularSettings(settings)) {
        return neutralDiagnostic(BaffleResponseStatus::InvalidParameters);
    }

    const double widthM = settings.widthMm / 1000.0;
    const double heightM = settings.heightMm / 1000.0;
    const double sourceXM = settings.driverXmm / 1000.0;
    const double sourceYM = settings.driverYmm / 1000.0;
    const std::array<std::size_t, 4> edgeCounts =
        proportionalEdgeCounts(settings.edgeSourceCount, widthM, heightM);
    const std::vector<EdgePoint> edgePoints =
        rectangularEdgePoints(widthM, heightM, settings.edgeSourceCount);
    if (edgePoints.size() != settings.edgeSourceCount) {
        return neutralDiagnostic(BaffleResponseStatus::InvalidParameters);
    }

    const auto composeDiagnostic = [](const RectangularEdgeComponents& components) {
        BaffleRectangularBottomEdgeDiagnostic diagnostic;
        diagnostic.freeField = composeRectangularResponse(components, nullptr, nullptr, true);
        diagnostic.bottomEdgeOmitted =
            composeRectangularResponse(components, nullptr, nullptr, false);
        return diagnostic;
    };

    RectangularEdgeComponents pointComponents;
    if (!calculateRectangularEdgeComponentsForSource(
            edgePoints, edgeCounts, sourceXM, sourceYM, pointComponents)) {
        return neutralDiagnostic(BaffleResponseStatus::InvalidParameters);
    }
    const BaffleRectangularBottomEdgeDiagnostic pointDiagnostic =
        composeDiagnostic(pointComponents);
    if (pointDiagnostic.freeField.status != BaffleResponseStatus::Valid ||
        pointDiagnostic.bottomEdgeOmitted.status != BaffleResponseStatus::Valid) {
        return neutralDiagnostic(BaffleResponseStatus::InvalidParameters);
    }

    // Match the production finite-piston contract exactly: missing, invalid or
    // edge-touching Dm uses the point-source result; a failure during the M=73
    // refinement also falls back locally to the point-source diagnostic.
    if (!finitePistonFits(settings, effectiveDriverDiameterCm)) {
        return pointDiagnostic;
    }

    const double radiusM = effectiveDriverDiameterCm / 200.0;
    const std::array<SourcePoint, FinitePistonSourceCount> pistonPoints =
        finitePistonSamples(sourceXM, sourceYM, radiusM);
    RectangularEdgeComponents averagedComponents;
    for (const SourcePoint& pistonPoint : pistonPoints) {
        RectangularEdgeComponents sourceComponents;
        if (!calculateRectangularEdgeComponentsForSource(
                edgePoints, edgeCounts, pistonPoint.xM, pistonPoint.yM, sourceComponents)) {
            return pointDiagnostic;
        }
        addRectangularEdgeComponents(averagedComponents, sourceComponents);
    }
    scaleRectangularEdgeComponents(
        averagedComponents, 1.0 / static_cast<double>(FinitePistonSourceCount));

    const BaffleRectangularBottomEdgeDiagnostic finiteDiagnostic =
        composeDiagnostic(averagedComponents);
    if (finiteDiagnostic.freeField.status != BaffleResponseStatus::Valid ||
        finiteDiagnostic.bottomEdgeOmitted.status != BaffleResponseStatus::Valid) {
        return pointDiagnostic;
    }
    return finiteDiagnostic;
}

BaffleRectangularRigidFloorDiagnostic calculateBaffleRectangularRigidFloorDiagnostic(
    const BaffleSettings& settings,
    double effectiveDriverDiameterCm)
{
    const auto neutralDiagnostic = [](BaffleResponseStatus status) {
        BaffleRectangularRigidFloorDiagnostic diagnostic;
        diagnostic.freeField = neutralResponse(status);
        diagnostic.bottomEdgeOmitted = neutralResponse(status);
        diagnostic.imageGeometryRealSource = neutralResponse(status);
        diagnostic.imageGeometryMirrorSource = neutralResponse(status);
        diagnostic.imageGeometryNormalized = neutralResponse(status);
        diagnostic.imageGeometryRaw = neutralResponse(status);
        return diagnostic;
    };

    const BaffleRectangularBottomEdgeDiagnostic base =
        calculateBaffleRectangularBottomEdgeDiagnostic(settings, effectiveDriverDiameterCm);
    if (base.freeField.status != BaffleResponseStatus::Valid ||
        base.bottomEdgeOmitted.status != BaffleResponseStatus::Valid) {
        const BaffleResponseStatus status = base.freeField.status;
        return neutralDiagnostic(status);
    }

    // Ideal rigid-plane image construction for the lower cabinet edge. The
    // same unfolding helper is used by the Patch-220 production mode so this
    // diagnostic remains a direct reference for the productive response.
    BaffleSettings unfolded;
    if (!makeRigidFloorUnfoldedSettings(settings, unfolded)) {
        return neutralDiagnostic(BaffleResponseStatus::InvalidParameters);
    }

    const double unfoldedDiameterCm =
        finitePistonFits(settings, effectiveDriverDiameterCm)
            ? effectiveDriverDiameterCm
            : 0.0;

    // Real source in the unfolded geometry.
    unfolded.driverYmm = settings.driverYmm;
    const BaffleResponse realSource =
        calculateRectangularEdgeDiffractionRawResponse(unfolded, unfoldedDiameterCm);

    // Neumann (rigid) image source: same amplitude and phase, mirrored around
    // the floor plane. Computing it explicitly also gives a useful symmetry
    // check against numerical contour discretization.
    unfolded.driverYmm = unfolded.heightMm - settings.driverYmm;
    const BaffleResponse imageSource =
        calculateRectangularEdgeDiffractionRawResponse(unfolded, unfoldedDiameterCm);

    if (realSource.status != BaffleResponseStatus::Valid ||
        imageSource.status != BaffleResponseStatus::Valid) {
        return neutralDiagnostic(BaffleResponseStatus::InvalidParameters);
    }

    BaffleRectangularRigidFloorDiagnostic diagnostic;
    diagnostic.freeField = base.freeField;
    diagnostic.bottomEdgeOmitted = base.bottomEdgeOmitted;
    diagnostic.imageGeometryRealSource = realSource;
    diagnostic.imageGeometryMirrorSource = imageSource;
    diagnostic.imageGeometryNormalized = neutralResponse(BaffleResponseStatus::Valid);
    diagnostic.imageGeometryRaw = neutralResponse(BaffleResponseStatus::Valid);

    // In the exact unfolded continuum problem, the real and mirror source are
    // related by the geometry's mirror symmetry and therefore have the same
    // receiver-free/on-axis transfer response.  With finite contour sampling
    // (and especially the finite M=73 piston sample cloud) evaluating both
    // sources independently introduces a small numerical asymmetry.  Do not
    // average that quadrature artefact into the candidate floor response:
    // define the normalized diffraction-shape reference from one unfolded
    // source and retain the independently evaluated mirror source only as a
    // diagnostic symmetry check.
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const std::complex<double> normalized = realSource.values[sampleIndex];
        const std::complex<double> raw = 2.0 * normalized;
        if (!finiteComplex(raw) || !finiteComplex(normalized)) {
            return neutralDiagnostic(BaffleResponseStatus::InvalidParameters);
        }
        diagnostic.imageGeometryRaw.values[sampleIndex] = raw;
        diagnostic.imageGeometryNormalized.values[sampleIndex] = normalized;
    }

    return diagnostic;
}

BaffleResponse calculateBaffleResponseWithChamfer45(
    const BaffleSettings& settings,
    const BaffleChamfer45Parameters& chamfer,
    double effectiveDriverDiameterCm)
{
    // Stage 3A keeps Sharp exactly on the Patch-211 public path.  This is also
    // the exact zero-chamfer regression behavior; no truncated-order limit is
    // used to approximate a sharp edge.
    if (chamfer.leftSetbackMm == 0.0 && chamfer.rightSetbackMm == 0.0) {
        BaffleSettings sharpSettings = settings;
        sharpSettings.leftEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
        sharpSettings.rightEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
        return calculateBaffleResponse(sharpSettings, effectiveDriverDiameterCm);
    }

    if (!settings.enabled) {
        return neutralResponse(BaffleResponseStatus::Neutral);
    }
    if (settings.model != BaffleModel::RectangularEdgeDiffraction ||
        settings.boundaryCondition != BaffleBoundaryCondition::FreeField) {
        return neutralResponse(BaffleResponseStatus::UnsupportedModel);
    }

    const BaffleResponse rawResponse = calculateRectangularChamfer45Response(
        settings, chamfer, effectiveDriverDiameterCm);
    return applyFreeFieldRectangularLfMagnitudeHybrid(settings, rawResponse);
}

std::complex<double> applyBaffleResponseSample(
    const BaffleResponse& response,
    std::size_t sampleIndex,
    const std::complex<double>& signal)
{
    if (response.status != BaffleResponseStatus::Valid ||
        sampleIndex >= KFilterFrequencyCount) {
        return signal;
    }

    return signal * response.values[sampleIndex];
}

const BaffleResponse& BaffleResponseCache::responseFor(const BaffleSettings& settings,
                                                        double effectiveDriverDiameterCm)
{
    const double diameterCacheKey =
        rectangularDiameterCacheKey(settings, effectiveDriverDiameterCm);
    if (!m_valid || !m_cachedSettings.transferEquivalent(settings) ||
        m_cachedEffectiveDriverDiameterCm != diameterCacheKey) {
        m_response = calculateBaffleResponse(settings, effectiveDriverDiameterCm);
        m_cachedSettings = settings;
        m_cachedEffectiveDriverDiameterCm = diameterCacheKey;
        m_valid = true;
        ++m_generation;
    }

    return m_response;
}

std::uint64_t BaffleResponseCache::generation() const
{
    return m_generation;
}
