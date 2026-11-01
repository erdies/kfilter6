/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "impedancereferencecurve.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>
#include <limits>

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

    // Decimal commas are common in exports from localized tools.
    token.replace(QLatin1Char(','), QLatin1Char('.'));
    value = token.toDouble(&ok);
    return ok && std::isfinite(value);
}

bool parseZmaLine(const QString& line, KFilterImpedancePoint& point)
{
    static const QRegularExpression whitespaceOrSemicolon(QStringLiteral("[\\s;]+"));

    const QStringList tokens = line.split(whitespaceOrSemicolon, Qt::SkipEmptyParts);
    if (tokens.size() < 2 ||
        !parseNumber(tokens.at(0), point.frequencyHz) ||
        !parseNumber(tokens.at(1), point.magnitudeOhm)) {
        return false;
    }

    point.phaseDeg = std::numeric_limits<double>::quiet_NaN();
    if (tokens.size() >= 3 && !parseNumber(tokens.at(2), point.phaseDeg)) {
        return false;
    }

    return point.frequencyHz > 0.0 && point.magnitudeOhm >= 0.0;
}
}

bool KFilterImpedanceParseResult::isValid() const
{
    return errorMessage.isEmpty() && points.size() >= 2;
}

KFilterImpedanceParseResult parseKFilterZmaFile(const QString& fileName)
{
    KFilterImpedanceParseResult result;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("Cannot open impedance file '%1': %2")
                                  .arg(fileName)
                                  .arg(file.errorString());
        return result;
    }

    KFilterImpedanceReferenceCurve parsedPoints;
    QTextStream stream(&file);
    int lineNumber = 0;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString trimmed = stream.readLine().trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')) ||
            trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('*')) ||
            trimmed.startsWith(QStringLiteral("//"))) {
            continue;
        }

        KFilterImpedancePoint point;
        if (!parseZmaLine(trimmed, point)) {
            ++result.ignoredLineCount;
            continue;
        }

        if (!parsedPoints.isEmpty() && point.frequencyHz <= parsedPoints.constLast().frequencyHz) {
            result.errorMessage = QStringLiteral(
                "Frequencies in the impedance file must strictly increase (line %1: %2 Hz).")
                                      .arg(lineNumber)
                                      .arg(point.frequencyHz);
            return result;
        }

        parsedPoints.append(point);
    }

    if (stream.status() != QTextStream::Ok) {
        result.errorMessage = QStringLiteral("Failed while reading impedance file '%1'.")
                                  .arg(fileName);
        return result;
    }

    if (parsedPoints.size() < 2) {
        result.errorMessage = QStringLiteral(
            "The impedance file does not contain at least two valid frequency/impedance rows.");
        return result;
    }

    result.points = parsedPoints;
    if (result.ignoredLineCount > 0) {
        result.warnings.append(
            QStringLiteral("%1 non-data or invalid line(s) were ignored.")
                .arg(result.ignoredLineCount));
    }

    return result;
}
