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
#include <limits>

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


ActiveFilterChain singleBessel(ActiveFilterType type, int order, double cutoffHz)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(type);
    ActiveFilterSection& section = chain.section(index);
    if (type == ActiveFilterType::LowPass) {
        auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::Bessel;
        parameters.order = order;
        parameters.frequencyHz = cutoffHz;
    } else {
        auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::Bessel;
        parameters.order = order;
        parameters.frequencyHz = cutoffHz;
    }
    return chain;
}




ActiveFilterChain singleGenericQ(ActiveFilterType type, int order, double cutoffHz, double q)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(type);
    ActiveFilterSection& section = chain.section(index);
    if (type == ActiveFilterType::LowPass) {
        auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::GenericQ;
        parameters.order = order;
        parameters.frequencyHz = cutoffHz;
        parameters.q = q;
    } else {
        auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::GenericQ;
        parameters.order = order;
        parameters.frequencyHz = cutoffHz;
        parameters.q = q;
    }
    return chain;
}

ActiveFilterChain singleLinkwitzRiley(ActiveFilterType type, int order, double cutoffHz)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(type);
    ActiveFilterSection& section = chain.section(index);
    if (type == ActiveFilterType::LowPass) {
        auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
        parameters.order = order;
        parameters.frequencyHz = cutoffHz;
    } else {
        auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        parameters.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
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

ActiveFilterChain singlePeakingEq(double centerFrequencyHz, double q, double gainDb)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(ActiveFilterType::PeakingEq);
    auto& parameters =
        std::get<ActiveFilterPeakingEqParameters>(chain.section(index).parameters());
    parameters.centerFrequencyHz = centerFrequencyHz;
    parameters.q = q;
    parameters.gainDb = gainDb;
    return chain;
}

ActiveFilterChain singleShelf(ActiveFilterType type,
                              double transitionFrequencyHz,
                              double q,
                              double gainDb)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(type);
    if (type == ActiveFilterType::LowShelf) {
        auto& parameters =
            std::get<ActiveFilterLowShelfParameters>(chain.section(index).parameters());
        parameters.transitionFrequencyHz = transitionFrequencyHz;
        parameters.q = q;
        parameters.gainDb = gainDb;
    } else {
        auto& parameters =
            std::get<ActiveFilterHighShelfParameters>(chain.section(index).parameters());
        parameters.transitionFrequencyHz = transitionFrequencyHz;
        parameters.q = q;
        parameters.gainDb = gainDb;
    }
    return chain;
}

ActiveFilterChain singleAllPass(int order, double frequencyHz, double q = 0.707)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(ActiveFilterType::AllPass);
    auto& parameters =
        std::get<ActiveFilterAllPassParameters>(chain.section(index).parameters());
    parameters.order = order;
    parameters.frequencyHz = frequencyHz;
    parameters.q = q;
    return chain;
}

ActiveFilterChain singleGain(double gainDb)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(ActiveFilterType::Gain);
    std::get<ActiveFilterGainParameters>(chain.section(index).parameters()).gainDb = gainDb;
    return chain;
}

ActiveFilterChain singleDelay(double delayMs)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(ActiveFilterType::Delay);
    std::get<ActiveFilterDelayParameters>(chain.section(index).parameters()).delayMs = delayMs;
    return chain;
}

ActiveFilterChain singlePolarity(bool inverted)
{
    ActiveFilterChain chain;
    chain.setEnabled(true);
    const std::size_t index = chain.addSection(ActiveFilterType::Polarity);
    std::get<ActiveFilterPolarityParameters>(chain.section(index).parameters()).inverted = inverted;
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

    // Patch 205: Bessel LP/HP, orders 1...8.  The analog prototypes are
    // magnitude-normalized: the user-facing frequency is always the
    // -3.0103 dB point.  Reference complex values below are independent
    // prototype values at f/fc = 0.5, 1.0 and 2.0.
    const double besselRatios[] = {0.5, 1.0, 2.0};
    const std::complex<double> besselLowPassReference[8][3] = {
        {{0.79999999999999993, -0.40000000000000024},
         {0.49999999999999978, -0.50000000000000011},
         {0.19999999999999984, -0.39999999999999997}},
        {{0.71750243364634159, -0.57776461850205307},
         {0.19098300562505199, -0.68082706435806539},
         {-0.15361003830943884, -0.28416362247785698}},
        {{0.59024609374801396, -0.7105353367752324},
         {-0.11647709323013922, -0.69744755125576097},
         {-0.24806467701558424, -0.039440248651622371}},
        {{0.45320947634732578, -0.80295510553057603},
         {-0.36247782263765932, -0.60713246338493654},
         {-0.16515424284390687, 0.13555865534109571}},
        {{0.32176063281885842, -0.86242984736910822},
         {-0.5338537228056468, -0.4636811432941299},
         {-0.025804264356561375, 0.19640346808136644}},
        {{0.19987011364364532, -0.89759322747591364},
         {-0.64024925982998804, -0.30013477853649906},
         {0.098462106782924413, 0.16902438819218807}},
        {{0.087117896902350958, -0.91489739547880355},
         {-0.69439615213501771, -0.13346903723366985},
         {0.17829607393671107, 0.090670333765104874}},
        {{-0.017465337474808492, -0.91852329576582326},
         {-0.70659572301692664, 0.026879066467983447},
         {0.20700815674254269, -0.0066881013948341126}}
    };

    const double probeHz = frequencies[CutoffIndex];
    for (int order = 1; order <= 8; ++order) {
        for (int ratioIndex = 0; ratioIndex < 3; ++ratioIndex) {
            const double ratio = besselRatios[ratioIndex];
            const double localCutoffHz = probeHz / ratio;

            const ActiveFilterResponse lowPass = calculateActiveFilterResponse(
                singleBessel(ActiveFilterType::LowPass, order, localCutoffHz));
            if (!require(lowPass.status == ActiveFilterResponseStatus::Valid,
                         "Bessel low-pass orders 1...8 must be supported") ||
                !require(nearComplex(lowPass.values[CutoffIndex],
                                     besselLowPassReference[order - 1][ratioIndex],
                                     2.0e-9),
                         "Bessel low-pass complex response does not match reference")) {
                return 1;
            }

            const ActiveFilterResponse highPass = calculateActiveFilterResponse(
                singleBessel(ActiveFilterType::HighPass, order, localCutoffHz));
            const std::complex<double> expectedHighPass =
                std::conj(besselLowPassReference[order - 1][2 - ratioIndex]);
            if (!require(highPass.status == ActiveFilterResponseStatus::Valid,
                         "Bessel high-pass orders 1...8 must be supported") ||
                !require(nearComplex(highPass.values[CutoffIndex], expectedHighPass, 2.0e-9),
                         "Bessel high-pass complex response does not match LP->HP reference")) {
                return 1;
            }
        }

        const ActiveFilterResponse lowPassAtCutoff = calculateActiveFilterResponse(
            singleBessel(ActiveFilterType::LowPass, order, cutoffHz));
        const ActiveFilterResponse highPassAtCutoff = calculateActiveFilterResponse(
            singleBessel(ActiveFilterType::HighPass, order, cutoffHz));
        if (!require(near(std::abs(lowPassAtCutoff.values[CutoffIndex]), invSqrt2, 2.0e-9),
                     "Bessel low-pass cutoff magnitude must be -3.0103 dB") ||
            !require(near(std::abs(highPassAtCutoff.values[CutoffIndex]), invSqrt2, 2.0e-9),
                     "Bessel high-pass cutoff magnitude must be -3.0103 dB")) {
            return 1;
        }
    }

    for (int invalidOrder : {0, 9}) {
        response = calculateActiveFilterResponse(
            singleBessel(ActiveFilterType::LowPass, invalidOrder, cutoffHz));
        if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                     "Bessel order outside 1...8 must be invalid")) {
            return 1;
        }
    }


    // Patch 206: Generic / Q-based LP/HP is one canonical second-order
    // section.  Frequency is the natural frequency f0 and Q directly controls
    // the damping; therefore |H(f0)| = Q.
    for (double genericQ : {0.5, invSqrt2, 1.0, 2.0}) {
        for (ActiveFilterType type : {ActiveFilterType::LowPass, ActiveFilterType::HighPass}) {
            response = calculateActiveFilterResponse(
                singleGenericQ(type, 2, cutoffHz, genericQ));
            if (!require(response.status == ActiveFilterResponseStatus::Valid,
                         "Generic/Q-based LP2/HP2 must be supported") ||
                !require(nearComplex(response.values[CutoffIndex],
                                     type == ActiveFilterType::LowPass
                                         ? std::complex<double>{0.0, -genericQ}
                                         : std::complex<double>{0.0, genericQ},
                                     3.0e-10),
                         "Generic/Q-based response at f0 must have magnitude Q and +/-90 degree phase")) {
                return 1;
            }

            for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
                const std::complex<double> normalizedS{0.0, frequencies[index] / cutoffHz};
                const std::complex<double> squared = normalizedS * normalizedS;
                const std::complex<double> denominator = squared + normalizedS / genericQ + 1.0;
                const std::complex<double> expected =
                    type == ActiveFilterType::LowPass ? 1.0 / denominator
                                                      : squared / denominator;
                if (!require(nearComplex(response.values[index], expected, 4.0e-10),
                             "Generic/Q-based complex response does not match reference form")) {
                    return 1;
                }
            }
        }
    }

    // Q = 1/sqrt(2) is exactly the canonical second-order Butterworth damping.
    for (ActiveFilterType type : {ActiveFilterType::LowPass, ActiveFilterType::HighPass}) {
        const ActiveFilterResponse generic = calculateActiveFilterResponse(
            singleGenericQ(type, 2, cutoffHz, invSqrt2));
        const ActiveFilterResponse butterworth = calculateActiveFilterResponse(
            singleButterworth(type, 2, cutoffHz));
        for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
            if (!require(nearComplex(generic.values[index], butterworth.values[index], 5.0e-10),
                         "Generic/Q-based Q=1/sqrt(2) must equal Butterworth order 2")) {
                return 1;
            }
        }
    }

    for (int unsupportedOrder : {0, 1, 3, 8}) {
        response = calculateActiveFilterResponse(
            singleGenericQ(ActiveFilterType::LowPass, unsupportedOrder, cutoffHz, invSqrt2));
        if (!require(response.status == ActiveFilterResponseStatus::Unsupported,
                     "Generic/Q-based orders other than 2 must remain unsupported")) {
            return 1;
        }
    }

    response = calculateActiveFilterResponse(
        singleGenericQ(ActiveFilterType::LowPass, 2, 0.0, invSqrt2));
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "Generic/Q-based frequency must be positive")) {
        return 1;
    }
    for (double invalidQ : {0.0, -1.0, std::numeric_limits<double>::infinity()}) {
        response = calculateActiveFilterResponse(
            singleGenericQ(ActiveFilterType::HighPass, 2, cutoffHz, invalidQ));
        if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                     "Generic/Q-based Q must be finite and positive")) {
            return 1;
        }
    }
    response = calculateActiveFilterResponse(
        singleGenericQ(ActiveFilterType::HighPass, 2, cutoffHz,
                       std::numeric_limits<double>::quiet_NaN()));
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "Generic/Q-based NaN Q must be invalid")) {
        return 1;
    }

    // Patch 210 extends the existing Linkwitz-Riley LP/HP implementation
    // to LR6 and LR8. Every supported final order is two cascaded Butterworth
    // filters of half the final order, so the complete complex response must
    // equal H_BW(order/2)^2.
    for (int order : {2, 4, 6, 8}) {
        for (ActiveFilterType type : {ActiveFilterType::LowPass, ActiveFilterType::HighPass}) {
            const ActiveFilterResponse linkwitzRiley =
                calculateActiveFilterResponse(singleLinkwitzRiley(type, order, cutoffHz));
            const ActiveFilterResponse butterworthHalfOrder =
                calculateActiveFilterResponse(singleButterworth(type, order / 2, cutoffHz));

            if (!require(linkwitzRiley.status == ActiveFilterResponseStatus::Valid,
                         "LR2/LR4/LR6/LR8 low-pass/high-pass must be supported") ||
                !require(near(std::abs(linkwitzRiley.values[CutoffIndex]), 0.5, 2.0e-10),
                         "Linkwitz-Riley cutoff magnitude must be -6.0206 dB")) {
                return 1;
            }

            for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
                const std::complex<double> expected =
                    butterworthHalfOrder.values[index] * butterworthHalfOrder.values[index];
                if (!require(nearComplex(linkwitzRiley.values[index], expected, 5.0e-10),
                             "Linkwitz-Riley complex response must equal squared Butterworth response")) {
                    return 1;
                }
            }
        }
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::LowPass, 2, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.0, -0.5}, 2.0e-10),
                 "LR2 low-pass cutoff phase must be -90 degrees")) {
        return 1;
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::HighPass, 2, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.0, 0.5}, 2.0e-10),
                 "LR2 high-pass cutoff phase must be +90 degrees")) {
        return 1;
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::LowPass, 4, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {-0.5, 0.0}, 3.0e-10),
                 "LR4 low-pass cutoff phase must be 180 degrees")) {
        return 1;
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::HighPass, 4, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {-0.5, 0.0}, 3.0e-10),
                 "LR4 high-pass cutoff phase must be 180 degrees")) {
        return 1;
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::LowPass, 6, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.0, 0.5}, 4.0e-10),
                 "LR6 low-pass cutoff phase must be +90 degrees modulo 360")) {
        return 1;
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::HighPass, 6, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.0, -0.5}, 4.0e-10),
                 "LR6 high-pass cutoff phase must be -90 degrees modulo 360")) {
        return 1;
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::LowPass, 8, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.5, 0.0}, 5.0e-10),
                 "LR8 low-pass cutoff phase must be 0 degrees modulo 360")) {
        return 1;
    }

    response = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::HighPass, 8, cutoffHz));
    if (!require(nearComplex(response.values[CutoffIndex], {0.5, 0.0}, 5.0e-10),
                 "LR8 high-pass cutoff phase must be 0 degrees modulo 360")) {
        return 1;
    }

    // Ideal matched LR branches sum with flat magnitude. LR2/LR6 need a
    // relative polarity inversion; LR4/LR8 are already in phase.
    for (int order : {2, 4, 6, 8}) {
        const ActiveFilterResponse lowPass = calculateActiveFilterResponse(
            singleLinkwitzRiley(ActiveFilterType::LowPass, order, cutoffHz));
        const ActiveFilterResponse highPass = calculateActiveFilterResponse(
            singleLinkwitzRiley(ActiveFilterType::HighPass, order, cutoffHz));
        const double highPassSign = ((order / 2) % 2) == 0 ? 1.0 : -1.0;
        for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
            if (!require(near(std::abs(lowPass.values[index] +
                                      highPassSign * highPass.values[index]),
                              1.0, 1.0e-9),
                         "matched Linkwitz-Riley branches must sum with flat magnitude using the correct relative polarity")) {
                return 1;
            }
        }
    }

    // Retain an explicit LR4 reference for the later elementary-section
    // cascade regression test.
    const ActiveFilterResponse lr4LowPass = calculateActiveFilterResponse(
        singleLinkwitzRiley(ActiveFilterType::LowPass, 4, cutoffHz));

    for (int invalidOrder : {1, 3, 5, 7, 9}) {
        response = calculateActiveFilterResponse(
            singleLinkwitzRiley(ActiveFilterType::LowPass, invalidOrder, cutoffHz));
        if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                     "Linkwitz-Riley supports only even orders LR2 through LR8")) {
            return 1;
        }
    }

    // Patch 188: AP1/AP2 All-pass. Both variants must have exact unity
    // magnitude over the whole grid and must match the agreed complex forms.
    const ActiveFilterResponse ap1 =
        calculateActiveFilterResponse(singleAllPass(1, cutoffHz));
    if (!require(ap1.status == ActiveFilterResponseStatus::Valid,
                 "AP1 must be supported") ||
        !require(ap1.hasActiveSections,
                 "enabled AP1 must report an active section") ||
        !require(nearComplex(ap1.values[CutoffIndex], {0.0, 1.0}, 2.0e-10),
                 "AP1 must be +90 degrees at f0 for the Patch-188 convention")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const double ratio = frequencies[index] / cutoffHz;
        const std::complex<double> normalizedS{0.0, ratio};
        const std::complex<double> expected =
            (normalizedS - 1.0) / (normalizedS + 1.0);
        if (!require(near(std::abs(ap1.values[index]), 1.0, 3.0e-12),
                     "AP1 magnitude must remain unity") ||
            !require(nearComplex(ap1.values[index], expected, 3.0e-10),
                     "AP1 complex response does not match the reference form")) {
            return 1;
        }
    }

    constexpr double AllPassQ = 0.8;
    const ActiveFilterResponse ap2 =
        calculateActiveFilterResponse(singleAllPass(2, cutoffHz, AllPassQ));
    if (!require(ap2.status == ActiveFilterResponseStatus::Valid,
                 "AP2 must be supported") ||
        !require(nearComplex(ap2.values[CutoffIndex], {-1.0, 0.0}, 2.0e-10),
                 "AP2 must be 180 degrees at f0")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const double ratio = frequencies[index] / cutoffHz;
        const std::complex<double> normalizedS{0.0, ratio};
        const std::complex<double> squared = normalizedS * normalizedS;
        const std::complex<double> expected =
            (squared - normalizedS / AllPassQ + 1.0) /
            (squared + normalizedS / AllPassQ + 1.0);
        if (!require(near(std::abs(ap2.values[index]), 1.0, 3.0e-12),
                     "AP2 magnitude must remain unity") ||
            !require(nearComplex(ap2.values[index], expected, 4.0e-10),
                     "AP2 complex response does not match the reference form")) {
            return 1;
        }
    }

    // Q is deliberately irrelevant to AP1. This keeps persisted AP1 Q metadata
    // from affecting either transfer validity or cache equivalence.
    response = calculateActiveFilterResponse(
        singleAllPass(1, cutoffHz, std::numeric_limits<double>::quiet_NaN()));
    if (!require(response.status == ActiveFilterResponseStatus::Valid,
                 "AP1 must ignore its unused Q metadata")) {
        return 1;
    }

    for (int invalidOrder : {0, 3, 8}) {
        response = calculateActiveFilterResponse(singleAllPass(invalidOrder, cutoffHz));
        if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                     "All-pass must support only AP1 and AP2")) {
            return 1;
        }
    }
    response = calculateActiveFilterResponse(singleAllPass(2, 0.0, AllPassQ));
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "All-pass frequency must be positive")) {
        return 1;
    }
    response = calculateActiveFilterResponse(singleAllPass(2, cutoffHz, 0.0));
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "AP2 Q must be positive")) {
        return 1;
    }
    response = calculateActiveFilterResponse(
        singleAllPass(2, cutoffHz, std::numeric_limits<double>::infinity()));
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "AP2 Q must be finite")) {
        return 1;
    }

    // Regression for the Patch-187 observation: once All-pass is implemented,
    // an All-pass -> Gain chain must stay valid and the Gain must determine the
    // magnitude while the All-pass supplies only phase.
    ActiveFilterChain allPassGain;
    allPassGain.setEnabled(true);
    std::size_t allPassGainIndex = allPassGain.addSection(ActiveFilterType::AllPass);
    auto& allPassGainAp =
        std::get<ActiveFilterAllPassParameters>(allPassGain.section(allPassGainIndex).parameters());
    allPassGainAp.order = 2;
    allPassGainAp.frequencyHz = cutoffHz;
    allPassGainAp.q = AllPassQ;
    allPassGainIndex = allPassGain.addSection(ActiveFilterType::Gain);
    constexpr double AllPassGainDb = -6.020599913279624;
    std::get<ActiveFilterGainParameters>(
        allPassGain.section(allPassGainIndex).parameters()).gainDb = AllPassGainDb;
    const ActiveFilterResponse allPassGainResponse =
        calculateActiveFilterResponse(allPassGain);
    if (!require(allPassGainResponse.status == ActiveFilterResponseStatus::Valid,
                 "All-pass/Gain chain must be valid")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        if (!require(near(std::abs(allPassGainResponse.values[index]), 0.5, 4.0e-12),
                     "Gain must remain effective after All-pass") ||
            !require(nearComplex(allPassGainResponse.values[index],
                                 ap2.values[index] * 0.5,
                                 5.0e-10),
                     "All-pass/Gain chain must multiply phase and gain")) {
            return 1;
        }
    }

    // Patch 187: elementary active-processing sections. Gain is a constant
    // positive real multiplier, Delay is a unit-magnitude phase rotation, and
    // Polarity is exactly +1 or -1.
    constexpr double HalfGainDb = -6.020599913279624;
    const ActiveFilterResponse halfGain = calculateActiveFilterResponse(singleGain(HalfGainDb));
    if (!require(halfGain.status == ActiveFilterResponseStatus::Valid,
                 "finite Gain must be supported") ||
        !require(halfGain.hasActiveSections,
                 "enabled Gain must report an active section")) {
        return 1;
    }
    for (const std::complex<double>& value : halfGain.values) {
        if (!require(nearComplex(value, {0.5, 0.0}, 2.0e-12),
                     "Gain must be frequency-independent and phase-neutral")) {
            return 1;
        }
    }

    const ActiveFilterResponse unityGain = calculateActiveFilterResponse(singleGain(0.0));
    for (const std::complex<double>& value : unityGain.values) {
        if (!require(nearComplex(value, {1.0, 0.0}, 1.0e-15),
                     "0 dB Gain must be exact unity")) {
            return 1;
        }
    }

    const double quarterCycleDelayMs = 250.0 / cutoffHz;
    const ActiveFilterResponse delay = calculateActiveFilterResponse(singleDelay(quarterCycleDelayMs));
    if (!require(delay.status == ActiveFilterResponseStatus::Valid,
                 "non-negative finite Delay must be supported") ||
        !require(nearComplex(delay.values[CutoffIndex], {0.0, -1.0}, 3.0e-10),
                 "quarter-cycle Delay must produce -90 degrees at the reference frequency")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const double phase = -2.0 * Pi * frequencies[index] * (quarterCycleDelayMs / 1000.0);
        const std::complex<double> expected = std::polar(1.0, phase);
        if (!require(near(std::abs(delay.values[index]), 1.0, 2.0e-12),
                     "Delay magnitude must remain exactly unity") ||
            !require(nearComplex(delay.values[index], expected, 3.0e-10),
                     "Delay phase must follow -2*pi*f*tau")) {
            return 1;
        }
    }

    const ActiveFilterResponse invertedPolarity = calculateActiveFilterResponse(singlePolarity(true));
    const ActiveFilterResponse normalPolarity = calculateActiveFilterResponse(singlePolarity(false));
    if (!require(invertedPolarity.status == ActiveFilterResponseStatus::Valid &&
                     normalPolarity.status == ActiveFilterResponseStatus::Valid,
                 "both Polarity states must be supported")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        if (!require(nearComplex(invertedPolarity.values[index], {-1.0, 0.0}, 1.0e-15),
                     "inverted Polarity must be exactly -1") ||
            !require(nearComplex(normalPolarity.values[index], {1.0, 0.0}, 1.0e-15),
                     "normal Polarity must be exactly +1")) {
            return 1;
        }
    }

    ActiveFilterChain elementaryCascade;
    elementaryCascade.setEnabled(true);
    std::size_t sectionIndex = elementaryCascade.addSection(ActiveFilterType::LowPass);
    auto& cascadeLowPass =
        std::get<ActiveFilterLowPassParameters>(elementaryCascade.section(sectionIndex).parameters());
    cascadeLowPass.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
    cascadeLowPass.order = 4;
    cascadeLowPass.frequencyHz = cutoffHz;
    sectionIndex = elementaryCascade.addSection(ActiveFilterType::Gain);
    std::get<ActiveFilterGainParameters>(elementaryCascade.section(sectionIndex).parameters()).gainDb =
        HalfGainDb;
    sectionIndex = elementaryCascade.addSection(ActiveFilterType::Delay);
    std::get<ActiveFilterDelayParameters>(elementaryCascade.section(sectionIndex).parameters()).delayMs =
        quarterCycleDelayMs;
    sectionIndex = elementaryCascade.addSection(ActiveFilterType::Polarity);
    std::get<ActiveFilterPolarityParameters>(elementaryCascade.section(sectionIndex).parameters()).inverted = true;

    const ActiveFilterResponse elementaryCascadeResponse =
        calculateActiveFilterResponse(elementaryCascade);
    if (!require(elementaryCascadeResponse.status == ActiveFilterResponseStatus::Valid,
                 "LR4/Gain/Delay/Polarity cascade must be valid")) {
        return 1;
    }
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const std::complex<double> expected =
            lr4LowPass.values[index] * halfGain.values[index] *
            delay.values[index] * invertedPolarity.values[index];
        if (!require(nearComplex(elementaryCascadeResponse.values[index], expected, 8.0e-10),
                     "elementary sections must multiply with the existing complex filter response")) {
            return 1;
        }
    }

    ActiveFilterChain invalidGain = singleGain(std::numeric_limits<double>::infinity());
    response = calculateActiveFilterResponse(invalidGain);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "non-finite Gain must be invalid")) {
        return 1;
    }
    invalidGain = singleGain(1.0e9);
    response = calculateActiveFilterResponse(invalidGain);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "Gain that overflows the linear multiplier must be invalid")) {
        return 1;
    }

    ActiveFilterChain invalidDelay = singleDelay(-0.001);
    response = calculateActiveFilterResponse(invalidDelay);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "negative Delay must be invalid")) {
        return 1;
    }
    invalidDelay = singleDelay(std::numeric_limits<double>::infinity());
    response = calculateActiveFilterResponse(invalidDelay);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "non-finite Delay must be invalid")) {
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

    // Patch 208: analog Parametric / Peaking EQ.  The center magnitude must
    // equal the requested gain exactly, 0 dB must be fully neutral, and
    // equal positive/negative gains must be complex reciprocals.
    constexpr double peakingQ = 2.0;
    constexpr double peakingGainDb = 6.0;
    const double peakingLinearGain = std::pow(10.0, peakingGainDb / 20.0);
    const ActiveFilterResponse peakingBoost =
        calculateActiveFilterResponse(singlePeakingEq(cutoffHz, peakingQ, peakingGainDb));
    const ActiveFilterResponse peakingCut =
        calculateActiveFilterResponse(singlePeakingEq(cutoffHz, peakingQ, -peakingGainDb));
    const ActiveFilterResponse peakingNeutral =
        calculateActiveFilterResponse(singlePeakingEq(cutoffHz, peakingQ, 0.0));

    if (!require(peakingBoost.status == ActiveFilterResponseStatus::Valid &&
                     peakingCut.status == ActiveFilterResponseStatus::Valid &&
                     peakingNeutral.status == ActiveFilterResponseStatus::Valid,
                 "Peaking EQ boost/cut/neutral must be supported") ||
        !require(nearComplex(peakingBoost.values[CutoffIndex],
                             {peakingLinearGain, 0.0}, 3.0e-10),
                 "Peaking EQ center magnitude must equal requested boost") ||
        !require(nearComplex(peakingCut.values[CutoffIndex],
                             {1.0 / peakingLinearGain, 0.0}, 3.0e-10),
                 "Peaking EQ center magnitude must equal requested cut")) {
        return 1;
    }

    const double peakingAmplitude = std::pow(10.0, peakingGainDb / 40.0);
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const double ratio = frequencies[index] / cutoffHz;
        const std::complex<double> normalizedS{0.0, ratio};
        const std::complex<double> sSquared = normalizedS * normalizedS;
        const std::complex<double> expected =
            (sSquared + (peakingAmplitude / peakingQ) * normalizedS + 1.0) /
            (sSquared + normalizedS / (peakingAmplitude * peakingQ) + 1.0);
        if (!require(nearComplex(peakingBoost.values[index], expected, 4.0e-10),
                     "Peaking EQ complex response does not match analog reference") ||
            !require(nearComplex(peakingNeutral.values[index], {1.0, 0.0}, 2.0e-12),
                     "0 dB Peaking EQ must be exactly neutral") ||
            !require(nearComplex(peakingBoost.values[index] * peakingCut.values[index],
                                 {1.0, 0.0}, 5.0e-10),
                     "Peaking EQ equal boost/cut must be reciprocal including phase")) {
            return 1;
        }
    }

    if (!require(peakingBoost.values[CutoffIndex - 1].imag() > 0.0,
                 "Peaking EQ boost phase must be positive below center") ||
        !require(peakingBoost.values[CutoffIndex + 1].imag() < 0.0,
                 "Peaking EQ boost phase must be negative above center")) {
        return 1;
    }

    const ActiveFilterResponse broadPeaking =
        calculateActiveFilterResponse(singlePeakingEq(cutoffHz, 0.7, peakingGainDb));
    const ActiveFilterResponse narrowPeaking =
        calculateActiveFilterResponse(singlePeakingEq(cutoffHz, 8.0, peakingGainDb));
    if (!require(std::abs(narrowPeaking.values[CutoffIndex - 1]) <
                     std::abs(broadPeaking.values[CutoffIndex - 1]),
                 "higher Peaking-EQ Q must produce a narrower boost")) {
        return 1;
    }

    ActiveFilterChain invalidPeakingEq = singlePeakingEq(0.0, peakingQ, peakingGainDb);
    response = calculateActiveFilterResponse(invalidPeakingEq);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "zero Peaking-EQ center frequency must be invalid")) {
        return 1;
    }
    invalidPeakingEq = singlePeakingEq(cutoffHz, 0.0, peakingGainDb);
    response = calculateActiveFilterResponse(invalidPeakingEq);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "zero Peaking-EQ Q must be invalid")) {
        return 1;
    }
    invalidPeakingEq = singlePeakingEq(cutoffHz, peakingQ,
                                       std::numeric_limits<double>::infinity());
    response = calculateActiveFilterResponse(invalidPeakingEq);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "non-finite Peaking-EQ gain must be invalid")) {
        return 1;
    }

    // Patch 209: normalized second-order analog Low/High Shelf. Frequency is
    // the midpoint of the transition in dB: a +/-6 dB shelf is +/-3 dB at f0.
    // Equal positive/negative gains are complex reciprocals and Low/High Shelf
    // are frequency-inverted counterparts of the same prototype.
    constexpr double shelfQ = 0.707;
    constexpr double shelfGainDb = 6.0;
    const double shelfAmplitude = std::pow(10.0, shelfGainDb / 40.0);
    const double shelfLinearGain = std::pow(10.0, shelfGainDb / 20.0);

    for (ActiveFilterType shelfType : {ActiveFilterType::LowShelf,
                                       ActiveFilterType::HighShelf}) {
        const ActiveFilterResponse shelfBoost = calculateActiveFilterResponse(
            singleShelf(shelfType, cutoffHz, shelfQ, shelfGainDb));
        const ActiveFilterResponse shelfCut = calculateActiveFilterResponse(
            singleShelf(shelfType, cutoffHz, shelfQ, -shelfGainDb));
        const ActiveFilterResponse shelfNeutral = calculateActiveFilterResponse(
            singleShelf(shelfType, cutoffHz, shelfQ, 0.0));

        if (!require(shelfBoost.status == ActiveFilterResponseStatus::Valid &&
                         shelfCut.status == ActiveFilterResponseStatus::Valid &&
                         shelfNeutral.status == ActiveFilterResponseStatus::Valid,
                     "Low/High Shelf boost/cut/neutral must be supported") ||
            !require(near(std::abs(shelfBoost.values[CutoffIndex]), shelfAmplitude, 4.0e-10),
                     "Shelf transition magnitude must be half the requested gain in dB")) {
            return 1;
        }

        for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
            const double ratio = frequencies[index] / cutoffHz;
            const std::complex<double> normalizedS{0.0, ratio};
            const std::complex<double> sSquared = normalizedS * normalizedS;
            const double sqrtAmplitude = std::sqrt(shelfAmplitude);
            std::complex<double> expected;
            if (shelfType == ActiveFilterType::HighShelf) {
                expected =
                    (shelfAmplitude * sSquared +
                     (sqrtAmplitude / shelfQ) * normalizedS + 1.0) /
                    (sSquared / shelfAmplitude +
                     normalizedS / (sqrtAmplitude * shelfQ) + 1.0);
            } else {
                expected =
                    (sSquared + (sqrtAmplitude / shelfQ) * normalizedS + shelfAmplitude) /
                    (sSquared + normalizedS / (sqrtAmplitude * shelfQ) +
                     1.0 / shelfAmplitude);
            }

            if (!require(nearComplex(shelfBoost.values[index], expected, 5.0e-10),
                         "Shelf complex response does not match analog reference") ||
                !require(nearComplex(shelfNeutral.values[index], {1.0, 0.0}, 2.0e-12),
                         "0 dB Shelf must be exactly neutral") ||
                !require(nearComplex(shelfBoost.values[index] * shelfCut.values[index],
                                     {1.0, 0.0}, 6.0e-10),
                         "Shelf equal boost/cut must be reciprocal including phase")) {
                return 1;
            }
        }

        if (shelfType == ActiveFilterType::LowShelf) {
            if (!require(std::abs(shelfBoost.values.front()) > shelfAmplitude,
                         "Low Shelf boost must rise toward its low-frequency plateau") ||
                !require(std::abs(shelfBoost.values.front()) < shelfLinearGain * 1.001,
                         "Low Shelf boost must not exceed its low-frequency asymptote at low Q") ||
                !require(shelfBoost.values[CutoffIndex].imag() < 0.0,
                         "Low Shelf boost phase must be negative at transition")) {
                return 1;
            }
        } else {
            if (!require(std::abs(shelfBoost.values.back()) > shelfAmplitude,
                         "High Shelf boost must rise toward its high-frequency plateau") ||
                !require(std::abs(shelfBoost.values.back()) < shelfLinearGain * 1.001,
                         "High Shelf boost must not exceed its high-frequency asymptote at low Q") ||
                !require(shelfBoost.values[CutoffIndex].imag() > 0.0,
                         "High Shelf boost phase must be positive at transition")) {
                return 1;
            }
        }
    }

    ActiveFilterChain invalidShelf =
        singleShelf(ActiveFilterType::LowShelf, 0.0, shelfQ, shelfGainDb);
    response = calculateActiveFilterResponse(invalidShelf);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "zero Shelf transition frequency must be invalid")) {
        return 1;
    }
    invalidShelf = singleShelf(ActiveFilterType::HighShelf, cutoffHz, 0.0, shelfGainDb);
    response = calculateActiveFilterResponse(invalidShelf);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "zero Shelf Q must be invalid")) {
        return 1;
    }
    invalidShelf = singleShelf(ActiveFilterType::LowShelf, cutoffHz, shelfQ,
                               std::numeric_limits<double>::infinity());
    response = calculateActiveFilterResponse(invalidShelf);
    if (!require(response.status == ActiveFilterResponseStatus::InvalidParameters,
                 "non-finite Shelf gain must be invalid")) {
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

    ActiveFilterChain unsupported =
        singleGenericQ(ActiveFilterType::LowPass, 4, cutoffHz, invSqrt2);
    response = calculateActiveFilterResponse(unsupported);
    if (!require(response.status == ActiveFilterResponseStatus::Unsupported,
                 "Generic/Q-based order 4 must remain unsupported") ||
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

    ActiveFilterChain unsupportedBesselBandPass = singleBandPass(2, bandLowerHz, bandUpperHz);
    std::get<ActiveFilterBandPassParameters>(
        unsupportedBesselBandPass.section(0).parameters()).characteristic =
        ActiveFilterCharacteristic::Bessel;
    response = calculateActiveFilterResponse(unsupportedBesselBandPass);
    if (!require(response.status == ActiveFilterResponseStatus::Unsupported,
                 "Patch 205 intentionally leaves Bessel band-pass unsupported")) {
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
    const std::size_t unsupportedIndex = ignoredUnsupported.addSection(ActiveFilterType::LowPass);
    auto& disabledUnsupportedParameters =
        std::get<ActiveFilterLowPassParameters>(ignoredUnsupported.section(unsupportedIndex).parameters());
    disabledUnsupportedParameters.characteristic = ActiveFilterCharacteristic::GenericQ;
    disabledUnsupportedParameters.order = 4;
    ignoredUnsupported.section(unsupportedIndex).setEnabled(false);
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

    ActiveFilterResponseCache genericQCache;
    ActiveFilterChain cachedGenericQ =
        singleGenericQ(ActiveFilterType::LowPass, 2, cutoffHz, 0.7);
    genericQCache.responseFor(cachedGenericQ);
    const std::uint64_t genericQGeneration = genericQCache.generation();
    std::get<ActiveFilterLowPassParameters>(cachedGenericQ.section(0).parameters()).q = 1.4;
    genericQCache.responseFor(cachedGenericQ);
    if (!require(genericQCache.generation() == genericQGeneration + 1,
                 "Generic/Q-based Q change must invalidate transfer cache")) {
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

    ActiveFilterResponseCache peakingEqCache;
    ActiveFilterChain cachedPeakingEq = singlePeakingEq(cutoffHz, 2.0, 4.0);
    peakingEqCache.responseFor(cachedPeakingEq);
    const std::uint64_t peakingEqGeneration = peakingEqCache.generation();
    auto& cachedPeakingEqParameters =
        std::get<ActiveFilterPeakingEqParameters>(cachedPeakingEq.section(0).parameters());
    cachedPeakingEqParameters.gainDb = -4.0;
    peakingEqCache.responseFor(cachedPeakingEq);
    if (!require(peakingEqCache.generation() == peakingEqGeneration + 1,
                 "Peaking-EQ gain change must invalidate transfer cache")) {
        return 1;
    }
    const std::uint64_t peakingEqGainGeneration = peakingEqCache.generation();
    cachedPeakingEqParameters.q = 3.0;
    peakingEqCache.responseFor(cachedPeakingEq);
    if (!require(peakingEqCache.generation() == peakingEqGainGeneration + 1,
                 "Peaking-EQ Q change must invalidate transfer cache")) {
        return 1;
    }
    const std::uint64_t peakingEqQGeneration = peakingEqCache.generation();
    cachedPeakingEqParameters.centerFrequencyHz *= 1.1;
    peakingEqCache.responseFor(cachedPeakingEq);
    if (!require(peakingEqCache.generation() == peakingEqQGeneration + 1,
                 "Peaking-EQ frequency change must invalidate transfer cache")) {
        return 1;
    }

    ActiveFilterResponseCache shelfCache;
    ActiveFilterChain cachedShelf =
        singleShelf(ActiveFilterType::LowShelf, cutoffHz, 0.7, 4.0);
    shelfCache.responseFor(cachedShelf);
    const std::uint64_t shelfGeneration = shelfCache.generation();
    auto& cachedShelfParameters =
        std::get<ActiveFilterLowShelfParameters>(cachedShelf.section(0).parameters());
    cachedShelfParameters.gainDb = -4.0;
    shelfCache.responseFor(cachedShelf);
    if (!require(shelfCache.generation() == shelfGeneration + 1,
                 "Shelf gain change must invalidate transfer cache")) {
        return 1;
    }
    const std::uint64_t shelfGainGeneration = shelfCache.generation();
    cachedShelfParameters.q = 1.1;
    shelfCache.responseFor(cachedShelf);
    if (!require(shelfCache.generation() == shelfGainGeneration + 1,
                 "Shelf Q change must invalidate transfer cache")) {
        return 1;
    }
    const std::uint64_t shelfQGeneration = shelfCache.generation();
    cachedShelfParameters.transitionFrequencyHz *= 1.1;
    shelfCache.responseFor(cachedShelf);
    if (!require(shelfCache.generation() == shelfQGeneration + 1,
                 "Shelf frequency change must invalidate transfer cache")) {
        return 1;
    }

    ActiveFilterResponseCache allPassCache;
    ActiveFilterChain cachedAllPass = singleAllPass(1, cutoffHz, 0.5);
    allPassCache.responseFor(cachedAllPass);
    const std::uint64_t allPassGeneration = allPassCache.generation();
    std::get<ActiveFilterAllPassParameters>(cachedAllPass.section(0).parameters()).q = 9.0;
    allPassCache.responseFor(cachedAllPass);
    if (!require(allPassCache.generation() == allPassGeneration,
                 "unused AP1 Q must not invalidate transfer cache")) {
        return 1;
    }
    auto& cachedAllPassParameters =
        std::get<ActiveFilterAllPassParameters>(cachedAllPass.section(0).parameters());
    cachedAllPassParameters.frequencyHz *= 1.1;
    allPassCache.responseFor(cachedAllPass);
    if (!require(allPassCache.generation() == allPassGeneration + 1,
                 "All-pass frequency change must invalidate transfer cache")) {
        return 1;
    }
    cachedAllPassParameters.order = 2;
    cachedAllPassParameters.q = 0.7;
    allPassCache.responseFor(cachedAllPass);
    const std::uint64_t ap2Generation = allPassCache.generation();
    cachedAllPassParameters.q = 1.4;
    allPassCache.responseFor(cachedAllPass);
    if (!require(allPassCache.generation() == ap2Generation + 1,
                 "AP2 Q change must invalidate transfer cache")) {
        return 1;
    }

    ActiveFilterResponseCache gainCache;
    ActiveFilterChain cachedGain = singleGain(-3.0);
    gainCache.responseFor(cachedGain);
    const std::uint64_t gainGeneration = gainCache.generation();
    std::get<ActiveFilterGainParameters>(cachedGain.section(0).parameters()).gainDb = -6.0;
    gainCache.responseFor(cachedGain);
    if (!require(gainCache.generation() == gainGeneration + 1,
                 "Gain change must invalidate transfer cache")) {
        return 1;
    }

    ActiveFilterResponseCache delayCache;
    ActiveFilterChain cachedDelay = singleDelay(0.25);
    delayCache.responseFor(cachedDelay);
    const std::uint64_t delayGeneration = delayCache.generation();
    std::get<ActiveFilterDelayParameters>(cachedDelay.section(0).parameters()).delayMs = 0.5;
    delayCache.responseFor(cachedDelay);
    if (!require(delayCache.generation() == delayGeneration + 1,
                 "Delay change must invalidate transfer cache")) {
        return 1;
    }

    ActiveFilterResponseCache polarityCache;
    ActiveFilterChain cachedPolarity = singlePolarity(false);
    polarityCache.responseFor(cachedPolarity);
    const std::uint64_t polarityGeneration = polarityCache.generation();
    std::get<ActiveFilterPolarityParameters>(cachedPolarity.section(0).parameters()).inverted = true;
    polarityCache.responseFor(cachedPolarity);
    if (!require(polarityCache.generation() == polarityGeneration + 1,
                 "Polarity change must invalidate transfer cache")) {
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
    const std::size_t mixedUnsupportedIndex =
        mixedUnsupported.addSection(ActiveFilterType::HighPass);
    auto& mixedUnsupportedParameters =
        std::get<ActiveFilterHighPassParameters>(
            mixedUnsupported.section(mixedUnsupportedIndex).parameters());
    mixedUnsupportedParameters.characteristic = ActiveFilterCharacteristic::GenericQ;
    mixedUnsupportedParameters.order = 4;
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
