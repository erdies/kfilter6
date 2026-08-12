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

bool validBesselParameters(int order, double cutoffHz)
{
    return order >= 1 && order <= 8 && std::isfinite(cutoffHz) && cutoffHz > 0.0;
}

bool validGenericQParameters(double cutoffHz, double q)
{
    return std::isfinite(cutoffHz) && cutoffHz > 0.0 &&
           std::isfinite(q) && q > 0.0;
}

bool validLinkwitzRileyParameters(int order, double cutoffHz)
{
    return order >= 2 && order <= 8 && (order % 2) == 0 &&
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

bool validPeakingEqParameters(double centerFrequencyHz, double q, double gainDb)
{
    if (!std::isfinite(centerFrequencyHz) || centerFrequencyHz <= 0.0 ||
        !std::isfinite(q) || q <= 0.0 || !std::isfinite(gainDb)) {
        return false;
    }

    const double amplitude = std::pow(10.0, gainDb / 40.0);
    return std::isfinite(amplitude) && amplitude > 0.0 &&
           std::isfinite(1.0 / amplitude);
}

bool validShelfParameters(double transitionFrequencyHz, double q, double gainDb)
{
    if (!std::isfinite(transitionFrequencyHz) || transitionFrequencyHz <= 0.0 ||
        !std::isfinite(q) || q <= 0.0 || !std::isfinite(gainDb)) {
        return false;
    }

    const double amplitude = std::pow(10.0, gainDb / 40.0);
    return std::isfinite(amplitude) && amplitude > 0.0 &&
           std::isfinite(std::sqrt(amplitude)) &&
           std::isfinite(1.0 / amplitude) &&
           std::isfinite(1.0 / std::sqrt(amplitude));
}

std::complex<double> peakingEqTransfer(double frequencyHz,
                                       double centerFrequencyHz,
                                       double q,
                                       double gainDb)
{
    // Analog peaking-EQ prototype (the sample-rate-independent counterpart of
    // the standard peaking biquad). With A = 10^(gainDb/40):
    // H(s) = (s^2 + (A/Q)s + 1) / (s^2 + s/(A Q) + 1).
    // Therefore H(j) = A^2 = 10^(gainDb/20) exactly at f = f0, while
    // gainDb == 0 makes numerator and denominator identical at every frequency.
    const double amplitude = std::pow(10.0, gainDb / 40.0);
    const double ratio = frequencyHz / centerFrequencyHz;
    const std::complex<double> normalizedS{0.0, ratio};
    const std::complex<double> sSquared = normalizedS * normalizedS;
    const std::complex<double> numerator =
        sSquared + (amplitude / q) * normalizedS + 1.0;
    const std::complex<double> denominator =
        sSquared + normalizedS / (amplitude * q) + 1.0;
    return numerator / denominator;
}

std::complex<double> shelfTransfer(double frequencyHz,
                                   double transitionFrequencyHz,
                                   double q,
                                   double gainDb,
                                   bool highShelf)
{
    // Symmetric normalized second-order analog shelving prototype.
    // A = 10^(gainDb/40), s = j*f/f0.  For Low Shelf:
    //   H_LS(s) = (s^2 + sqrt(A)/Q*s + A) /
    //             (s^2 + 1/(sqrt(A)Q)*s + 1/A)
    // High Shelf is the frequency-inverted counterpart H_LS(1/s).
    // Consequently the affected plateau is exactly 10^(gainDb/20), the
    // opposite plateau is unity, and |H(j*f0)| = A (half the gain in dB).
    // gainDb == 0 makes numerator and denominator identical for every f.
    const double amplitude = std::pow(10.0, gainDb / 40.0);
    const double sqrtAmplitude = std::sqrt(amplitude);
    const double ratio = frequencyHz / transitionFrequencyHz;
    const std::complex<double> normalizedS{0.0, ratio};
    const std::complex<double> sSquared = normalizedS * normalizedS;

    if (highShelf) {
        const std::complex<double> numerator =
            amplitude * sSquared + (sqrtAmplitude / q) * normalizedS + 1.0;
        const std::complex<double> denominator =
            sSquared / amplitude + normalizedS / (sqrtAmplitude * q) + 1.0;
        return numerator / denominator;
    }

    const std::complex<double> numerator =
        sSquared + (sqrtAmplitude / q) * normalizedS + amplitude;
    const std::complex<double> denominator =
        sSquared + normalizedS / (sqrtAmplitude * q) + 1.0 / amplitude;
    return numerator / denominator;
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

struct BesselPoleSet
{
    const std::complex<double>* poles = nullptr;
    int count = 0;
};

BesselPoleSet besselPoleSet(int order)
{
    // Analog Bessel poles for orders 1...8, magnitude-normalized so that
    // |H(j)| = 1/sqrt(2).  This makes the user-facing cutoff frequency the
    // conventional -3.0103 dB point for every supported order.
    static const std::complex<double> order1[] = {
        {-0.99999999999999933, 0.0}
    };
    static const std::complex<double> order2[] = {
        {-1.1016013305921608, 0.63600982475703405},
        {-1.1016013305921608, -0.63600982475703405}
    };
    static const std::complex<double> order3[] = {
        {-1.0474091610089349, 0.99926443628063688},
        {-1.3226757999104441, 0.0},
        {-1.0474091610089349, -0.99926443628063688}
    };
    static const std::complex<double> order4[] = {
        {-0.99520876435027195, 1.2571057394546641},
        {-1.3700678305514422, 0.41024971749375155},
        {-1.3700678305514422, -0.41024971749375155},
        {-0.99520876435027195, -1.2571057394546641}
    };
    static const std::complex<double> order5[] = {
        {-0.95767654856268147, 1.4711243207303943},
        {-1.3808773258604392, 0.71790958762676804},
        {-1.5023162714474785, 0.0},
        {-1.3808773258604392, -0.71790958762676804},
        {-0.95767654856268147, -1.4711243207303943}
    };
    static const std::complex<double> order6[] = {
        {-0.93065652294685908, 1.6618632689425918},
        {-1.3818580975965642, 0.97147189071157158},
        {-1.5714904036160318, 0.32089637422262396},
        {-1.5714904036160318, -0.32089637422262396},
        {-1.3818580975965642, -0.97147189071157158},
        {-0.93065652294685908, -1.6618632689425918}
    };
    static const std::complex<double> order7[] = {
        {-0.90986778062347051, 1.8364513530363944},
        {-1.3789032167954749, 1.1915667778006531},
        {-1.6120387662261257, 0.58924450693147201},
        {-1.6843681792731817, 0.0},
        {-1.6120387662261257, -0.58924450693147201},
        {-1.3789032167954749, -1.1915667778006531},
        {-0.90986778062347051, -1.8364513530363944}
    };
    static const std::complex<double> order8[] = {
        {-0.89286971884713751, 1.9983258436413065},
        {-1.3738412176373769, 1.3883565758775629},
        {-1.6369394181268888, 0.82279562513969984},
        {-1.757408400401653, 0.2728675751022327},
        {-1.757408400401653, -0.2728675751022327},
        {-1.6369394181268888, -0.82279562513969984},
        {-1.3738412176373769, -1.3883565758775629},
        {-0.89286971884713751, -1.9983258436413065}
    };

    switch (order) {
    case 1: return {order1, 1};
    case 2: return {order2, 2};
    case 3: return {order3, 3};
    case 4: return {order4, 4};
    case 5: return {order5, 5};
    case 6: return {order6, 6};
    case 7: return {order7, 7};
    case 8: return {order8, 8};
    default: return {};
    }
}

std::complex<double> besselTransfer(int order,
                                    double frequencyHz,
                                    double cutoffHz,
                                    bool highPass)
{
    const BesselPoleSet poleSet = besselPoleSet(order);
    const std::complex<double> s{0.0, frequencyHz / cutoffHz};

    // The low-pass prototype is H(s) = product((-p)/(s-p)), which has unity
    // DC gain.  The high-pass is obtained by the exact analog LP->HP
    // transformation s -> 1/s.
    const std::complex<double> prototypeS = highPass ? 1.0 / s : s;
    std::complex<double> response{1.0, 0.0};
    for (int poleIndex = 0; poleIndex < poleSet.count; ++poleIndex) {
        const std::complex<double> pole = poleSet.poles[poleIndex];
        response *= (-pole) / (prototypeS - pole);
    }
    return response;
}

std::complex<double> genericQTransfer(double frequencyHz,
                                      double cutoffHz,
                                      double q,
                                      bool highPass)
{
    // Canonical second-order Q-based crossover form with normalized
    // s = j*f/f0.  The user-facing Frequency value is therefore the
    // natural frequency f0; at f0 the magnitude is exactly Q.
    const std::complex<double> s{0.0, frequencyHz / cutoffHz};
    const std::complex<double> sSquared = s * s;
    const std::complex<double> denominator = sSquared + s / q + 1.0;
    return highPass ? sSquared / denominator
                    : 1.0 / denominator;
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

    if (section.type() == ActiveFilterType::PeakingEq) {
        const auto& parameters =
            std::get<ActiveFilterPeakingEqParameters>(section.parameters());
        if (!validPeakingEqParameters(parameters.centerFrequencyHz,
                                      parameters.q,
                                      parameters.gainDb)) {
            return SectionSupport::Invalid;
        }

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            const double frequencyHz = frequencies[sampleIndex];
            if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
                return SectionSupport::Invalid;
            }
            response[sampleIndex] = peakingEqTransfer(frequencyHz,
                                                      parameters.centerFrequencyHz,
                                                      parameters.q,
                                                      parameters.gainDb);
        }
        return SectionSupport::Supported;
    }

    if (section.type() == ActiveFilterType::LowShelf ||
        section.type() == ActiveFilterType::HighShelf) {
        const bool highShelf = section.type() == ActiveFilterType::HighShelf;
        double transitionFrequencyHz = 0.0;
        double q = 0.0;
        double gainDb = 0.0;
        if (highShelf) {
            const auto& parameters =
                std::get<ActiveFilterHighShelfParameters>(section.parameters());
            transitionFrequencyHz = parameters.transitionFrequencyHz;
            q = parameters.q;
            gainDb = parameters.gainDb;
        } else {
            const auto& parameters =
                std::get<ActiveFilterLowShelfParameters>(section.parameters());
            transitionFrequencyHz = parameters.transitionFrequencyHz;
            q = parameters.q;
            gainDb = parameters.gainDb;
        }

        if (!validShelfParameters(transitionFrequencyHz, q, gainDb)) {
            return SectionSupport::Invalid;
        }

        for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            const double frequencyHz = frequencies[sampleIndex];
            if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
                return SectionSupport::Invalid;
            }
            response[sampleIndex] = shelfTransfer(frequencyHz,
                                                  transitionFrequencyHz,
                                                  q,
                                                  gainDb,
                                                  highShelf);
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
    double q = 0.707;
    bool highPass = false;
    ActiveFilterCharacteristic characteristic = ActiveFilterCharacteristic::Butterworth;

    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        const auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        characteristic = parameters.characteristic;
        order = parameters.order;
        cutoffHz = parameters.frequencyHz;
        q = parameters.q;
        break;
    }
    case ActiveFilterType::HighPass: {
        const auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        characteristic = parameters.characteristic;
        order = parameters.order;
        cutoffHz = parameters.frequencyHz;
        q = parameters.q;
        highPass = true;
        break;
    }
    case ActiveFilterType::BandPass: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Notch: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::PeakingEq: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::LowShelf: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::HighShelf: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::AllPass: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Gain: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Delay: // handled above; retained for exhaustive enum handling
    case ActiveFilterType::Polarity: // handled above; retained for exhaustive enum handling
        return SectionSupport::Unsupported;
    }

    if (characteristic != ActiveFilterCharacteristic::Butterworth &&
        characteristic != ActiveFilterCharacteristic::Bessel &&
        characteristic != ActiveFilterCharacteristic::LinkwitzRiley &&
        characteristic != ActiveFilterCharacteristic::GenericQ) {
        return SectionSupport::Unsupported;
    }

    const bool bessel = characteristic == ActiveFilterCharacteristic::Bessel;
    const bool genericQ = characteristic == ActiveFilterCharacteristic::GenericQ;
    const bool linkwitzRiley = characteristic == ActiveFilterCharacteristic::LinkwitzRiley;

    // Patch 206 deliberately defines Generic/Q-based only as one canonical
    // second-order section.  Higher-order section decomposition is not
    // implied by a single Q value, so those orders remain unsupported.
    if (genericQ && order != 2) {
        return SectionSupport::Unsupported;
    }

    const bool invalidParameters =
        bessel ? !validBesselParameters(order, cutoffHz)
               : (genericQ ? !validGenericQParameters(cutoffHz, q)
                           : (linkwitzRiley ? !validLinkwitzRileyParameters(order, cutoffHz)
                                            : !validButterworthParameters(order, cutoffHz)));
    if (invalidParameters) {
        return SectionSupport::Invalid;
    }

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double frequencyHz = frequencies[sampleIndex];
        if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
            return SectionSupport::Invalid;
        }

        if (bessel) {
            response[sampleIndex] = besselTransfer(order, frequencyHz, cutoffHz, highPass);
        } else if (genericQ) {
            response[sampleIndex] = genericQTransfer(frequencyHz, cutoffHz, q, highPass);
        } else if (linkwitzRiley) {
            response[sampleIndex] = linkwitzRileyTransfer(order, frequencyHz, cutoffHz, highPass);
        } else {
            response[sampleIndex] = butterworthTransfer(order, frequencyHz, cutoffHz, highPass);
        }
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
