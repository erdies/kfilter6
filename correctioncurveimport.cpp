/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "correctioncurveimport.h"

#include <algorithm>
#include <cmath>

namespace
{
bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

double median(QVector<double> values)
{
    std::sort(values.begin(), values.end());
    const qsizetype count = values.size();
    if (count == 0) {
        return 0.0;
    }
    if ((count % 2) != 0) {
        return values.at(count / 2);
    }
    return (values.at((count / 2) - 1) + values.at(count / 2)) / 2.0;
}

double smoothstep(double value)
{
    const double t = std::clamp(value, 0.0, 1.0);
    return t * t * (3.0 - (2.0 * t));
}

double logarithmicFraction(double frequencyHz, double startHz, double endHz)
{
    if (!finitePositive(frequencyHz) || !finitePositive(startHz) ||
        !finitePositive(endHz) || endHz <= startHz) {
        return 0.0;
    }

    return (std::log(frequencyHz) - std::log(startHz)) /
           (std::log(endHz) - std::log(startHz));
}

bool appendOrReplacePoint(KFilterMeasurementCurve& curve, double frequencyHz, double value)
{
    if (!curve.points.isEmpty() && curve.points.constLast().frequencyHz == frequencyHz) {
        curve.points.last().value = value;
        return true;
    }
    return curve.appendPoint(frequencyHz, value);
}
}

bool KFilterCorrectionImportResult::isValid() const
{
    return errorMessage.isEmpty() && correctionCurve.size() >= 2;
}

KFilterCorrectionImportResult createKFilterCorrectionCurve(
    const QVector<KFilterImportedMeasurementPoint>& measurements,
    const KFilterCorrectionImportSettings& settings)
{
    KFilterCorrectionImportResult result;

    if (measurements.size() < 2) {
        result.errorMessage = QStringLiteral("At least two measurement points are required.");
        return result;
    }
    if (!finitePositive(settings.calibrationMinHz) ||
        !finitePositive(settings.calibrationMaxHz) ||
        settings.calibrationMinHz >= settings.calibrationMaxHz) {
        result.errorMessage = QStringLiteral("The calibration range is invalid.");
        return result;
    }
    if (!finitePositive(settings.correctionMinHz) ||
        !finitePositive(settings.correctionMaxHz) ||
        settings.correctionMinHz >= settings.correctionMaxHz) {
        result.errorMessage = QStringLiteral("The correction window is invalid.");
        return result;
    }
    if (!std::isfinite(settings.manualOffsetDb)) {
        result.errorMessage = QStringLiteral("The manual offset is invalid.");
        return result;
    }
    if ((settings.lowerFadeEnabled &&
         (!finitePositive(settings.lowerFadeOctaves))) ||
        (settings.upperFadeEnabled &&
         (!finitePositive(settings.upperFadeOctaves)))) {
        result.errorMessage = QStringLiteral("Enabled fade widths must be positive.");
        return result;
    }

    for (const KFilterImportedMeasurementPoint& point : measurements) {
        if (!finitePositive(point.frequencyHz) || !std::isfinite(point.levelDb)) {
            result.errorMessage = QStringLiteral("The measurement data contain an invalid value.");
            return result;
        }
    }

    QVector<KFilterImportedMeasurementPoint> sorted = measurements;
    std::sort(sorted.begin(), sorted.end(),
              [](const KFilterImportedMeasurementPoint& lhs,
                 const KFilterImportedMeasurementPoint& rhs) {
                  return lhs.frequencyHz < rhs.frequencyHz;
              });

    const double sourceMinHz = sorted.constFirst().frequencyHz;
    const double sourceMaxHz = sorted.constLast().frequencyHz;
    if (settings.calibrationMinHz < sourceMinHz || settings.calibrationMaxHz > sourceMaxHz) {
        result.errorMessage = QStringLiteral(
            "The calibration range must lie within the measurement frequency range.");
        return result;
    }
    if (settings.correctionMinHz < sourceMinHz || settings.correctionMaxHz > sourceMaxHz) {
        result.errorMessage = QStringLiteral(
            "The correction window must lie within the measurement frequency range.");
        return result;
    }

    QVector<double> calibrationLevels;
    int correctionWindowSourcePointCount = 0;
    for (const KFilterImportedMeasurementPoint& point : sorted) {
        if (point.frequencyHz >= settings.calibrationMinHz &&
            point.frequencyHz <= settings.calibrationMaxHz) {
            calibrationLevels.append(point.levelDb);
        }
        if (point.frequencyHz >= settings.correctionMinHz &&
            point.frequencyHz <= settings.correctionMaxHz) {
            ++correctionWindowSourcePointCount;
        }
    }

    if (calibrationLevels.isEmpty()) {
        result.errorMessage = QStringLiteral(
            "The calibration range does not contain a measurement point.");
        return result;
    }
    if (calibrationLevels.size() < 3) {
        result.warnings.append(QStringLiteral(
            "The calibration range contains fewer than three points; the offset may be sensitive to individual samples."));
    }
    if (correctionWindowSourcePointCount < 2) {
        result.errorMessage = QStringLiteral(
            "The correction window must contain at least two measurement points.");
        return result;
    }

    result.referenceMedianDb = median(calibrationLevels);
    result.automaticOffsetDb = -result.referenceMedianDb;
    result.effectiveOffsetDb = result.automaticOffsetDb + settings.manualOffsetDb;

    KFilterMeasurementCurve normalizedSource;
    for (const KFilterImportedMeasurementPoint& point : sorted) {
        const double normalizedValue = point.levelDb + result.effectiveOffsetDb;
        if (!normalizedSource.appendPoint(point.frequencyHz, normalizedValue)) {
            result.errorMessage = QStringLiteral(
                "The measurement frequencies must be distinct and strictly increasing.");
            return result;
        }
        result.calibratedMeasurement.append(
            KFilterMeasurementPoint{point.frequencyHz, normalizedValue});
    }

    const double lowerFadeEndHz = settings.lowerFadeEnabled
                                      ? settings.correctionMinHz *
                                            std::pow(2.0, settings.lowerFadeOctaves)
                                      : settings.correctionMinHz;
    const double upperFadeStartHz = settings.upperFadeEnabled
                                        ? settings.correctionMaxHz /
                                              std::pow(2.0, settings.upperFadeOctaves)
                                        : settings.correctionMaxHz;

    if (settings.lowerFadeEnabled && lowerFadeEndHz >= settings.correctionMaxHz) {
        result.errorMessage = QStringLiteral(
            "The lower fade consumes the complete correction window.");
        return result;
    }
    if (settings.upperFadeEnabled && upperFadeStartHz <= settings.correctionMinHz) {
        result.errorMessage = QStringLiteral(
            "The upper fade consumes the complete correction window.");
        return result;
    }
    if (settings.lowerFadeEnabled && settings.upperFadeEnabled &&
        lowerFadeEndHz >= upperFadeStartHz) {
        result.errorMessage = QStringLiteral(
            "The lower and upper fades overlap. Reduce one or both fade widths.");
        return result;
    }

    auto effectiveCorrection = [&](double frequencyHz, double normalizedValue) {
        double weight = 1.0;
        if (settings.lowerFadeEnabled && frequencyHz <= lowerFadeEndHz) {
            weight *= smoothstep(logarithmicFraction(
                frequencyHz, settings.correctionMinHz, lowerFadeEndHz));
        }
        if (settings.upperFadeEnabled && frequencyHz >= upperFadeStartHz) {
            weight *= 1.0 - smoothstep(logarithmicFraction(
                frequencyHz, upperFadeStartHz, settings.correctionMaxHz));
        }
        return normalizedValue * weight;
    };

    double lowerBoundaryValue = 0.0;
    if (!normalizedSource.interpolatedValueAt(settings.correctionMinHz, lowerBoundaryValue)) {
        result.errorMessage = QStringLiteral("Cannot interpolate the lower correction boundary.");
        return result;
    }
    appendOrReplacePoint(result.correctionCurve,
                         settings.correctionMinHz,
                         effectiveCorrection(settings.correctionMinHz, lowerBoundaryValue));

    for (const KFilterMeasurementPoint& point : normalizedSource.points) {
        if (point.frequencyHz <= settings.correctionMinHz ||
            point.frequencyHz >= settings.correctionMaxHz) {
            continue;
        }
        appendOrReplacePoint(result.correctionCurve,
                             point.frequencyHz,
                             effectiveCorrection(point.frequencyHz, point.value));
    }

    double upperBoundaryValue = 0.0;
    if (!normalizedSource.interpolatedValueAt(settings.correctionMaxHz, upperBoundaryValue)) {
        result.errorMessage = QStringLiteral("Cannot interpolate the upper correction boundary.");
        result.correctionCurve.clear();
        return result;
    }
    appendOrReplacePoint(result.correctionCurve,
                         settings.correctionMaxHz,
                         effectiveCorrection(settings.correctionMaxHz, upperBoundaryValue));

    if (result.correctionCurve.size() < 2) {
        result.errorMessage = QStringLiteral(
            "The correction window does not contain enough usable points.");
        result.correctionCurve.clear();
        return result;
    }

    return result;
}
