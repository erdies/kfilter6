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

    int order = 0;
    double cutoffHz = 0.0;
    bool highPass = false;

    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        const auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        if (parameters.characteristic != ActiveFilterCharacteristic::Butterworth) {
            return SectionSupport::Unsupported;
        }
        order = parameters.order;
        cutoffHz = parameters.frequencyHz;
        break;
    }
    case ActiveFilterType::HighPass: {
        const auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        if (parameters.characteristic != ActiveFilterCharacteristic::Butterworth) {
            return SectionSupport::Unsupported;
        }
        order = parameters.order;
        cutoffHz = parameters.frequencyHz;
        highPass = true;
        break;
    }
    case ActiveFilterType::BandPass: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Notch: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::AllPass:
    case ActiveFilterType::Gain:
    case ActiveFilterType::Delay:
    case ActiveFilterType::Polarity:
        return SectionSupport::Unsupported;
    }

    if (!validButterworthParameters(order, cutoffHz)) {
        return SectionSupport::Invalid;
    }

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double frequencyHz = frequencies[sampleIndex];
        if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
            return SectionSupport::Invalid;
        }
        response[sampleIndex] = butterworthTransfer(order, frequencyHz, cutoffHz, highPass);
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
