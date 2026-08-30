/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

/*
 * Patch 300 regression test.
 *
 * Until Patch 300 the Vented branch of calculateAcousticResponse() contributed
 * magnitude only in simplified mode:
 *
 *     factor = bx / sqrt( (bx - a2*bu + a0)^2 + (a1*bw - a3*bu*bw)^2 )
 *     response *= factor;
 *
 * That is |H| of the fourth-order high-pass H(s) = s^4 / D(s) at s = j*bw, with
 * the denominator's real and imaginary parts already spelled out under the
 * square root. The phase was therefore never discarded, it was never formed.
 * Patch 300 builds the denominator as a complex number instead.
 *
 * This test pins both halves of that claim:
 *
 *   1. The magnitude is unchanged. The historical scalar formula is reproduced
 *      here and compared against std::abs() of the new complex result over the
 *      whole frequency grid, for several vented alignments.
 *   2. The phase is now present and behaves like a fourth-order high-pass:
 *      it approaches +360 degrees far below Fb, decreases monotonically with
 *      frequency, and approaches 0 degrees far above Fb. Before Patch 300 the
 *      argument was identically zero.
 */

#include "driver.h"
#include "kfilterfrequencygrid.h"

#include <QCoreApplication>
#include <QString>
#include <QTextStream>

#include <cmath>
#include <complex>
#include <vector>

namespace
{

int failures = 0;

void reportFailure(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

void expect(bool condition, const QString& message)
{
    if (!condition) {
        reportFailure(message);
    }
}

struct Alignment
{
    const char* name;
    double fs;
    double fb;
    double qts;
    double qes;
    double qms;
    double vas;
    double vb;
    double ql;
};

const Alignment alignments[] = {
    {"QB3 woofer",        38.0, 40.0, 0.38, 0.42, 3.8, 45.0,  30.0, 10.0},
    {"SBB4 woofer",       28.0, 28.0, 0.31, 0.34, 3.4, 90.0,  90.0,  7.0},
    {"C4 tuned low",      45.0, 32.0, 0.45, 0.50, 4.5, 20.0,  14.0, 15.0},
    {"small vented",      70.0, 85.0, 0.52, 0.58, 5.2,  6.0,   4.0, 12.0},
    {"lossy cabinet",     33.0, 36.0, 0.35, 0.39, 3.5, 60.0,  50.0,  3.0},
};

driver makeVentedDriver(const Alignment& alignment)
{
    driver subject;
    subject.setF0(alignment.fs);
    subject.setFb(alignment.fb);
    subject.setQts(alignment.qts);
    subject.setQes(alignment.qes);
    subject.setQms(alignment.qms);
    subject.setVas(alignment.vas);
    subject.setVb(alignment.vb);
    subject.setQl(alignment.ql);
    subject.setEnclosureTypeProposal(EnclosureType::Vented);
    subject.setFullCircuit(false);
    return subject;
}

struct DenominatorCoefficients
{
    double a0;
    double a1;
    double a2;
    double a3;
};

DenominatorCoefficients coefficients(const Alignment& alignment)
{
    DenominatorCoefficients c{};
    c.a0 = std::pow(alignment.fb / alignment.fs, 2.0);
    c.a1 = c.a0 / alignment.qts + alignment.fb / (alignment.ql * alignment.fs);
    c.a2 = 1.0 + c.a0 + alignment.fb / (alignment.ql * alignment.fs * alignment.qts) +
           alignment.vas / alignment.vb;
    c.a3 = 1.0 / alignment.qts + alignment.fb / (alignment.ql * alignment.fs);
    return c;
}

/**
 * The frequencies driver.cpp actually evaluates. It does not use
 * kfilterFrequencyGridHz() but starts at omega = 125.6637061 and multiplies,
 * then converts back with the truncated constant 0.159154943. Reproducing that
 * exactly keeps this comparison free of the small offset between the two grids.
 */
std::vector<double> driverInternalFrequencies()
{
    std::vector<double> frequencies;
    frequencies.reserve(KFilterFrequencyCount);
    double omega = 125.6637061;
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        frequencies.push_back(omega * 0.159154943);
        omega *= KFilterFrequencyStep;
    }
    return frequencies;
}

/** The intended complex transfer function, implemented independently. */
std::complex<double> referenceTransfer(const Alignment& alignment, double frequencyHz)
{
    const DenominatorCoefficients c = coefficients(alignment);
    const double bw = frequencyHz / alignment.fs;
    const double bu = bw * bw;
    const double bx = bu * bu;
    return bx / std::complex<double>{bx - c.a2 * bu + c.a0, c.a1 * bw - c.a3 * bu * bw};
}

/** The historical scalar formula, reproduced verbatim for comparison. */
double legacyMagnitude(const Alignment& alignment, double frequencyHz)
{
    const DenominatorCoefficients c = coefficients(alignment);
    const double a0 = c.a0;
    const double a1 = c.a1;
    const double a2 = c.a2;
    const double a3 = c.a3;

    const double bw = frequencyHz / alignment.fs;
    const double bu = std::pow(bw, 2.0);
    const double bx = std::pow(bu, 2.0);

    return bx / std::sqrt(std::pow(bx - a2 * bu + a0, 2.0) +
                          std::pow(a1 * bw - a3 * bu * bw, 2.0));
}

/**
 * Isolates the enclosure contribution. The driver response also carries the
 * network cascade and the gain, but with an empty network and unity gain the
 * ratio between the response and the reference driver response is exactly the
 * enclosure transfer function.
 */
std::vector<std::complex<double>> enclosureTransfer(const Alignment& alignment)
{
    driver subject = makeVentedDriver(alignment);
    subject.calculatePressureResponse();

    std::vector<std::complex<double>> transfer;
    transfer.reserve(KFilterFrequencyCount);
    for (const std::complex<double>& sample : subject.pressureResponse()) {
        transfer.push_back(sample);
    }
    return transfer;
}

void testMagnitudeIsUnchanged()
{
    const std::vector<double> frequencies = driverInternalFrequencies();

    for (const Alignment& alignment : alignments) {
        const std::vector<std::complex<double>> transfer = enclosureTransfer(alignment);
        const QString name = QLatin1String(alignment.name);

        if (transfer.size() != frequencies.size()) {
            reportFailure(QStringLiteral("%1: unexpected sample count").arg(name));
            continue;
        }

        double worstMagnitudeError = 0.0;
        double worstTransferError = 0.0;
        int worstIndex = -1;

        for (std::size_t index = 0; index < frequencies.size(); ++index) {
            const double actualMagnitude = std::abs(transfer[index]);
            if (!std::isfinite(actualMagnitude)) {
                reportFailure(QStringLiteral("%1: non-finite sample at %2 Hz")
                                  .arg(name).arg(frequencies[index]));
                break;
            }

            // 1. The magnitude must match the historical scalar formula.
            const double expectedMagnitude = legacyMagnitude(alignment, frequencies[index]);
            const double magnitudeError =
                std::abs(actualMagnitude - expectedMagnitude) /
                std::max(std::abs(expectedMagnitude), 1e-300);
            if (magnitudeError > worstMagnitudeError) {
                worstMagnitudeError = magnitudeError;
                worstIndex = static_cast<int>(index);
            }

            // 2. The complex value must match an independent implementation of
            //    the intended transfer function, magnitude and phase together.
            const std::complex<double> expected = referenceTransfer(alignment, frequencies[index]);
            const double transferError =
                std::abs(transfer[index] - expected) / std::max(std::abs(expected), 1e-300);
            worstTransferError = std::max(worstTransferError, transferError);
        }

        // Both paths evaluate the same expression with different groupings, so
        // only rounding differences are permitted.
        expect(worstMagnitudeError < 1e-12,
               QStringLiteral("%1: magnitude changed, worst relative error %2 at index %3")
                   .arg(name).arg(worstMagnitudeError, 0, 'e', 3).arg(worstIndex));

        expect(worstTransferError < 1e-12,
               QStringLiteral("%1: complex response deviates from the reference by %2")
                   .arg(name).arg(worstTransferError, 0, 'e', 3));
    }
}

double degrees(const std::complex<double>& value)
{
    return std::arg(value) * 180.0 / M_PI;
}

/**
 * Unwraps the phase into a continuous, monotonically decreasing sequence and
 * normalizes it so the highest frequency ends near zero degrees. A fourth-order
 * high-pass then starts near +360 degrees, or lower when the grid does not
 * reach far enough below the tuning frequency.
 */
std::vector<double> unwrappedPhaseDegrees(const std::vector<std::complex<double>>& transfer)
{
    std::vector<double> phase;
    phase.reserve(transfer.size());

    double offset = 0.0;
    double previous = degrees(transfer.front());
    phase.push_back(previous);

    for (std::size_t index = 1; index < transfer.size(); ++index) {
        const double current = degrees(transfer[index]);
        const double step = current - previous;
        if (step > 180.0) {
            offset -= 360.0;
        } else if (step < -180.0) {
            offset += 360.0;
        }
        phase.push_back(current + offset);
        previous = current;
    }

    // Shift the whole sequence so the last sample lies in (-180, 180].
    const double correction = -360.0 * std::round(phase.back() / 360.0);
    for (double& value : phase) {
        value += correction;
    }
    return phase;
}

void testPhaseIsPresentAndPlausible()
{
    const std::vector<double> frequencies = driverInternalFrequencies();

    for (const Alignment& alignment : alignments) {
        const std::vector<std::complex<double>> transfer = enclosureTransfer(alignment);
        const QString name = QLatin1String(alignment.name);

        // Before Patch 300 every sample was real and positive.
        bool anyPhase = false;
        for (const std::complex<double>& sample : transfer) {
            if (std::abs(std::arg(sample)) > 1e-6) {
                anyPhase = true;
                break;
            }
        }
        expect(anyPhase, QStringLiteral("%1: response still carries no phase").arg(name));

        const std::vector<double> phase = unwrappedPhaseDegrees(transfer);

        // Far above the tuning frequency the enclosure is transparent.
        expect(std::abs(phase.back()) < 5.0,
               QStringLiteral("%1: phase at %2 Hz is %3 deg, expected near 0")
                   .arg(name).arg(frequencies.back(), 0, 'f', 0).arg(phase.back(), 0, 'f', 2));

        // The grid starts at 20 Hz, which for these alignments is inside the
        // transition rather than far below it, so the visible rotation is
        // substantial but short of the full 360 degrees.
        const double totalRotation = phase.front() - phase.back();
        expect(totalRotation > 200.0 && totalRotation < 365.0,
               QStringLiteral("%1: total rotation %2 deg, expected between 200 and 365")
                   .arg(name).arg(totalRotation, 0, 'f', 1));

        // Monotonic decrease, with a small tolerance for rounding.
        for (std::size_t index = 1; index < phase.size(); ++index) {
            if (phase[index] > phase[index - 1] + 1e-6) {
                reportFailure(QStringLiteral("%1: phase increases at %2 Hz (%3 -> %4 deg)")
                                  .arg(name)
                                  .arg(frequencies[index], 0, 'f', 1)
                                  .arg(phase[index - 1], 0, 'f', 2)
                                  .arg(phase[index], 0, 'f', 2));
                break;
            }
        }
    }
}

void testOtherEnclosureTypesAreUntouched()
{
    // Sealed and Open Baffle already carried phase in simplified mode and must
    // not be affected by this patch.
    for (const EnclosureType type : {EnclosureType::OpenBaffle, EnclosureType::Sealed}) {
        driver subject;
        subject.setF0(45.0);
        subject.setVb(type == EnclosureType::Sealed ? 25.0 : 0.0);
        subject.setEnclosureTypeProposal(type);
        subject.setFullCircuit(false);
        subject.calculatePressureResponse();

        bool anyPhase = false;
        bool allFinite = true;
        for (const std::complex<double>& sample : subject.pressureResponse()) {
            if (!std::isfinite(sample.real()) || !std::isfinite(sample.imag())) {
                allFinite = false;
            }
            if (std::abs(std::arg(sample)) > 1e-6) {
                anyPhase = true;
            }
        }
        expect(allFinite, QStringLiteral("Enclosure type %1: non-finite samples")
                              .arg(static_cast<int>(type)));
        expect(anyPhase, QStringLiteral("Enclosure type %1: expected phase in simplified mode")
                             .arg(static_cast<int>(type)));
    }
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    (void)app;

    testMagnitudeIsUnchanged();
    testPhaseIsPresentAndPlausible();
    testOtherEnclosureTypesAreUntouched();

    if (failures != 0) {
        QTextStream(stderr) << failures << " vented rolloff check(s) failed\n";
        return 1;
    }

    QTextStream(stdout) << "Vented rolloff phase smoke test passed.\n";
    return 0;
}
