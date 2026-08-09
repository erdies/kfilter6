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


ActiveFilterChain singleBandPass(int order, double lowerFrequencyHz, double upperFrequencyHz)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(ActiveFilterType::BandPass);
    auto& parameters =
        std::get<ActiveFilterBandPassParameters>(chain.section(index).parameters());
    parameters.characteristic = ActiveFilterCharacteristic::Butterworth;
    parameters.order = order;
    parameters.lowerFrequencyHz = lowerFrequencyHz;
    parameters.upperFrequencyHz = upperFrequencyHz;
    return chain;
}

ActiveFilterChain singleNotch(double centerFrequencyHz, double q)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(ActiveFilterType::Notch);
    auto& parameters = std::get<ActiveFilterNotchParameters>(chain.section(index).parameters());
    parameters.centerFrequencyHz = centerFrequencyHz;
    parameters.q = q;
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

    // Patch 182: canonical full-depth second-order notch. The center is chosen
    // directly from the shared 150-point grid so H(f0) must be exactly 0+0j.
    constexpr double notchQ = 4.0;
    response = calculateActiveFilterResponse(singleNotch(cutoffHz, notchQ));
    if (!require(response.status == ActiveFilterResponseStatus::Valid,
                 "second-order notch must be supported") ||
        !require(response.hasActiveSections,
                 "enabled notch must report an active section") ||
        !require(nearComplex(response.values[CutoffIndex], {0.0, 0.0}, 1.0e-15),
                 "notch center must be a full null")) {
        return 1;
    }

    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const double ratio = frequencies[index] / cutoffHz;
        const double numerator = 1.0 - ratio * ratio;
        const double expectedMagnitude =
            std::abs(numerator) / std::hypot(numerator, ratio / notchQ);
        if (!require(near(std::abs(response.values[index]), expectedMagnitude, 3.0e-10),
                     "notch magnitude does not match second-order reference")) {
            return 1;
        }
    }

    if (!require(response.values[CutoffIndex - 1].imag() < 0.0,
                 "notch phase must be negative immediately below the center") ||
        !require(response.values[CutoffIndex + 1].imag() > 0.0,
                 "notch phase must be positive immediately above the center") ||
        !require(std::abs(response.values.front()) > 0.99,
                 "notch must approach neutral transfer far below the center") ||
        !require(std::abs(response.values.back()) > 0.99,
                 "notch must approach neutral transfer far above the center")) {
        return 1;
    }

    const ActiveFilterResponse broadNotch = calculateActiveFilterResponse(singleNotch(cutoffHz, 0.7));
    const ActiveFilterResponse narrowNotch = calculateActiveFilterResponse(singleNotch(cutoffHz, 8.0));
    if (!require(std::abs(narrowNotch.values[CutoffIndex - 1]) >
                     std::abs(broadNotch.values[CutoffIndex - 1]),
                 "higher notch Q must produce a narrower stop band")) {
        return 1;
    }

    // Patch 183: crossover-style Butterworth band-pass = HP(lower) * LP(upper).
    // Order applies to each flank independently.
    constexpr std::size_t BandLowerIndex = 45;
    constexpr std::size_t BandUpperIndex = 105;
    constexpr std::size_t BandMiddleIndex = 75;
    const double bandLowerHz = frequencies[BandLowerIndex];
    const double bandUpperHz = frequencies[BandUpperIndex];
    for (int order = 1; order <= 8; ++order) {
        const ActiveFilterResponse bandPass =
            calculateActiveFilterResponse(singleBandPass(order, bandLowerHz, bandUpperHz));
        const ActiveFilterResponse referenceHighPass = calculateActiveFilterResponse(
            singleButterworth(ActiveFilterType::HighPass, order, bandLowerHz));
        const ActiveFilterResponse referenceLowPass = calculateActiveFilterResponse(
            singleButterworth(ActiveFilterType::LowPass, order, bandUpperHz));

        if (!require(bandPass.status == ActiveFilterResponseStatus::Valid,
                     "Butterworth band-pass must be supported") ||
            !require(bandPass.hasActiveSections,
                     "enabled band-pass must report an active section")) {
            return 1;
        }

        for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
            const std::complex<double> expected =
                referenceHighPass.values[index] * referenceLowPass.values[index];
            if (!require(nearComplex(bandPass.values[index], expected, 5.0e-10),
                         "band-pass complex response must equal HP(lower) * LP(upper)")) {
                return 1;
            }
        }

        if (!require(std::abs(bandPass.values.front()) <
                         std::abs(bandPass.values[BandMiddleIndex]),
                     "band-pass must attenuate below the lower cutoff") ||
            !require(std::abs(bandPass.values.back()) <
                         std::abs(bandPass.values[BandMiddleIndex]),
                     "band-pass must attenuate above the upper cutoff")) {
            return 1;
        }

        if (!require(std::abs(bandPass.values[BandMiddleIndex].imag()) < 2.0e-9,
                     "log-symmetric Butterworth band-pass flanks must cancel phase at geometric center")) {
            return 1;
        }
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

    ActiveFilterChain invalidBandPass = singleBandPass(2, bandLowerHz, bandUpperHz);
    auto& invalidBandPassParameters =
        std::get<ActiveFilterBandPassParameters>(invalidBandPass.section(0).parameters());
    invalidBandPassParameters.lowerFrequencyHz = bandUpperHz;
    response = calculateActiveFilterResponse(invalidBandPass);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "band-pass lower cutoff must be below upper cutoff")) {
        return 1;
    }
    invalidBandPassParameters.lowerFrequencyHz = bandLowerHz;
    invalidBandPassParameters.upperFrequencyHz = bandUpperHz;
    invalidBandPassParameters.order = 0;
    response = calculateActiveFilterResponse(invalidBandPass);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "band-pass order below one must be invalid")) {
        return 1;
    }
    invalidBandPassParameters.order = 2;
    invalidBandPassParameters.upperFrequencyHz = 0.0;
    response = calculateActiveFilterResponse(invalidBandPass);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "band-pass upper cutoff must be positive")) {
        return 1;
    }

    ActiveFilterChain unsupportedBandPass = singleBandPass(2, bandLowerHz, bandUpperHz);
    std::get<ActiveFilterBandPassParameters>(unsupportedBandPass.section(0).parameters()).characteristic =
        ActiveFilterCharacteristic::LinkwitzRiley;
    response = calculateActiveFilterResponse(unsupportedBandPass);
    if (!require(response.status == ActiveFilterResponseStatus::Unsupported,
                 "non-Butterworth band-pass must remain unsupported")) {
        return 1;
    }

    ActiveFilterChain invalidNotch = singleNotch(cutoffHz, 0.0);
    response = calculateActiveFilterResponse(invalidNotch);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "zero notch Q must be invalid")) {
        return 1;
    }
    auto& invalidNotchParameters =
        std::get<ActiveFilterNotchParameters>(invalidNotch.section(0).parameters());
    invalidNotchParameters.q = 1.0;
    invalidNotchParameters.centerFrequencyHz = 0.0;
    response = calculateActiveFilterResponse(invalidNotch);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "zero notch center frequency must be invalid")) {
        return 1;
    }

    ActiveFilterChain ignoredUnsupported = singleButterworth(ActiveFilterType::LowPass, 2, cutoffHz);
    const std::size_t gainIndex = ignoredUnsupported.addSection(ActiveFilterType::Gain);
    ignoredUnsupported.section(gainIndex).setEnabled(false);
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

    ActiveFilterResponseCache bandPassCache;
    ActiveFilterChain cachedBandPass = singleBandPass(2, bandLowerHz, bandUpperHz);
    bandPassCache.responseFor(cachedBandPass);
    const std::uint64_t bandPassGeneration = bandPassCache.generation();
    std::get<ActiveFilterBandPassParameters>(cachedBandPass.section(0).parameters()).q = 9.0;
    bandPassCache.responseFor(cachedBandPass);
    if (!require(bandPassCache.generation() == bandPassGeneration,
                 "unused Butterworth band-pass Q must not invalidate transfer cache")) {
        return 1;
    }
    std::get<ActiveFilterBandPassParameters>(cachedBandPass.section(0).parameters()).upperFrequencyHz *= 1.1;
    bandPassCache.responseFor(cachedBandPass);
    if (!require(bandPassCache.generation() == bandPassGeneration + 1,
                 "band-pass cutoff change must invalidate transfer cache")) {
        return 1;
    }

    ActiveFilterResponseCache notchCache;
    ActiveFilterChain cachedNotch = singleNotch(cutoffHz, 3.0);
    notchCache.responseFor(cachedNotch);
    const std::uint64_t notchGeneration = notchCache.generation();
    std::get<ActiveFilterNotchParameters>(cachedNotch.section(0).parameters()).gainDb = -12.0;
    notchCache.responseFor(cachedNotch);
    if (!require(notchCache.generation() == notchGeneration,
                 "reserved notch gain metadata must not invalidate transfer cache")) {
        return 1;
    }
    std::get<ActiveFilterNotchParameters>(cachedNotch.section(0).parameters()).q = 6.0;
    notchCache.responseFor(cachedNotch);
    if (!require(notchCache.generation() == notchGeneration + 1,
                 "notch Q change must invalidate transfer cache")) {
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

    response = calculateActiveFilterResponse(singleNotch(cutoffHz, 4.0));
    if (!require(nearComplex(
                     applyActiveFilterResponseSample(response, CutoffIndex, inputSignal),
                     {0.0, 0.0},
                     1.0e-15),
                 "notch center must null the complex driver sample")) {
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
    mixedUnsupported.addSection(ActiveFilterType::Gain);
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
