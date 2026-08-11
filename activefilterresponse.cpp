/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "activefilterresponse.h"

#include <cmath>
#include <complex>
#include <limits>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;

ActiveFilterResponse neutralResponse()
{
    ActiveFilterResponse response;
    response.values.fill(std::complex<double>{1.0, 0.0});
    return response;
}


ActiveFilterResponse failedResponse(ActiveFilterResponseStatus status,
                                    std::size_t sectionIndex)
{
    ActiveFilterResponse response;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    response.values.fill(std::complex<double>{nan, nan});
    response.status = status;
    response.hasActiveSections = true;
    response.problemSectionIndex = sectionIndex;
    return response;
}

bool validButterworthParameters(int order, double cutoffHz)
{
    return order >= 1 && order <= 8 && std::isfinite(cutoffHz) && cutoffHz > 0.0;
}

bool validLinkwitzRileyParameters(int order, double cutoffHz)
{
    return (order == 2 || order == 4) &&
           std::isfinite(cutoffHz) && cutoffHz > 0.0;
}

bool validBandPassParameters(int order, double lowerFrequencyHz, double upperFrequencyHz)
{
    return order >= 1 && order <= 8 &&
           std::isfinite(lowerFrequencyHz) && lowerFrequencyHz > 0.0 &&
           std::isfinite(upperFrequencyHz) && upperFrequencyHz > lowerFrequencyHz;
}

bool validNotchParameters(double centerFrequencyHz, double q)
{
    return std::isfinite(centerFrequencyHz) && centerFrequencyHz > 0.0 &&
           std::isfinite(q) && q > 0.0;
}

bool validAllPassParameters(int order, double frequencyHz, double q)
{
    if ((order != 1 && order != 2) ||
        !std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
        return false;
    }

    // Q is part of the second-order denominator only.  Keep it deliberately
    // irrelevant for AP1, just as unused crossover Q metadata is ignored.
    return order == 1 || (std::isfinite(q) && q > 0.0);
}

bool validGainParameters(double gainDb)
{
    if (!std::isfinite(gainDb)) {
        return false;
    }

    const double linearGain = std::pow(10.0, gainDb / 20.0);
    return std::isfinite(linearGain) && linearGain > 0.0;
}

bool validDelayParameters(double delayMs)
{
    return std::isfinite(delayMs) && delayMs >= 0.0;
}

std::complex<double> allPassTransfer(int order,
                                     double frequencyHz,
                                     double centerFrequencyHz,
                                     double q)
{
    // Patch 188 All-pass convention, using normalized s = j*f/f0:
    // AP1: H(s) = (s - 1) / (s + 1)
    // AP2: H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1)
    // Both are exact unity-magnitude transfer functions for real f.
    const double ratio = frequencyHz / centerFrequencyHz;
    const std::complex<double> s{0.0, ratio};

    if (order == 1) {
        return (s - 1.0) / (s + 1.0);
    }

    const std::complex<double> sSquared = s * s;
    return (sSquared - s / q + 1.0) / (sSquared + s / q + 1.0);
}

std::complex<double> gainTransfer(double gainDb)
{
    return {std::pow(10.0, gainDb / 20.0), 0.0};
}

std::complex<double> delayTransfer(double frequencyHz, double delayMs)
{
    const double delaySeconds = delayMs / 1000.0;
    const double phaseRadians = -2.0 * Pi * frequencyHz * delaySeconds;
    return std::polar(1.0, phaseRadians);
}

std::complex<double> notchTransfer(double frequencyHz,
                                   double centerFrequencyHz,
                                   double q)
{
    // Normalized form of
    // H(s) = (s^2 + w0^2) / (s^2 + (w0/Q)s + w0^2), with s = j*w.
    // Using r = f/f0 keeps the computation well-scaled and yields exactly 0+0j
    // when a frequency-grid point is exactly equal to the notch center.
    const double ratio = frequencyHz / centerFrequencyHz;
    const double numerator = 1.0 - ratio * ratio;
    const std::complex<double> denominator{numerator, ratio / q};
    return numerator / denominator;
}

std::complex<double> butterworthTransfer(int order,
                                         double frequencyHz,
                                         double cutoffHz,
                                         bool highPass)
{
    const double normalizedFrequency = frequencyHz / cutoffHz;
    const std::complex<double> s{0.0, normalizedFrequency};
    std::complex<double> response{1.0, 0.0};

    for (int poleIndex = 0; poleIndex < order; ++poleIndex) {
        const double angle = Pi * static_cast<double>(2 * poleIndex + order + 1) /
                             static_cast<double>(2 * order);
        const std::complex<double> pole = std::polar(1.0, angle);
        if (highPass) {
            response *= s / (s - pole);
        } else {
            response *= (-pole) / (s - pole);
        }
    }

    return response;
}

std::complex<double> linkwitzRileyTransfer(int order,
                                           double frequencyHz,
                                           double cutoffHz,
                                           bool highPass)
{
    // A Linkwitz-Riley filter of order N is the cascade of two identical
    // Butterworth filters of order N/2.  Squaring the complete complex
    // Butterworth response preserves both magnitude and phase.
    const std::complex<double> butterworth =
        butterworthTransfer(order / 2, frequencyHz, cutoffHz, highPass);
    return butterworth * butterworth;
}

enum class SectionSupport
{
    Supported,
    Unsupported,
    Invalid
};

SectionSupport evaluateSection(const ActiveFilterSection& section,
                               const KFilterFrequencyGrid& frequencies,
                               std::array<std::complex<double>, KFilterFrequencyCount>& response)
{
    if (!section.enabled()) {
        response.fill(std::complex<double>{1.0, 0.0});
        return SectionSupport::Supported;
    }

    if (section.type() == ActiveFilterType::BandPass) {
        const auto& parameters = std::get<ActiveFilterBandPassParameters>(section.parameters());
        if (parameters.characteristic != ActiveFilterCharacteristic::Butterworth) {
            return SectionSupport::Unsupported;
        }
        if (!validBandPassParameters(parameters.order,
                                     parameters.lowerFrequencyHz,
                                     parameters.upperFrequencyHz)) {
            return SectionSupport::Invalid;
        }

        // KFilter defines a crossover-style band-pass as one Butterworth high-pass
        // at the lower cutoff multiplied by one Butterworth low-pass at the upper
        // cutoff. `order` therefore applies independently to both flanks.
        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            const double frequencyHz = frequencies[sampleIndex];
            if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
                return SectionSupport::Invalid;
            }
            response[sampleIndex] =
                butterworthTransfer(parameters.order,
                                    frequencyHz,
                                    parameters.lowerFrequencyHz,
                                    true) *
                butterworthTransfer(parameters.order,
                                    frequencyHz,
                                    parameters.upperFrequencyHz,
                                    false);
        }
        return SectionSupport::Supported;
    }

    if (section.type() == ActiveFilterType::Notch) {
        const auto& parameters = std::get<ActiveFilterNotchParameters>(section.parameters());
        if (!validNotchParameters(parameters.centerFrequencyHz, parameters.q)) {
            return SectionSupport::Invalid;
        }

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            const double frequencyHz = frequencies[sampleIndex];
            if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
                return SectionSupport::Invalid;
            }
            response[sampleIndex] = notchTransfer(frequencyHz,
                                                  parameters.centerFrequencyHz,
                                                  parameters.q);
        }
        return SectionSupport::Supported;
    }

    if (section.type() == ActiveFilterType::AllPass) {
        const auto& parameters = std::get<ActiveFilterAllPassParameters>(section.parameters());
        if (!validAllPassParameters(parameters.order, parameters.frequencyHz, parameters.q)) {
            return SectionSupport::Invalid;
        }

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            const double frequencyHz = frequencies[sampleIndex];
            if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
                return SectionSupport::Invalid;
            }
            response[sampleIndex] = allPassTransfer(parameters.order,
                                                    frequencyHz,
                                                    parameters.frequencyHz,
                                                    parameters.q);
        }
        return SectionSupport::Supported;
    }

    if (section.type() == ActiveFilterType::Gain) {
        const auto& parameters = std::get<ActiveFilterGainParameters>(section.parameters());
        if (!validGainParameters(parameters.gainDb)) {
            return SectionSupport::Invalid;
        }

        response.fill(gainTransfer(parameters.gainDb));
        return SectionSupport::Supported;
    }

    if (section.type() == ActiveFilterType::Delay) {
        const auto& parameters = std::get<ActiveFilterDelayParameters>(section.parameters());
        if (!validDelayParameters(parameters.delayMs)) {
            return SectionSupport::Invalid;
        }

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            const double frequencyHz = frequencies[sampleIndex];
            if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
                return SectionSupport::Invalid;
            }

            const double phaseRadians = -2.0 * Pi * frequencyHz * (parameters.delayMs / 1000.0);
            if (!std::isfinite(phaseRadians)) {
                return SectionSupport::Invalid;
            }
            response[sampleIndex] = delayTransfer(frequencyHz, parameters.delayMs);
        }
        return SectionSupport::Supported;
    }

    if (section.type() == ActiveFilterType::Polarity) {
        const auto& parameters = std::get<ActiveFilterPolarityParameters>(section.parameters());
        response.fill(parameters.inverted ? std::complex<double>{-1.0, 0.0}
                                          : std::complex<double>{1.0, 0.0});
        return SectionSupport::Supported;
    }

    int order = 0;
    double cutoffHz = 0.0;
    bool highPass = false;
    ActiveFilterCharacteristic characteristic = ActiveFilterCharacteristic::Butterworth;

    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        const auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        characteristic = parameters.characteristic;
        order = parameters.order;
        cutoffHz = parameters.frequencyHz;
        break;
    }
    case ActiveFilterType::HighPass: {
        const auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        characteristic = parameters.characteristic;
        order = parameters.order;
        cutoffHz = parameters.frequencyHz;
        highPass = true;
        break;
    }
    case ActiveFilterType::BandPass: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Notch: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::AllPass: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Gain: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Delay: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Polarity: // handled above; retained for exhaustive enum handling
        return SectionSupport::Unsupported;
    }

    if (characteristic != ActiveFilterCharacteristic::Butterworth &&
        characteristic != ActiveFilterCharacteristic::LinkwitzRiley) {
        return SectionSupport::Unsupported;
    }

    const bool linkwitzRiley = characteristic == ActiveFilterCharacteristic::LinkwitzRiley;
    if (linkwitzRiley ? !validLinkwitzRileyParameters(order, cutoffHz)
                      : !validButterworthParameters(order, cutoffHz)) {
        return SectionSupport::Invalid;
    }

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double frequencyHz = frequencies[sampleIndex];
        if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
            return SectionSupport::Invalid;
        }
        response[sampleIndex] = linkwitzRiley
            ? linkwitzRileyTransfer(order, frequencyHz, cutoffHz, highPass)
            : butterworthTransfer(order, frequencyHz, cutoffHz, highPass);
    }

    return SectionSupport::Supported;
}
}

ActiveFilterResponse calculateActiveFilterResponse(const ActiveFilterChain& chain)
{
    ActiveFilterResponse response = neutralResponse();
    if (!chain.enabled()) {
        return response;
    }

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    bool haveActiveSection = false;

    for (std::size_t sectionIndex = 0; sectionIndex < chain.sectionCount(); ++sectionIndex) {
        const ActiveFilterSection& section = chain.section(sectionIndex);
        if (!section.enabled()) {
            continue;
        }

        haveActiveSection = true;
        std::array<std::complex<double>, KFilterFrequencyCount> sectionResponse{};
        const SectionSupport support = evaluateSection(section, frequencies, sectionResponse);
        if (support != SectionSupport::Supported) {
            return failedResponse(support == SectionSupport::Unsupported
                                      ? ActiveFilterResponseStatus::Unsupported
                                      : ActiveFilterResponseStatus::InvalidParameters,
                                  sectionIndex);
        }

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            response.values[sampleIndex] *= sectionResponse[sampleIndex];
        }
    }

    response.hasActiveSections = haveActiveSection;
    response.status = haveActiveSection ? ActiveFilterResponseStatus::Valid
                                        : ActiveFilterResponseStatus::Neutral;
    return response;
}

std::complex<double> applyActiveFilterResponseSample(
    const ActiveFilterResponse& response,
    std::size_t sampleIndex,
    const std::complex<double>& signal)
{
    if (response.status != ActiveFilterResponseStatus::Valid ||
        sampleIndex >= KFilterFrequencyCount) {
        return signal;
    }

    return signal * response.values[sampleIndex];
}

const ActiveFilterResponse& ActiveFilterResponseCache::responseFor(const ActiveFilterChain& chain)
{
    if (!m_valid || !m_cachedChain.transferEquivalent(chain)) {
        m_response = calculateActiveFilterResponse(chain);
        m_cachedChain = chain;
        m_valid = true;
        ++m_generation;
    }

    return m_response;
}

std::uint64_t ActiveFilterResponseCache::generation() const
{
    return m_generation;
}
