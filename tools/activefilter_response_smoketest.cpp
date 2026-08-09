/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "activefilterresponse.h"
#include "kfilterfrequencygrid.h"

#include <cmath>
#include <complex>
#include <iostream>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;

bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "active-filter response smoke test failed: " << message << '\n';
        return false;
    }
    return true;
}

bool near(double actual, double expected, double tolerance = 1.0e-10)
{
    return std::abs(actual - expected) <= tolerance;
}

bool nearComplex(const std::complex<double>& actual,
                 const std::complex<double>& expected,
                 double tolerance = 1.0e-10)
{
    return std::abs(actual - expected) <= tolerance;
}

ActiveFilterChain singleButterworth(ActiveFilterType type, int order, double cutoffHz)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(type);
    ActiveFilterSection& section = chain.section(index);
    if (type == ActiveFilterType::LowPass) {
        auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::Butterworth;
        parameters.order = order;
        parameters.frequencyHz = cutoffHz;
    } else {
        auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::Butterworth;
        parameters.order = order;
        parameters.frequencyHz = cutoffHz;
    }
    return chain;
}
}

int main()
{
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    constexpr std::size_t CutoffIndex = 75;
    const double cutoffHz = frequencies[CutoffIndex];
    const double invSqrt2 = 1.0 / std::sqrt(2.0);

    ActiveFilterChain disabled;
    disabled.setShowResponseInPlot(true);
    ActiveFilterResponse response = calculateActiveFilterResponse(disabled);
    if (!require(response.status == ActiveFilterResponseStatus::Neutral,
                 "disabled chain must be neutral") ||
        !require(!response.hasActiveSections, "disabled chain must report no active sections") ||
        !require(nearComplex(response.values[0], {1.0, 0.0}),
                 "disabled chain must produce 1+0j")) {
        return 1;
    }

    for (int order = 1; order <= 8; ++order) {
        const ActiveFilterChain lowPass = singleButterworth(ActiveFilterType::LowPass, order, cutoffHz);
        response = calculateActiveFilterResponse(lowPass);
        if (!require(response.status == ActiveFilterResponseStatus::Valid,
                     "Butterworth low-pass must be supported") ||
            !require(near(std::abs(response.values[CutoffIndex]), invSqrt2, 2.0e-10),
                     "low-pass cutoff magnitude must be -3.0103 dB")) {
            return 1;
        }

        for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
            const double ratio = frequencies[index] / cutoffHz;
            const double expectedMagnitude =
                1.0 / std::sqrt(1.0 + std::pow(ratio, 2.0 * order));
            if (!require(near(std::abs(response.values[index]), expectedMagnitude, 3.0e-10),
                         "low-pass magnitude does not match Butterworth reference")) {
                return 1;
            }
        }

        const ActiveFilterChain highPass = singleButterworth(ActiveFilterType::HighPass, order, cutoffHz);
        response = calculateActiveFilterResponse(highPass);
        if (!require(response.status == ActiveFilterResponseStatus::Valid,
                     "Butterworth high-pass must be supported") ||
            !require(near(std::abs(response.values[CutoffIndex]), invSqrt2, 2.0e-10),
                     "high-pass cutoff magnitude must be -3.0103 dB")) {
            return 1;
        }

        for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
            const double ratio = cutoffHz / frequencies[index];
            const double expectedMagnitude =
                1.0 / std::sqrt(1.0 + std::pow(ratio, 2.0 * order));
            if (!require(near(std::abs(response.values[index]), expectedMagnitude, 3.0e-10),
                         "high-pass magnitude does not match Butterworth reference")) {
                return 1;
            }
        }
    }

    response = calculateActiveFilterResponse(singleButterworth(ActiveFilterType::LowPass, 1, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.5, -0.5}, 2.0e-10),
                 "LP1 cutoff phase must be -45 degrees")) {
        return 1;
    }

    response = calculateActiveFilterResponse(singleButterworth(ActiveFilterType::HighPass, 1, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.5, 0.5}, 2.0e-10),
                 "HP1 cutoff phase must be +45 degrees")) {
        return 1;
    }

    response = calculateActiveFilterResponse(singleButterworth(ActiveFilterType::LowPass, 2, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.0, -invSqrt2}, 2.0e-10),
                 "LP2 cutoff phase must be -90 degrees")) {
        return 1;
    }

    response = calculateActiveFilterResponse(singleButterworth(ActiveFilterType::HighPass, 2, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.0, invSqrt2}, 2.0e-10),
                 "HP2 cutoff phase must be +90 degrees")) {
        return 1;
    }

    ActiveFilterChain cascade;
    cascade.setEnabled(true);
    cascade.addSection(ActiveFilterType::HighPass);
    cascade.addSection(ActiveFilterType::LowPass);
    auto& hp = std::get<ActiveFilterHighPassParameters>(cascade.section(0).parameters());
    hp.order = 2;
    hp.frequencyHz = frequencies[45];
    auto& lp = std::get<ActiveFilterLowPassParameters>(cascade.section(1).parameters());
    lp.order = 4;
    lp.frequencyHz = frequencies[105];
    response = calculateActiveFilterResponse(cascade);
    if (!require(response.status == ActiveFilterResponseStatus::Valid,
                 "supported cascade must be valid") ||
        !require(std::abs(response.values[0]) < std::abs(response.values[75]),
                 "high-pass part of cascade must attenuate low frequencies") ||
        !require(std::abs(response.values[149]) < std::abs(response.values[75]),
                 "low-pass part of cascade must attenuate high frequencies")) {
        return 1;
    }

    ActiveFilterChain unsupported;
    unsupported.setEnabled(true);
    unsupported.addSection(ActiveFilterType::LowPass);
    std::get<ActiveFilterLowPassParameters>(unsupported.section(0).parameters()).characteristic =
        ActiveFilterCharacteristic::Bessel;
    response = calculateActiveFilterResponse(unsupported);
    if (!require(response.status == ActiveFilterResponseStatus::Unsupported,
                 "Bessel must be reported as unsupported by the current transfer engine") ||
        !require(!response.plottable(), "unsupported response must not be plottable") ||
        !require(!std::isfinite(response.values[0].real()),
                 "unsupported response must not masquerade as neutral transfer data")) {
        return 1;
    }

    ActiveFilterChain invalid = singleButterworth(ActiveFilterType::LowPass, 2, cutoffHz);
    std::get<ActiveFilterLowPassParameters>(invalid.section(0).parameters()).frequencyHz = 0.0;
    response = calculateActiveFilterResponse(invalid);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "zero cutoff must be invalid") ||
        !require(!std::isfinite(response.values[0].real()),
                 "invalid response must not masquerade as neutral transfer data")) {
        return 1;
    }

    ActiveFilterChain ignoredUnsupported = singleButterworth(ActiveFilterType::LowPass, 2, cutoffHz);
    const std::size_t notchIndex = ignoredUnsupported.addSection(ActiveFilterType::Notch);
    ignoredUnsupported.section(notchIndex).setEnabled(false);
    response = calculateActiveFilterResponse(ignoredUnsupported);
    if (!require(response.status == ActiveFilterResponseStatus::Valid,
                 "disabled unsupported section must be neutral")) {
        return 1;
    }

    ActiveFilterResponseCache cache;
    ActiveFilterChain cached = singleButterworth(ActiveFilterType::LowPass, 2, cutoffHz);
    cache.responseFor(cached);
    const std::uint64_t firstGeneration = cache.generation();
    cache.responseFor(cached);
    if (!require(cache.generation() == firstGeneration,
                 "unchanged chain must reuse cached response")) {
        return 1;
    }

    cached.setShowResponseInPlot(true);
    cache.responseFor(cached);
    if (!require(cache.generation() == firstGeneration,
                 "plot-visibility flag must not invalidate transfer cache")) {
        return 1;
    }

    std::get<ActiveFilterLowPassParameters>(cached.section(0).parameters()).q = 9.0;
    cache.responseFor(cached);
    if (!require(cache.generation() == firstGeneration,
                 "unused Butterworth Q value must not invalidate transfer cache")) {
        return 1;
    }

    std::get<ActiveFilterLowPassParameters>(cached.section(0).parameters()).frequencyHz *= 1.1;
    cache.responseFor(cached);
    if (!require(cache.generation() == firstGeneration + 1,
                 "transfer parameter change must rebuild cache exactly once")) {
        return 1;
    }

    const std::complex<double> inputSignal{2.0, 0.0};
    response = calculateActiveFilterResponse(
        singleButterworth(ActiveFilterType::LowPass, 1, cutoffHz));
    if (!require(nearComplex(
                     applyActiveFilterResponseSample(response, CutoffIndex, inputSignal),
                     {1.0, -1.0},
                     2.0e-10),
                 "valid response must be multiplied into the complex driver sample")) {
        return 1;
    }

    response = calculateActiveFilterResponse(disabled);
    if (!require(nearComplex(
                     applyActiveFilterResponseSample(response, CutoffIndex, inputSignal),
                     inputSignal),
                 "neutral response must bypass the active-filter stage")) {
        return 1;
    }

    response = calculateActiveFilterResponse(unsupported);
    if (!require(nearComplex(
                     applyActiveFilterResponseSample(response, CutoffIndex, inputSignal),
                     inputSignal),
                 "unsupported response must bypass the complete active-filter stage")) {
        return 1;
    }

    ActiveFilterChain mixedUnsupported =
        singleButterworth(ActiveFilterType::LowPass, 1, cutoffHz);
    mixedUnsupported.addSection(ActiveFilterType::Notch);
    response = calculateActiveFilterResponse(mixedUnsupported);
    if (!require(response.status == ActiveFilterResponseStatus::Unsupported,
                 "mixed supported/unsupported chain must be reported as unsupported") ||
        !require(response.problemSectionIndex == 1,
                 "mixed chain must identify the first unsupported section") ||
        !require(nearComplex(
                     applyActiveFilterResponseSample(response, CutoffIndex, inputSignal),
                     inputSignal),
                 "mixed unsupported chain must bypass the complete active-filter stage")) {
        return 1;
    }

    response = calculateActiveFilterResponse(invalid);
    if (!require(nearComplex(
                     applyActiveFilterResponseSample(response, CutoffIndex, inputSignal),
                     inputSignal),
                 "invalid response must bypass the complete active-filter stage") ||
        !require(nearComplex(
                     applyActiveFilterResponseSample(
                         calculateActiveFilterResponse(singleButterworth(ActiveFilterType::LowPass, 1, cutoffHz)),
                         KFilterFrequencyCount,
                         inputSignal),
                     inputSignal),
                 "out-of-range response sample must be bypassed safely")) {
        return 1;
    }

    const double lp2PhaseDegrees = std::arg(
        calculateActiveFilterResponse(singleButterworth(ActiveFilterType::LowPass, 2, cutoffHz))
            .values[CutoffIndex]) * 180.0 / Pi;
    if (!require(near(lp2PhaseDegrees, -90.0, 1.0e-8),
                 "complex phase reference must remain unshifted")) {
        return 1;
    }

    std::cout << "active-filter response smoke test passed\n";
    return 0;
}
