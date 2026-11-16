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

// Values of "Fit <name>: <value> [unit]" comment lines; NaN marks a missing entry.
struct FitHeaderValues
{
    double re = std::numeric_limits<double>::quiet_NaN();
    double fs = std::numeric_limits<double>::quiet_NaN();
    double qms = std::numeric_limits<double>::quiet_NaN();
    double qes = std::numeric_limits<double>::quiet_NaN();
    double qts = std::numeric_limits<double>::quiet_NaN();
    double le = std::numeric_limits<double>::quiet_NaN();
    double r2 = std::numeric_limits<double>::quiet_NaN();
    double l2 = std::numeric_limits<double>::quiet_NaN();
};

void parseFitHeaderLine(const QString& comment, FitHeaderValues& values)
{
    static const QRegularExpression fitLine(
        QStringLiteral("^Fit\\s+(Re|Fs|Qms|Qes|Qts|Le|R2|L2)\\s*:\\s*(\\S+)\\s*(\\S*)"));

    const QRegularExpressionMatch match = fitLine.match(comment);
    double value = 0.0;
    if (!match.hasMatch() || !parseNumber(match.captured(2), value)) {
        return;
    }

    // Inductances are written in mH; kfilter keeps henry.
    if (match.captured(3).compare(QStringLiteral("mH"), Qt::CaseInsensitive) == 0) {
        value /= 1000.0;
    }

    const QString name = match.captured(1);
    if (name == QStringLiteral("Re")) {
        values.re = value;
    } else if (name == QStringLiteral("Fs")) {
        values.fs = value;
    } else if (name == QStringLiteral("Qms")) {
        values.qms = value;
    } else if (name == QStringLiteral("Qes")) {
        values.qes = value;
    } else if (name == QStringLiteral("Qts")) {
        values.qts = value;
    } else if (name == QStringLiteral("Le")) {
        values.le = value;
    } else if (name == QStringLiteral("R2")) {
        values.r2 = value;
    } else {
        values.l2 = value;
    }
}

KFilterZmaFitParameters fitParametersFrom(const FitHeaderValues& values)
{
    const auto positive = [](double value) { return std::isfinite(value) && value > 0.0; };
    const auto nonNegative = [](double value) { return std::isfinite(value) && value >= 0.0; };

    KFilterZmaFitParameters parameters;
    parameters.valid = positive(values.re) && positive(values.fs) && positive(values.qms) &&
                       positive(values.qes) && positive(values.qts);
    if (parameters.valid) {
        parameters.rdcOhm = values.re;
        parameters.fsHz = values.fs;
        parameters.qms = values.qms;
        parameters.qes = values.qes;
        parameters.qts = values.qts;
    }

    parameters.hasVoiceCoil = parameters.valid && nonNegative(values.le) &&
                              nonNegative(values.r2) && nonNegative(values.l2);
    if (parameters.hasVoiceCoil) {
        parameters.lspH = values.le;
        parameters.r2Ohm = values.r2;
        parameters.l2H = values.l2;
    }
    return parameters;
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
    FitHeaderValues fitValues;
    QTextStream stream(&file);
    int lineNumber = 0;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString trimmed = stream.readLine().trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("//"))) {
            parseFitHeaderLine(trimmed.mid(2).trimmed(), fitValues);
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('#')) || trimmed.startsWith(QLatin1Char(';')) ||
            trimmed.startsWith(QLatin1Char('*'))) {
            parseFitHeaderLine(trimmed.mid(1).trimmed(), fitValues);
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
    result.fitParameters = fitParametersFrom(fitValues);
    if (result.ignoredLineCount > 0) {
        result.warnings.append(
            QStringLiteral("%1 non-data or invalid line(s) were ignored.")
                .arg(result.ignoredLineCount));
    }

    return result;
}
