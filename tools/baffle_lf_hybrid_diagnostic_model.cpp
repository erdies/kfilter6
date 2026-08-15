/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffle_lf_hybrid_diagnostic_model.h"
#include "kfilterfrequencygrid.h"

#include <cmath>
#include <complex>
#include <limits>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double TwoPi = 2.0 * Pi;
constexpr double SpeedOfSoundMPerS = 343.0;
constexpr double WidthExponentN1 = 1.0;
constexpr double WidthExponentN15 = 1.5;
constexpr double WidthExponentN2 = 2.0;

bool finiteComplex(const std::complex<double>& value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

double magnitudeDb(const std::complex<double>& value)
{
    const double magnitude = std::abs(value);
    if (!std::isfinite(magnitude) || magnitude <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return 20.0 * std::log10(magnitude);
}

bool magnitudeDbWithPhase(double valueDb,
                          const std::complex<double>& phaseReference,
                          std::complex<double>& result)
{
    const double magnitude = std::pow(10.0, valueDb / 20.0);
    const double referenceMagnitude = std::abs(phaseReference);
    if (!std::isfinite(valueDb) || !std::isfinite(magnitude) || magnitude <= 0.0 ||
        !std::isfinite(referenceMagnitude) || referenceMagnitude <= 0.0) {
        return false;
    }
    result = phaseReference * (magnitude / referenceMagnitude);
    return finiteComplex(result);
}

bool setHybridSample(double simpleDb,
                     double rectangularDb,
                     double weight,
                     const std::complex<double>& rectangularValue,
                     std::complex<double>& hybridValue)
{
    if (!std::isfinite(weight) || weight < 0.0 || weight >= 1.0 ||
        !std::isfinite(simpleDb) || !std::isfinite(rectangularDb)) {
        return false;
    }

    const double hybridDb = simpleDb + weight * (rectangularDb - simpleDb);
    return magnitudeDbWithPhase(hybridDb, rectangularValue, hybridValue);
}
}

double baffleLfHybridBlendWeight(double widthMm,
                                 double heightMm,
                                 double frequencyHz)
{
    if (!std::isfinite(widthMm) || !std::isfinite(heightMm) ||
        !std::isfinite(frequencyHz) ||
        widthMm <= 0.0 || heightMm <= 0.0 || frequencyHz < 0.0) {
        return 0.0;
    }

    const double widthM = widthMm / 1000.0;
    const double heightM = heightMm / 1000.0;
    const double effectiveLengthM = std::sqrt(widthM * heightM);
    const double x = TwoPi * frequencyHz * effectiveLengthM / SpeedOfSoundMPerS;
    if (!std::isfinite(x) || x < 0.0) {
        return 0.0;
    }

    return x / (x + Pi);
}

double baffleLfWidthAnchoredBlendWeight(double widthMm,
                                        double frequencyHz,
                                        double exponent)
{
    if (!std::isfinite(widthMm) || !std::isfinite(frequencyHz) ||
        !std::isfinite(exponent) || widthMm <= 0.0 || frequencyHz < 0.0 ||
        exponent <= 0.0) {
        return 0.0;
    }

    const double midpointHz = simpleBaffleStepMidpointFrequencyHz(widthMm);
    if (!std::isfinite(midpointHz) || midpointHz <= 0.0) {
        return 0.0;
    }

    if (frequencyHz == 0.0) {
        return 0.0;
    }

    const double ratio = frequencyHz / midpointHz;
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        return 0.0;
    }

    const double poweredRatio = std::pow(ratio, exponent);
    if (!std::isfinite(poweredRatio) || poweredRatio <= 0.0) {
        return 0.0;
    }

    const double weight = poweredRatio / (1.0 + poweredRatio);
    return std::isfinite(weight) && weight >= 0.0 && weight < 1.0 ? weight : 0.0;
}

double baffleLfWidthAnchoredBlendWeight(double widthMm,
                                        double frequencyHz)
{
    return baffleLfWidthAnchoredBlendWeight(widthMm, frequencyHz, WidthExponentN1);
}

BaffleLfHybridDiagnostic calculateBaffleLfHybridDiagnostic(
    const BaffleSettings& rectangularSettings,
    double effectiveDriverDiameterCm)
{
    BaffleLfHybridDiagnostic diagnostic;

    if (!rectangularSettings.enabled ||
        rectangularSettings.model != BaffleModel::RectangularEdgeDiffraction ||
        rectangularSettings.boundaryCondition != BaffleBoundaryCondition::FreeField ||
        rectangularSettings.leftEdgeTreatment != BaffleSideEdgeTreatment::Sharp ||
        rectangularSettings.rightEdgeTreatment != BaffleSideEdgeTreatment::Sharp ||
        !std::isfinite(rectangularSettings.widthMm) ||
        !std::isfinite(rectangularSettings.heightMm) ||
        rectangularSettings.widthMm <= 0.0 || rectangularSettings.heightMm <= 0.0) {
        return diagnostic;
    }

    BaffleSettings simpleSettings = rectangularSettings;
    simpleSettings.model = BaffleModel::SimpleBaffleStep;
    simpleSettings.boundaryCondition = BaffleBoundaryCondition::FreeField;

    diagnostic.simple = calculateBaffleResponse(simpleSettings, 0.0);
    diagnostic.rectangular =
        calculateBaffleUnblendedRectangularResponseForDiagnostic(
            rectangularSettings, effectiveDriverDiameterCm);

    if (diagnostic.simple.status != BaffleResponseStatus::Valid ||
        diagnostic.rectangular.status != BaffleResponseStatus::Valid) {
        return diagnostic;
    }

    const double widthM = rectangularSettings.widthMm / 1000.0;
    const double heightM = rectangularSettings.heightMm / 1000.0;
    diagnostic.effectiveLengthM = std::sqrt(widthM * heightM);
    diagnostic.simpleMidpointFrequencyHz =
        simpleBaffleStepMidpointFrequencyHz(rectangularSettings.widthMm);
    if (!std::isfinite(diagnostic.effectiveLengthM) || diagnostic.effectiveLengthM <= 0.0 ||
        !std::isfinite(diagnostic.simpleMidpointFrequencyHz) ||
        diagnostic.simpleMidpointFrequencyHz <= 0.0) {
        return diagnostic;
    }

    diagnostic.hybrid.status = BaffleResponseStatus::Valid;
    diagnostic.widthAnchoredHybrid.status = BaffleResponseStatus::Valid;
    diagnostic.widthAnchoredHybridN15.status = BaffleResponseStatus::Valid;
    diagnostic.widthAnchoredHybridN2.status = BaffleResponseStatus::Valid;
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();

    for (std::size_t sampleIndex = 0; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
        const double frequencyHz = frequencies[sampleIndex];
        const double weight = baffleLfHybridBlendWeight(rectangularSettings.widthMm,
                                                        rectangularSettings.heightMm,
                                                        frequencyHz);
        const double widthWeight =
            baffleLfWidthAnchoredBlendWeight(rectangularSettings.widthMm,
                                             frequencyHz,
                                             WidthExponentN1);
        const double widthWeightN15 =
            baffleLfWidthAnchoredBlendWeight(rectangularSettings.widthMm,
                                             frequencyHz,
                                             WidthExponentN15);
        const double widthWeightN2 =
            baffleLfWidthAnchoredBlendWeight(rectangularSettings.widthMm,
                                             frequencyHz,
                                             WidthExponentN2);
        const double simpleDb = magnitudeDb(diagnostic.simple.values[sampleIndex]);
        const double rectangularDb = magnitudeDb(diagnostic.rectangular.values[sampleIndex]);

        std::complex<double> hybridValue;
        std::complex<double> widthHybridValue;
        std::complex<double> widthHybridValueN15;
        std::complex<double> widthHybridValueN2;
        const std::complex<double>& rectangularValue = diagnostic.rectangular.values[sampleIndex];
        if (!setHybridSample(simpleDb, rectangularDb, weight,
                             rectangularValue, hybridValue) ||
            !setHybridSample(simpleDb, rectangularDb, widthWeight,
                             rectangularValue, widthHybridValue) ||
            !setHybridSample(simpleDb, rectangularDb, widthWeightN15,
                             rectangularValue, widthHybridValueN15) ||
            !setHybridSample(simpleDb, rectangularDb, widthWeightN2,
                             rectangularValue, widthHybridValueN2)) {
            diagnostic.hybrid.status = BaffleResponseStatus::InvalidParameters;
            diagnostic.widthAnchoredHybrid.status = BaffleResponseStatus::InvalidParameters;
            diagnostic.widthAnchoredHybridN15.status = BaffleResponseStatus::InvalidParameters;
            diagnostic.widthAnchoredHybridN2.status = BaffleResponseStatus::InvalidParameters;
            return diagnostic;
        }

        diagnostic.blendWeight[sampleIndex] = weight;
        diagnostic.widthAnchoredBlendWeight[sampleIndex] = widthWeight;
        diagnostic.widthAnchoredBlendWeightN15[sampleIndex] = widthWeightN15;
        diagnostic.widthAnchoredBlendWeightN2[sampleIndex] = widthWeightN2;
        diagnostic.hybrid.values[sampleIndex] = hybridValue;
        diagnostic.widthAnchoredHybrid.values[sampleIndex] = widthHybridValue;
        diagnostic.widthAnchoredHybridN15.values[sampleIndex] = widthHybridValueN15;
        diagnostic.widthAnchoredHybridN2.values[sampleIndex] = widthHybridValueN2;
    }

    diagnostic.valid = true;
    return diagnostic;
}
