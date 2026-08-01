/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "measurementcurveparser.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace
{
bool parseNumber(QString token, double& value)
{
    token = token.trimmed();
    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    value = token.toDouble(&ok);
    if (ok && std::isfinite(value)) {
        return true;
    }

    // Decimal commas are common in measurement exports from localized tools.
    // This fallback is only used after normal tokenization, so commas used as
    // column separators are handled separately.
    token.replace(QLatin1Char(','), QLatin1Char('.'));
    value = token.toDouble(&ok);
    return ok && std::isfinite(value);
}

bool parseMeasurementLine(const QString& line, double& frequencyHz, double& levelDb)
{
    static const QRegularExpression whitespaceOrSemicolon(QStringLiteral("[\\s;]+"));
    static const QRegularExpression commaSeparator(QStringLiteral("\\s*,\\s*"));

    QStringList tokens = line.split(whitespaceOrSemicolon, Qt::SkipEmptyParts);
    if (tokens.size() >= 2 &&
        parseNumber(tokens.at(0), frequencyHz) &&
        parseNumber(tokens.at(1), levelDb)) {
        return true;
    }

    // A comma-delimited row is tried only when whitespace/semicolon parsing did
    // not already identify two numeric columns. This preserves decimal-comma
    // input such as "100,0;85,3" or "100,0 85,3".
    tokens = line.split(commaSeparator, Qt::SkipEmptyParts);
    return tokens.size() >= 2 &&
           parseNumber(tokens.at(0), frequencyHz) &&
           parseNumber(tokens.at(1), levelDb);
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
}

bool KFilterMeasurementParseResult::isValid() const
{
    return errorMessage.isEmpty() && points.size() >= 2;
}

KFilterMeasurementParseResult parseKFilterMeasurementFile(const QString& fileName)
{
    KFilterMeasurementParseResult result;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("Cannot open measurement file '%1': %2")
                                  .arg(fileName)
                                  .arg(file.errorString());
        return result;
    }

    QVector<KFilterImportedMeasurementPoint> parsedPoints;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString trimmed = stream.readLine().trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')) ||
            trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('*')) ||
            trimmed.startsWith(QStringLiteral("//"))) {
            continue;
        }

        double frequencyHz = 0.0;
        double levelDb = 0.0;
        if (!parseMeasurementLine(trimmed, frequencyHz, levelDb) || frequencyHz <= 0.0) {
            ++result.ignoredLineCount;
            continue;
        }

        parsedPoints.append(KFilterImportedMeasurementPoint{frequencyHz, levelDb});
    }

    if (stream.status() != QTextStream::Ok) {
        result.errorMessage = QStringLiteral("Failed while reading measurement file '%1'.")
                                  .arg(fileName);
        return result;
    }

    if (parsedPoints.size() < 2) {
        result.errorMessage = QStringLiteral(
            "The measurement file does not contain at least two valid frequency/level rows.");
        return result;
    }

    std::sort(parsedPoints.begin(), parsedPoints.end(),
              [](const KFilterImportedMeasurementPoint& lhs,
                 const KFilterImportedMeasurementPoint& rhs) {
                  return lhs.frequencyHz < rhs.frequencyHz;
              });

    for (qsizetype index = 0; index < parsedPoints.size();) {
        const double frequencyHz = parsedPoints.at(index).frequencyHz;
        QVector<double> duplicateLevels;
        duplicateLevels.append(parsedPoints.at(index).levelDb);

        qsizetype next = index + 1;
        while (next < parsedPoints.size() &&
               parsedPoints.at(next).frequencyHz == frequencyHz) {
            duplicateLevels.append(parsedPoints.at(next).levelDb);
            ++next;
        }

        if (duplicateLevels.size() > 1) {
            result.duplicateFrequencyCount += static_cast<int>(duplicateLevels.size() - 1);
        }
        result.points.append(
            KFilterImportedMeasurementPoint{frequencyHz, median(duplicateLevels)});
        index = next;
    }

    if (result.points.size() < 2) {
        result.errorMessage = QStringLiteral(
            "The measurement file contains fewer than two distinct valid frequencies.");
        result.points.clear();
        return result;
    }

    if (result.ignoredLineCount > 0) {
        result.warnings.append(
            QStringLiteral("%1 non-data or invalid line(s) were ignored.")
                .arg(result.ignoredLineCount));
    }
    if (result.duplicateFrequencyCount > 0) {
        result.warnings.append(
            QStringLiteral("%1 duplicate frequency row(s) were combined using the median level.")
                .arg(result.duplicateFrequencyCount));
    }

    return result;
}
