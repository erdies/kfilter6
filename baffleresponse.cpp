/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleresponse.h"

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

BaffleResponse calculateRectangularEdgeDiffractionResponse(const BaffleSettings& settings,
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

BaffleResponse calculateBaffleResponse(const BaffleSettings& settings,
                                       double effectiveDriverDiameterCm)
{
    if (!settings.enabled) {
        return neutralResponse(BaffleResponseStatus::Neutral);
    }

    switch (settings.model) {
    case BaffleModel::SimpleBaffleStep:
        return calculateSimpleBaffleStepResponse(settings);

    case BaffleModel::RectangularEdgeDiffraction:
        return calculateRectangularEdgeDiffractionResponse(settings,
                                                           effectiveDriverDiameterCm);
    }

    return neutralResponse(BaffleResponseStatus::UnsupportedModel);
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
