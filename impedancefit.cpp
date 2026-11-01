/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "impedancefit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

namespace
{
constexpr double TwoPi = 6.283185307179586;
constexpr int MaximumIterations = 200;

struct FitPoint
{
    double frequencyHz = 0.0;
    std::complex<double> measured;
    double magnitude = 0.0;
};

// Parameter order in the log vector: Lsp, R2, L2[, Rdc].
struct ParameterBounds
{
    double minimum;
    double maximum;
};
constexpr std::array<ParameterBounds, 4> Bounds{{
    {1.0e-8, 1.0},     // Lsp [H]
    {1.0e-3, 1.0e4},   // R2 [Ohm], matches the dialog range
    {1.0e-8, 1.0},     // L2 [H]
    {1.0e-3, 1.0e3},   // Rdc [Ohm]
}};

class FitProblem
{
public:
    FitProblem(const driver& drv, std::vector<FitPoint> points, bool magnitudeOnly, bool fitRdc)
        : m_model(drv), m_points(std::move(points)), m_magnitudeOnly(magnitudeOnly), m_fitRdc(fitRdc)
    {
    }

    int parameterCount() const { return m_fitRdc ? 4 : 3; }

    std::vector<double> clamp(std::vector<double> logParameters) const
    {
        for (std::size_t index = 0; index < logParameters.size(); ++index) {
            logParameters[index] = std::clamp(logParameters[index],
                                              std::log(Bounds[index].minimum),
                                              std::log(Bounds[index].maximum));
        }
        return logParameters;
    }

    void apply(const std::vector<double>& logParameters)
    {
        m_model.setLsp(std::exp(logParameters[0]));
        m_model.setR2(std::exp(logParameters[1]));
        m_model.setL2(std::exp(logParameters[2]));
        if (m_fitRdc) {
            m_model.setRdc(std::exp(logParameters[3]));
        }
    }

    std::vector<double> residuals(const std::vector<double>& logParameters)
    {
        apply(logParameters);
        return residualsOf(m_model);
    }

    std::vector<double> residualsOf(driver& model) const
    {
        std::vector<double> result;
        result.reserve(m_points.size() * 2);
        for (const FitPoint& point : m_points) {
            const std::complex<double> simulated = model.electricalImpedance(point.frequencyHz);
            if (m_magnitudeOnly) {
                result.push_back((std::abs(simulated) - point.magnitude) / point.magnitude);
            } else {
                const std::complex<double> error = (simulated - point.measured) / point.magnitude;
                result.push_back(error.real());
                result.push_back(error.imag());
            }
        }
        return result;
    }

    double rmsPercent(double sumOfSquares) const
    {
        return 100.0 * std::sqrt(sumOfSquares / static_cast<double>(m_points.size()));
    }

private:
    driver m_model;
    std::vector<FitPoint> m_points;
    bool m_magnitudeOnly = false;
    bool m_fitRdc = false;
};

double sumOfSquares(const std::vector<double>& values)
{
    double sum = 0.0;
    for (double value : values) {
        sum += value * value;
    }
    return std::isfinite(sum) ? sum : std::numeric_limits<double>::infinity();
}

// Solves A x = b in place by Gaussian elimination with partial pivoting.
bool solveLinearSystem(std::vector<std::vector<double>> a, std::vector<double> b, std::vector<double>& x)
{
    const std::size_t n = b.size();
    for (std::size_t column = 0; column < n; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < n; ++row) {
            if (std::abs(a[row][column]) > std::abs(a[pivot][column])) {
                pivot = row;
            }
        }
        if (!(std::abs(a[pivot][column]) > 1.0e-300)) {
            return false;
        }
        std::swap(a[pivot], a[column]);
        std::swap(b[pivot], b[column]);
        for (std::size_t row = column + 1; row < n; ++row) {
            const double factor = a[row][column] / a[column][column];
            for (std::size_t k = column; k < n; ++k) {
                a[row][k] -= factor * a[column][k];
            }
            b[row] -= factor * b[column];
        }
    }

    x.assign(n, 0.0);
    for (std::size_t row = n; row-- > 0;) {
        double sum = b[row];
        for (std::size_t k = row + 1; k < n; ++k) {
            sum -= a[row][k] * x[k];
        }
        x[row] = sum / a[row][row];
    }
    return std::all_of(x.cbegin(), x.cend(), [](double value) { return std::isfinite(value); });
}

struct LevenbergMarquardtResult
{
    std::vector<double> logParameters;
    double sumOfSquares = std::numeric_limits<double>::infinity();
    int iterations = 0;
};

LevenbergMarquardtResult levenbergMarquardt(FitProblem& problem, std::vector<double> start)
{
    LevenbergMarquardtResult result;
    std::vector<double> parameters = problem.clamp(std::move(start));
    std::vector<double> residuals = problem.residuals(parameters);
    double cost = sumOfSquares(residuals);
    double lambda = 1.0e-3;
    const std::size_t n = parameters.size();
    constexpr double StepSize = 1.0e-6;

    int iteration = 0;
    for (; iteration < MaximumIterations && std::isfinite(cost); ++iteration) {
        // Forward-difference Jacobian in log-parameter space.
        std::vector<std::vector<double>> jacobian(n);
        for (std::size_t k = 0; k < n; ++k) {
            std::vector<double> shifted = parameters;
            shifted[k] += StepSize;
            const std::vector<double> shiftedResiduals = problem.residuals(shifted);
            jacobian[k].resize(residuals.size());
            for (std::size_t i = 0; i < residuals.size(); ++i) {
                jacobian[k][i] = (shiftedResiduals[i] - residuals[i]) / StepSize;
            }
        }

        std::vector<std::vector<double>> normal(n, std::vector<double>(n, 0.0));
        std::vector<double> gradient(n, 0.0);
        for (std::size_t row = 0; row < n; ++row) {
            for (std::size_t column = 0; column < n; ++column) {
                for (std::size_t i = 0; i < residuals.size(); ++i) {
                    normal[row][column] += jacobian[row][i] * jacobian[column][i];
                }
            }
            for (std::size_t i = 0; i < residuals.size(); ++i) {
                gradient[row] -= jacobian[row][i] * residuals[i];
            }
        }

        bool improved = false;
        bool converged = false;
        while (lambda < 1.0e12) {
            std::vector<std::vector<double>> damped = normal;
            for (std::size_t k = 0; k < n; ++k) {
                damped[k][k] += lambda * std::max(normal[k][k], 1.0e-12);
            }

            std::vector<double> step;
            if (!solveLinearSystem(damped, gradient, step)) {
                lambda *= 10.0;
                continue;
            }

            std::vector<double> candidate = parameters;
            for (std::size_t k = 0; k < n; ++k) {
                candidate[k] += step[k];
            }
            candidate = problem.clamp(std::move(candidate));
            std::vector<double> candidateResiduals = problem.residuals(candidate);
            const double candidateCost = sumOfSquares(candidateResiduals);
            if (candidateCost < cost) {
                const double maximumStep = std::abs(*std::max_element(
                    step.cbegin(), step.cend(),
                    [](double lhs, double rhs) { return std::abs(lhs) < std::abs(rhs); }));
                converged = (cost - candidateCost) <= 1.0e-12 * cost || maximumStep < 1.0e-9;
                parameters = std::move(candidate);
                residuals = std::move(candidateResiduals);
                cost = candidateCost;
                lambda = std::max(lambda / 10.0, 1.0e-12);
                improved = true;
                break;
            }
            lambda *= 10.0;
        }

        if (!improved || converged) {
            ++iteration;
            break;
        }
    }

    result.logParameters = parameters;
    result.sumOfSquares = cost;
    result.iterations = iteration;
    return result;
}

double geometricMean(double lhs, double rhs)
{
    return std::sqrt(lhs * rhs);
}

// Start values from the measured effective coil inductance Im{Zcoil}/w, where
// Zcoil is the measurement minus the model's motional/enclosure part.
bool effectiveInductanceStart(const driver& drv,
                              const std::vector<FitPoint>& points,
                              std::vector<double>& start)
{
    driver coilless(drv);
    coilless.setLsp(0.0);
    coilless.setR2(0.0);
    coilless.setL2(0.0);

    std::vector<double> effectiveInductance;
    effectiveInductance.reserve(points.size());
    for (const FitPoint& point : points) {
        const std::complex<double> motional =
            coilless.electricalImpedance(point.frequencyHz) - std::complex<double>{drv.getRdc(), 0.0};
        const std::complex<double> coil = point.measured - motional;
        effectiveInductance.push_back(coil.imag() / (TwoPi * point.frequencyHz));
    }

    const auto maximum = std::max_element(effectiveInductance.cbegin(), effectiveInductance.cend());
    const double lowInductance = *maximum;
    if (!(lowInductance > 0.0)) {
        return false;
    }

    double highInductance = 0.7 * effectiveInductance.back();
    if (!(highInductance > 0.0) || highInductance >= lowInductance) {
        highInductance = 0.5 * lowInductance;
    }
    const double eddyInductance = std::max(lowInductance - highInductance, 0.25 * lowInductance);

    // Corner frequency: where Leff has fallen halfway from its maximum to Lsp.
    const double halfway = 0.5 * (lowInductance + highInductance);
    double cornerFrequencyHz = geometricMean(points.front().frequencyHz, points.back().frequencyHz);
    for (auto it = maximum; it != effectiveInductance.cend(); ++it) {
        if (*it <= halfway) {
            cornerFrequencyHz = points[static_cast<std::size_t>(it - effectiveInductance.cbegin())].frequencyHz;
            break;
        }
    }

    start = {std::log(highInductance),
             std::log(TwoPi * cornerFrequencyHz * eddyInductance),
             std::log(eddyInductance)};
    return true;
}

std::vector<double> currentValuesStart(const driver& drv, const std::vector<FitPoint>& points)
{
    const double lsp = drv.getLsp() > 0.0 ? drv.getLsp() : 1.0e-4;
    const double l2 = drv.getL2() > 0.0 ? drv.getL2() : lsp;
    const double r2 = drv.getR2() > 0.0
                          ? drv.getR2()
                          : TwoPi * geometricMean(points.front().frequencyHz, points.back().frequencyHz) * l2;
    return {std::log(lsp), std::log(r2), std::log(l2)};
}
}

void defaultImpedanceFitBand(const driver& drv,
                             const KFilterImpedanceReferenceCurve& zma,
                             double& minimumFrequencyHz,
                             double& maximumFrequencyHz)
{
    minimumFrequencyHz = 0.0;
    maximumFrequencyHz = 0.0;
    if (zma.isEmpty()) {
        return;
    }

    const double first = zma.constFirst().frequencyHz;
    const double last = zma.constLast().frequencyHz;
    maximumFrequencyHz = last;
    minimumFrequencyHz = drv.getF0() > 0.0 ? std::clamp(2.0 * drv.getF0(), first, last) : first;
    if (minimumFrequencyHz >= last) {
        minimumFrequencyHz = first;
    }
}

KFilterImpedanceFitResult fitVoiceCoilToImpedance(const driver& drv,
                                                  const KFilterImpedanceReferenceCurve& zma,
                                                  const KFilterImpedanceFitOptions& options)
{
    KFilterImpedanceFitResult result;
    result.rdcOhm = drv.getRdc();
    result.lspH = drv.getLsp();
    result.r2Ohm = drv.getR2();
    result.l2H = drv.getL2();

    if (!(options.minimumFrequencyHz > 0.0) ||
        !(options.maximumFrequencyHz > options.minimumFrequencyHz)) {
        result.errorMessage = QStringLiteral("The fit band must have a positive lower frequency below the upper frequency.");
        return result;
    }

    std::vector<FitPoint> points;
    bool magnitudeOnly = false;
    for (const KFilterImpedancePoint& point : zma) {
        if (point.frequencyHz < options.minimumFrequencyHz ||
            point.frequencyHz > options.maximumFrequencyHz ||
            !(point.magnitudeOhm > 0.0)) {
            continue;
        }
        if (!std::isfinite(point.phaseDeg)) {
            magnitudeOnly = true;
        }
        const double phaseRad = std::isfinite(point.phaseDeg) ? point.phaseDeg * TwoPi / 360.0 : 0.0;
        points.push_back(FitPoint{point.frequencyHz,
                                  std::polar(point.magnitudeOhm, phaseRad),
                                  point.magnitudeOhm});
    }

    const int parameterCount = options.fitRdc ? 4 : 3;
    if (static_cast<int>(points.size()) < parameterCount + 2) {
        result.errorMessage = QStringLiteral("The fit band contains only %1 usable ZMA point(s); at least %2 are needed.")
                                  .arg(points.size())
                                  .arg(parameterCount + 2);
        return result;
    }

    result.pointCount = static_cast<int>(points.size());
    result.magnitudeOnly = magnitudeOnly;

    FitProblem problem(drv, points, magnitudeOnly, options.fitRdc);
    driver initialModel(drv);
    result.initialRmsPercent = problem.rmsPercent(sumOfSquares(problem.residualsOf(initialModel)));

    std::vector<std::vector<double>> starts{currentValuesStart(drv, points)};
    std::vector<double> inductanceStart;
    if (!magnitudeOnly && effectiveInductanceStart(drv, points, inductanceStart)) {
        starts.push_back(inductanceStart);
    }

    LevenbergMarquardtResult best;
    for (std::vector<double> start : starts) {
        if (options.fitRdc) {
            start.push_back(std::log(std::max(drv.getRdc(), Bounds[3].minimum)));
        }
        LevenbergMarquardtResult candidate = levenbergMarquardt(problem, std::move(start));
        if (candidate.sumOfSquares < best.sumOfSquares) {
            best = std::move(candidate);
        }
    }

    if (!std::isfinite(best.sumOfSquares) || best.logParameters.empty()) {
        result.errorMessage = QStringLiteral("The fit did not converge to a finite result.");
        return result;
    }

    result.success = true;
    result.lspH = std::exp(best.logParameters[0]);
    result.r2Ohm = std::exp(best.logParameters[1]);
    result.l2H = std::exp(best.logParameters[2]);
    if (options.fitRdc) {
        result.rdcOhm = std::exp(best.logParameters[3]);
    }
    result.finalRmsPercent = problem.rmsPercent(best.sumOfSquares);
    result.iterations = best.iterations;
    return result;
}
