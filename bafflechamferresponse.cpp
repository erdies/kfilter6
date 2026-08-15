/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "bafflechamferresponse.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double TwoPi = 2.0 * Pi;
constexpr double SpeedOfSoundMPerS = 343.0;
constexpr double ChamferNu = 4.0 / 5.0;
constexpr double ChamferOpenAngleRad = 5.0 * Pi / 4.0;
constexpr std::size_t FirstOrderElementCount = 240;
constexpr std::size_t MinimumHigherOrderElementCount = 64;
constexpr std::size_t MaximumHigherOrderElementCount = 120;
constexpr double HigherOrderHeightSetbackFactor = 0.6;

bool finiteComplex(const std::complex<double>& value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

std::size_t pairIndex(std::size_t first, std::size_t second, std::size_t count)
{
    return first * count + second;
}

std::size_t tripleIndex(std::size_t first,
                        std::size_t second,
                        std::size_t third,
                        std::size_t count)
{
    return (first * count + second) * count + third;
}

std::size_t higherOrderElementCount(double edgeHeightM, double setbackM)
{
    const double requested = std::ceil(HigherOrderHeightSetbackFactor * edgeHeightM / setbackM);
    if (!std::isfinite(requested)) {
        return MaximumHigherOrderElementCount;
    }

    const double bounded = std::clamp(
        requested,
        static_cast<double>(MinimumHigherOrderElementCount),
        static_cast<double>(MaximumHigherOrderElementCount));
    return static_cast<std::size_t>(bounded);
}

bool calculateFirstOrderWedge(std::array<std::complex<double>, KFilterFrequencyCount>& response,
                              double perpendicularSourceDistanceM,
                              double sourceYM,
                              double edgeHeightM,
                              double closedWedgeAngleRad)
{
    if (!std::isfinite(perpendicularSourceDistanceM) ||
        !std::isfinite(sourceYM) ||
        !std::isfinite(edgeHeightM) ||
        !std::isfinite(closedWedgeAngleRad) ||
        perpendicularSourceDistanceM <= 0.0 ||
        edgeHeightM <= 0.0) {
        return false;
    }

    const double acousticOpenAngle = TwoPi - closedWedgeAngleRad;
    const double nu = Pi / acousticOpenAngle;
    if (!std::isfinite(nu) || nu <= 0.0) {
        return false;
    }

    const double dy = edgeHeightM / static_cast<double>(FirstOrderElementCount);
    std::vector<double> amplitudes(FirstOrderElementCount);
    std::vector<double> pathExtras(FirstOrderElementCount);

    const double angleA = 1.5 * Pi * nu;
    const double angleB = 0.5 * Pi * nu;
    const double sinA = std::sin(angleA);
    const double cosA = std::cos(angleA);
    const double sinB = std::sin(angleB);
    const double cosB = std::cos(angleB);

    for (std::size_t index = 0; index < FirstOrderElementCount; ++index) {
        const double y = (static_cast<double>(index) + 0.5) * dy;
        const double z = y - sourceYM;
        const double sourceToEdge = std::hypot(perpendicularSourceDistanceM, z);
        // Analytic on-axis far-field limit.  The common 1/R pressure factor is
        // removed because C123 is a ratio of edge contributions; no arbitrary
        // observer distance enters the production transfer.
        const double raw = std::max(1.0, sourceToEdge / perpendicularSourceDistanceM);
        const double coshNuEta = std::cosh(nu * std::acosh(raw));
        const double denominatorA = coshNuEta - cosA;
        const double denominatorB = coshNuEta - cosB;
        if (!std::isfinite(denominatorA) || !std::isfinite(denominatorB) ||
            std::abs(denominatorA) <= std::numeric_limits<double>::epsilon() ||
            std::abs(denominatorB) <= std::numeric_limits<double>::epsilon()) {
            return false;
        }

        // theta_s = 0 and theta_r = pi/2 make terms 1/3 and 2/4 equal.
        const double beta = 2.0 * sinA / denominatorA + 2.0 * sinB / denominatorB;
        amplitudes[index] = beta / sourceToEdge * dy * (-nu / (4.0 * Pi));
        pathExtras[index] = sourceToEdge;

        if (!std::isfinite(amplitudes[index]) || !std::isfinite(pathExtras[index])) {
            return false;
        }
    }

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double waveNumber = TwoPi * frequencies[sampleIndex] / SpeedOfSoundMPerS;
        std::complex<double> value{0.0, 0.0};
        for (std::size_t index = 0; index < FirstOrderElementCount; ++index) {
            const double phase = -waveNumber * pathExtras[index];
            value += amplitudes[index] * std::complex<double>{std::cos(phase), std::sin(phase)};
        }
        if (!finiteComplex(value)) {
            return false;
        }
        response[sampleIndex] = value;
    }

    return true;
}

class HigherOrderChamferKernel
{
public:
    HigherOrderChamferKernel(double distanceFromOriginalEdgeM,
                             double sourceYM,
                             double edgeHeightM,
                             double setbackM)
        : m_count(higherOrderElementCount(edgeHeightM, setbackM)),
          m_setbackM(setbackM),
          m_distanceFromOriginalEdgeM(distanceFromOriginalEdgeM),
          m_sourceYM(sourceYM),
          m_edgeHeightM(edgeHeightM),
          m_edgeSeparationM(std::sqrt(2.0) * setbackM),
          m_dy(edgeHeightM / static_cast<double>(m_count)),
          m_c2(std::pow(ChamferNu * m_dy, 2.0) / (4.0 * Pi * Pi)),
          m_c3(m_c2 * (-ChamferNu * m_dy / (2.0 * Pi)))
    {
    }

    bool prepare()
    {
        if (!std::isfinite(m_setbackM) || !std::isfinite(m_distanceFromOriginalEdgeM) ||
            !std::isfinite(m_sourceYM) || !std::isfinite(m_edgeHeightM) ||
            m_setbackM <= 0.0 || m_edgeHeightM <= 0.0 ||
            m_distanceFromOriginalEdgeM <= m_setbackM ||
            m_count < MinimumHigherOrderElementCount ||
            m_count > MaximumHigherOrderElementCount) {
            return false;
        }

        m_y.resize(m_count);
        for (std::size_t index = 0; index < m_count; ++index) {
            m_y[index] = (static_cast<double>(index) + 0.5) * m_dy;
        }

        const std::size_t pairCount = m_count * m_count;
        const std::size_t tripleCount = pairCount * m_count;
        m_sourceGeometry.resize(pairCount);
        m_sourcePath.resize(pairCount);
        m_intermediateGeometry.resize(tripleCount);
        m_edgePath.resize(pairCount);
        m_receiver2Geometry.resize(pairCount);
        m_receiver2Path.resize(pairCount);
        m_receiver1Geometry.resize(pairCount);
        m_receiver1Path.resize(pairCount);

        const double sourceRadius = m_distanceFromOriginalEdgeM - m_setbackM;
        const double sourceAngle = ChamferNu * (Pi + ChamferOpenAngleRad);
        const double sourceAngleSin = std::sin(sourceAngle);
        const double sourceAngleCos = std::cos(sourceAngle);

        for (std::size_t first = 0; first < m_count; ++first) {
            for (std::size_t second = 0; second < m_count; ++second) {
                const std::size_t index = pairIndex(first, second, m_count);
                const double sourceToFirst =
                    std::hypot(sourceRadius, m_y[first] - m_sourceYM);
                const double edgeToEdge =
                    std::hypot(m_edgeSeparationM, m_y[first] - m_y[second]);
                const double rawSource = std::max(
                    1.0,
                    ((m_y[first] - m_sourceYM) * (m_y[first] - m_y[second]) +
                     sourceToFirst * edgeToEdge) /
                        (sourceRadius * m_edgeSeparationM));
                const double sourceCosh = std::cosh(ChamferNu * std::acosh(rawSource));
                const double sourceDenominator = sourceCosh - sourceAngleCos;
                if (!std::isfinite(sourceDenominator) ||
                    std::abs(sourceDenominator) <= std::numeric_limits<double>::epsilon()) {
                    return false;
                }
                const double sourceDirectivity = sourceAngleSin / sourceDenominator;
                m_sourceGeometry[index] =
                    sourceDirectivity / (sourceToFirst * edgeToEdge);
                m_sourcePath[index] = sourceToFirst + edgeToEdge;
                m_edgePath[index] = edgeToEdge;

                if (!prepareReceiverPair(index, edgeToEdge, true) ||
                    !prepareReceiverPair(index, edgeToEdge, false)) {
                    return false;
                }
            }
        }

        const double intermediateAngle = ChamferNu * Pi;
        const double intermediateSin = std::sin(intermediateAngle);
        const double intermediateCos = std::cos(intermediateAngle);
        for (std::size_t previous = 0; previous < m_count; ++previous) {
            for (std::size_t current = 0; current < m_count; ++current) {
                const double incoming =
                    std::hypot(m_edgeSeparationM, m_y[current] - m_y[previous]);
                for (std::size_t next = 0; next < m_count; ++next) {
                    const double outgoing =
                        std::hypot(m_edgeSeparationM, m_y[current] - m_y[next]);
                    const double raw = std::max(
                        1.0,
                        ((m_y[current] - m_y[previous]) *
                             (m_y[current] - m_y[next]) +
                         incoming * outgoing) /
                            (m_edgeSeparationM * m_edgeSeparationM));
                    const double coshTerm = std::cosh(ChamferNu * std::acosh(raw));
                    const double denominator = coshTerm - intermediateCos;
                    if (!std::isfinite(denominator) ||
                        std::abs(denominator) <= std::numeric_limits<double>::epsilon()) {
                        return false;
                    }
                    const double directivity = intermediateSin / denominator;
                    m_intermediateGeometry[
                        tripleIndex(previous, current, next, m_count)] =
                        directivity / outgoing;
                }
            }
        }

        return true;
    }

    bool evaluate(std::array<std::complex<double>, KFilterFrequencyCount>& higherOrders) const
    {
        if (m_sourceGeometry.empty() || m_intermediateGeometry.empty()) {
            return false;
        }

        const std::size_t pairCount = m_count * m_count;
        std::vector<std::complex<double>> order2State(pairCount);
        std::vector<std::complex<double>> order3State(pairCount);
        const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            const double waveNumber = TwoPi * frequencies[sampleIndex] / SpeedOfSoundMPerS;

            for (std::size_t first = 0; first < m_count; ++first) {
                for (std::size_t second = 0; second < m_count; ++second) {
                    const std::size_t index = pairIndex(first, second, m_count);
                    const double phase = -waveNumber * m_sourcePath[index];
                    order2State[index] =
                        m_sourceGeometry[index] *
                        std::complex<double>{std::cos(phase), std::sin(phase)};
                }
            }

            std::complex<double> h2{0.0, 0.0};
            for (std::size_t index = 0; index < pairCount; ++index) {
                const double phase = -waveNumber * m_receiver2Path[index];
                h2 += order2State[index] * m_receiver2Geometry[index] *
                      std::complex<double>{std::cos(phase), std::sin(phase)};
            }

            std::fill(order3State.begin(), order3State.end(), std::complex<double>{0.0, 0.0});
            for (std::size_t current = 0; current < m_count; ++current) {
                for (std::size_t next = 0; next < m_count; ++next) {
                    std::complex<double> contraction{0.0, 0.0};
                    for (std::size_t previous = 0; previous < m_count; ++previous) {
                        contraction +=
                            order2State[pairIndex(previous, current, m_count)] *
                            m_intermediateGeometry[
                                tripleIndex(previous, current, next, m_count)];
                    }
                    const double edgePhase =
                        -waveNumber * m_edgePath[pairIndex(current, next, m_count)];
                    order3State[pairIndex(current, next, m_count)] =
                        contraction *
                        std::complex<double>{std::cos(edgePhase), std::sin(edgePhase)};
                }
            }

            std::complex<double> h3{0.0, 0.0};
            for (std::size_t index = 0; index < pairCount; ++index) {
                const double phase = -waveNumber * m_receiver1Path[index];
                h3 += order3State[index] * m_receiver1Geometry[index] *
                      std::complex<double>{std::cos(phase), std::sin(phase)};
            }

            const std::complex<double> value = m_c2 * h2 + m_c3 * h3;
            if (!finiteComplex(value)) {
                return false;
            }
            higherOrders[sampleIndex] = value;
        }

        return true;
    }

private:
    bool prepareReceiverPair(std::size_t index,
                             double edgeToEdge,
                             bool lastEdgeIsSecond)
    {
        // Analytic receiver-side far-field limit.  For two parallel chamfer
        // edges, eta depends only on the inter-edge ray, while the on-axis
        // receiver angles tend to pi/4 at E2 and pi/2 at E1.  The rear E2
        // edge retains the additional setback path; E1 has zero receiver-side
        // excess path in the chosen front-baffle reference plane.
        const double raw = std::max(1.0, edgeToEdge / m_edgeSeparationM);
        const double coshTerm = std::cosh(ChamferNu * std::acosh(raw));
        const double previousTheta = lastEdgeIsSecond ? 0.0 : ChamferOpenAngleRad;
        const double receiverTheta = lastEdgeIsSecond ? Pi / 4.0 : Pi / 2.0;
        const double anglePlus = ChamferNu * (Pi + previousTheta + receiverTheta);
        const double angleMinus = ChamferNu * (Pi + previousTheta - receiverTheta);
        const double denominatorPlus = coshTerm - std::cos(anglePlus);
        const double denominatorMinus = coshTerm - std::cos(angleMinus);
        if (!std::isfinite(denominatorPlus) || !std::isfinite(denominatorMinus) ||
            std::abs(denominatorPlus) <= std::numeric_limits<double>::epsilon() ||
            std::abs(denominatorMinus) <= std::numeric_limits<double>::epsilon()) {
            return false;
        }

        const double receiverDirectivity =
            std::sin(anglePlus) / denominatorPlus +
            std::sin(angleMinus) / denominatorMinus;
        if (!std::isfinite(receiverDirectivity)) {
            return false;
        }
        if (lastEdgeIsSecond) {
            m_receiver2Geometry[index] = receiverDirectivity;
            m_receiver2Path[index] = m_setbackM;
        } else {
            m_receiver1Geometry[index] = receiverDirectivity;
            m_receiver1Path[index] = 0.0;
        }
        return true;
    }

    std::size_t m_count = 0;
    double m_setbackM = 0.0;
    double m_distanceFromOriginalEdgeM = 0.0;
    double m_sourceYM = 0.0;
    double m_edgeHeightM = 0.0;
    double m_edgeSeparationM = 0.0;
    double m_dy = 0.0;
    double m_c2 = 0.0;
    double m_c3 = 0.0;
    std::vector<double> m_y;
    std::vector<double> m_sourceGeometry;
    std::vector<double> m_sourcePath;
    std::vector<double> m_intermediateGeometry;
    std::vector<double> m_edgePath;
    std::vector<double> m_receiver2Geometry;
    std::vector<double> m_receiver2Path;
    std::vector<double> m_receiver1Geometry;
    std::vector<double> m_receiver1Path;
};
}

BaffleChamfer45SideCorrection calculateBaffleChamfer45SideCorrection(
    double distanceFromOriginalEdgeM,
    double sourceYM,
    double edgeHeightM,
    double setbackM)
{
    BaffleChamfer45SideCorrection correction;
    if (!std::isfinite(distanceFromOriginalEdgeM) ||
        !std::isfinite(sourceYM) ||
        !std::isfinite(edgeHeightM) ||
        !std::isfinite(setbackM) ||
        distanceFromOriginalEdgeM <= setbackM ||
        sourceYM <= 0.0 || sourceYM >= edgeHeightM ||
        edgeHeightM <= 0.0 || setbackM <= 0.0) {
        return correction;
    }

    std::array<std::complex<double>, KFilterFrequencyCount> h1Chamfer{};
    std::array<std::complex<double>, KFilterFrequencyCount> h1Sharp{};
    std::array<std::complex<double>, KFilterFrequencyCount> higherOrders{};

    if (!calculateFirstOrderWedge(h1Chamfer,
                                  distanceFromOriginalEdgeM - setbackM,
                                  sourceYM,
                                  edgeHeightM,
                                  3.0 * Pi / 4.0) ||
        !calculateFirstOrderWedge(h1Sharp,
                                  distanceFromOriginalEdgeM,
                                  sourceYM,
                                  edgeHeightM,
                                  Pi / 2.0)) {
        return correction;
    }

    HigherOrderChamferKernel higherOrderKernel(
        distanceFromOriginalEdgeM, sourceYM, edgeHeightM, setbackM);
    if (!higherOrderKernel.prepare() || !higherOrderKernel.evaluate(higherOrders)) {
        return correction;
    }

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double denominatorMagnitude = std::abs(h1Sharp[sampleIndex]);
        if (!std::isfinite(denominatorMagnitude) || denominatorMagnitude <= 1.0e-15) {
            return BaffleChamfer45SideCorrection{};
        }
        const std::complex<double> value =
            (h1Chamfer[sampleIndex] + higherOrders[sampleIndex]) / h1Sharp[sampleIndex];
        if (!finiteComplex(value)) {
            return BaffleChamfer45SideCorrection{};
        }
        correction.values[sampleIndex] = value;
    }

    correction.valid = true;
    return correction;
}
