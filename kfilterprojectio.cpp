/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterprojectio.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLocale>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace
{
constexpr auto CaseInsensitive = Qt::CaseInsensitive;
const QString JsonFormatName = QStringLiteral("KFilter project");
const QString ApplicationName = QStringLiteral("KFilter6");

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

QString valueAfterEquals(const QString& line)
{
    const qsizetype equalsPosition = line.indexOf(QLatin1Char('='));
    if (equalsPosition < 0) {
        return line.trimmed();
    }

    return line.mid(equalsPosition + 1).trimmed();
}

bool readNextDataLine(const QStringList& lines, qsizetype& index, QString& line)
{
    while (index < lines.size()) {
        line = lines.at(index++).trimmed();
        if (!line.startsWith(QLatin1Char('#'))) {
            return true;
        }
    }

    return false;
}

bool parseDoubleValue(const QString& line, double& value)
{
    bool ok = false;
    value = QLocale::c().toDouble(valueAfterEquals(line), &ok);
    return ok && std::isfinite(value);
}

bool parseIntValue(const QString& line, int& value)
{
    bool ok = false;
    value = QLocale::c().toInt(valueAfterEquals(line), &ok);
    return ok;
}

bool parseBoolValue(const QString& line, bool& value)
{
    int numericValue = 0;
    if (!parseIntValue(line, numericValue)) {
        return false;
    }

    value = (numericValue == 1);
    return true;
}

bool parseNetworkValues(const QStringList& lines,
                        qsizetype& index,
                        driver (&drivers)[KFilterProjectIo::DriverCount],
                        QString* errorMessage)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
            QString line;
            if (!readNextDataLine(lines, index, line)) {
                setError(errorMessage, QStringLiteral("Project file ended while reading network values."));
                return false;
            }

            if (line.startsWith(QLatin1Char('['))) {
                --index;
                return true;
            }

            bool ok = false;
            const double value = QLocale::c().toDouble(line, &ok);
            if (!ok || !std::isfinite(value)) {
                setError(errorMessage,
                         QStringLiteral("Invalid network value for driver %1, unit %2: '%3'")
                             .arg(driverIndex + 1)
                             .arg(unitIndex)
                             .arg(line));
                return false;
            }

            drivers[driverIndex].setUnit(unitIndex, value);
        }
    }

    return true;
}

bool readRequiredDataLine(const QStringList& lines,
                          qsizetype& index,
                          QString& line,
                          const QString& fieldName,
                          int driverNumber,
                          QString* errorMessage)
{
    if (!readNextDataLine(lines, index, line) || line.startsWith(QLatin1Char('['))) {
        setError(errorMessage,
                 QStringLiteral("Project file ended while reading '%1' for driver %2.")
                     .arg(fieldName)
                     .arg(driverNumber));
        return false;
    }

    return true;
}

bool parseDriverParameters(const QStringList& lines,
                           qsizetype& index,
                           driver (&drivers)[KFilterProjectIo::DriverCount],
                           QString* errorMessage)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& currentDriver = drivers[driverIndex];
        const int driverNumber = driverIndex + 1;
        QString line;
        double doubleValue = 0.0;
        int intValue = 0;
        bool boolValue = false;

#define READ_DOUBLE_FIELD(fieldName, setterCall) \
        if (!readRequiredDataLine(lines, index, line, QStringLiteral(fieldName), driverNumber, errorMessage) || \
            !parseDoubleValue(line, doubleValue)) { \
            setError(errorMessage, QStringLiteral("Invalid value for '%1' in driver %2: '%3'") \
                                       .arg(QStringLiteral(fieldName)) \
                                       .arg(driverNumber) \
                                       .arg(line)); \
            return false; \
        } \
        setterCall

#define READ_INT_FIELD(fieldName, target) \
        if (!readRequiredDataLine(lines, index, line, QStringLiteral(fieldName), driverNumber, errorMessage) || \
            !parseIntValue(line, intValue)) { \
            setError(errorMessage, QStringLiteral("Invalid value for '%1' in driver %2: '%3'") \
                                       .arg(QStringLiteral(fieldName)) \
                                       .arg(driverNumber) \
                                       .arg(line)); \
            return false; \
        } \
        target = intValue

#define READ_BOOL_FIELD(fieldName, target) \
        if (!readRequiredDataLine(lines, index, line, QStringLiteral(fieldName), driverNumber, errorMessage) || \
            !parseBoolValue(line, boolValue)) { \
            setError(errorMessage, QStringLiteral("Invalid value for '%1' in driver %2: '%3'") \
                                       .arg(QStringLiteral(fieldName)) \
                                       .arg(driverNumber) \
                                       .arg(line)); \
            return false; \
        } \
        target = boolValue

        READ_DOUBLE_FIELD("Rdc", currentDriver.setRdc(doubleValue));
        READ_DOUBLE_FIELD("Lsp", currentDriver.setLsp(doubleValue));
        READ_DOUBLE_FIELD("F0", currentDriver.setF0(doubleValue));
        READ_DOUBLE_FIELD("Qts", currentDriver.setQtc(doubleValue));
        READ_DOUBLE_FIELD("Qe", currentDriver.setQes(doubleValue));
        READ_DOUBLE_FIELD("Qms", currentDriver.setQms(doubleValue));
        READ_DOUBLE_FIELD("Vas", currentDriver.setVas(doubleValue));
        READ_DOUBLE_FIELD("Dm", currentDriver.setDm(doubleValue));
        READ_DOUBLE_FIELD("Vb", currentDriver.Vb = doubleValue);
        READ_DOUBLE_FIELD("Fb", currentDriver.Fb = doubleValue);
        READ_DOUBLE_FIELD("V2", currentDriver.V2 = doubleValue);
        READ_INT_FIELD("GTypProposal", currentDriver.GTypProposal);
        READ_DOUBLE_FIELD("Gain", currentDriver.gain = doubleValue);
        READ_BOOL_FIELD("Pressure", currentDriver.PressureisActive);
        READ_BOOL_FIELD("Impedanz", currentDriver.ImpedanzisActive);
        READ_BOOL_FIELD("Summary", currentDriver.SummaryisActive);
        READ_BOOL_FIELD("ScalarSummary", currentDriver.ScalarSummaryisActive);
        READ_BOOL_FIELD("ImpedanzSummary", currentDriver.ImpedanzSummaryisActive);
        READ_BOOL_FIELD("InvertPhase", currentDriver.InvertPhase);

        if (!readRequiredDataLine(lines, index, line, QStringLiteral("Title"), driverNumber, errorMessage)) {
            return false;
        }
        currentDriver.SetTitle(valueAfterEquals(line));

#undef READ_BOOL_FIELD
#undef READ_INT_FIELD
#undef READ_DOUBLE_FIELD
    }

    return true;
}

bool parseDriverEnclosureLosses(const QStringList& lines,
                                qsizetype& index,
                                driver (&drivers)[KFilterProjectIo::DriverCount],
                                QString* errorMessage)
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& currentDriver = drivers[driverIndex];
        const int driverNumber = driverIndex + 1;
        QString line;
        double ql = 0.0;

        if (!readRequiredDataLine(lines, index, line, QStringLiteral("Ql"), driverNumber, errorMessage) ||
            !parseDoubleValue(line, ql)) {
            setError(errorMessage,
                     QStringLiteral("Invalid value for 'Ql' in driver %1: '%2'")
                         .arg(driverNumber)
                         .arg(line));
            return false;
        }

        if (ql <= 0.0) {
            setError(errorMessage,
                     QStringLiteral("Invalid value for 'Ql' in driver %1: Ql must be greater than zero.")
                         .arg(driverNumber));
            return false;
        }

        currentDriver.setQl(ql);
    }

    return true;
}

bool loadLegacyProject(const QByteArray& data,
                       driver (&drivers)[KFilterProjectIo::DriverCount],
                       QString* errorMessage)
{
    const QString content = QString::fromUtf8(data);
    if (content.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("Project file is empty."));
        return false;
    }

    const QStringList lines = content.split(QLatin1Char('\n'));
    qsizetype index = 0;
    bool foundKnownSection = false;

    while (index < lines.size()) {
        QString line;
        if (!readNextDataLine(lines, index, line)) {
            break;
        }

        if (line.contains(QStringLiteral("Network values"), CaseInsensitive)) {
            foundKnownSection = true;
            if (!parseNetworkValues(lines, index, drivers, errorMessage)) {
                return false;
            }
        } else if (line.contains(QStringLiteral("Driver parameters"), CaseInsensitive)) {
            foundKnownSection = true;
            if (!parseDriverParameters(lines, index, drivers, errorMessage)) {
                return false;
            }
        } else if (line.contains(QStringLiteral("Driver enclosure losses"), CaseInsensitive)) {
            foundKnownSection = true;
            if (!parseDriverEnclosureLosses(lines, index, drivers, errorMessage)) {
                return false;
            }
        }
    }

    if (!foundKnownSection) {
        setError(errorMessage, QStringLiteral("File is neither a supported JSON project nor a legacy KFilter project."));
        return false;
    }

    return true;
}

bool readObject(const QJsonObject& parent,
                const QString& key,
                QJsonObject& value,
                const QString& context,
                QString* errorMessage)
{
    const QJsonValue jsonValue = parent.value(key);
    if (!jsonValue.isObject()) {
        setError(errorMessage,
                 QStringLiteral("Missing or invalid object '%1.%2'.").arg(context, key));
        return false;
    }

    value = jsonValue.toObject();
    return true;
}

bool readRequiredString(const QJsonObject& object,
                        const QString& key,
                        QString& value,
                        const QString& context,
                        QString* errorMessage)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isString()) {
        setError(errorMessage,
                 QStringLiteral("Missing or invalid string field '%1.%2'.").arg(context, key));
        return false;
    }

    value = jsonValue.toString();
    return true;
}

bool readRequiredDouble(const QJsonObject& object,
                        const QString& key,
                        double& value,
                        const QString& context,
                        QString* errorMessage)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble()) {
        setError(errorMessage,
                 QStringLiteral("Missing or invalid numeric field '%1.%2'.").arg(context, key));
        return false;
    }

    value = jsonValue.toDouble();
    if (!std::isfinite(value)) {
        setError(errorMessage,
                 QStringLiteral("Field '%1.%2' must be finite.").arg(context, key));
        return false;
    }

    return true;
}

bool readRequiredInt(const QJsonObject& object,
                     const QString& key,
                     int& value,
                     const QString& context,
                     QString* errorMessage)
{
    double doubleValue = 0.0;
    if (!readRequiredDouble(object, key, doubleValue, context, errorMessage)) {
        return false;
    }

    if (doubleValue < static_cast<double>(std::numeric_limits<int>::min()) ||
        doubleValue > static_cast<double>(std::numeric_limits<int>::max()) ||
        std::trunc(doubleValue) != doubleValue) {
        setError(errorMessage,
                 QStringLiteral("Field '%1.%2' must be an integer in the supported range.")
                     .arg(context, key));
        return false;
    }

    value = static_cast<int>(doubleValue);
    return true;
}

bool readRequiredBool(const QJsonObject& object,
                      const QString& key,
                      bool& value,
                      const QString& context,
                      QString* errorMessage)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isBool()) {
        setError(errorMessage,
                 QStringLiteral("Missing or invalid boolean field '%1.%2'.").arg(context, key));
        return false;
    }

    value = jsonValue.toBool();
    return true;
}

bool jsonToDriverParameters(const QJsonObject& parameters,
                            driver& currentDriver,
                            const QString& context,
                            QString* errorMessage)
{
    QString title;
    double rdc = 0.0;
    double lsp = 0.0;
    double fs = 0.0;
    double qts = 0.0;
    double qes = 0.0;
    double qms = 0.0;
    double vas = 0.0;
    double diameter = 0.0;
    double vb = 0.0;
    double ql = 0.0;
    double fb = 0.0;
    double v2 = 0.0;
    int enclosureTypeProposal = 0;
    double gain = 0.0;
    bool pressureActive = false;
    bool impedanceActive = false;
    bool summaryActive = false;
    bool scalarSummaryActive = false;
    bool impedanceSummaryActive = false;
    bool invertPhase = false;
    bool fullCircuit = false;

    if (!readRequiredString(parameters, QStringLiteral("title"), title, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("rdc_ohm"), rdc, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("lsp_h"), lsp, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("fs_hz"), fs, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("qts"), qts, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("qes"), qes, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("qms"), qms, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("vas_l"), vas, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("diameter_cm"), diameter, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("vb_l"), vb, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("ql"), ql, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("fb_hz"), fb, context, errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("v2_l"), v2, context, errorMessage) ||
        !readRequiredInt(parameters,
                         QStringLiteral("enclosureTypeProposal"),
                         enclosureTypeProposal,
                         context,
                         errorMessage) ||
        !readRequiredDouble(parameters, QStringLiteral("gainLinear"), gain, context, errorMessage) ||
        !readRequiredBool(parameters, QStringLiteral("pressureActive"), pressureActive, context, errorMessage) ||
        !readRequiredBool(parameters, QStringLiteral("impedanceActive"), impedanceActive, context, errorMessage) ||
        !readRequiredBool(parameters, QStringLiteral("summaryActive"), summaryActive, context, errorMessage) ||
        !readRequiredBool(parameters,
                          QStringLiteral("scalarSummaryActive"),
                          scalarSummaryActive,
                          context,
                          errorMessage) ||
        !readRequiredBool(parameters,
                          QStringLiteral("impedanceSummaryActive"),
                          impedanceSummaryActive,
                          context,
                          errorMessage) ||
        !readRequiredBool(parameters, QStringLiteral("invertPhase"), invertPhase, context, errorMessage) ||
        !readRequiredBool(parameters, QStringLiteral("fullCircuit"), fullCircuit, context, errorMessage)) {
        return false;
    }

    if (ql <= 0.0) {
        setError(errorMessage, QStringLiteral("Field '%1.ql' must be greater than zero.").arg(context));
        return false;
    }

    currentDriver.SetTitle(title);
    currentDriver.setRdc(rdc);
    currentDriver.setLsp(lsp);
    currentDriver.setF0(fs);
    currentDriver.setQtc(qts);
    currentDriver.setQes(qes);
    currentDriver.setQms(qms);
    currentDriver.setVas(vas);
    currentDriver.setDm(diameter);
    currentDriver.Vb = vb;
    currentDriver.setQl(ql);
    currentDriver.Fb = fb;
    currentDriver.V2 = v2;
    currentDriver.GTypProposal = enclosureTypeProposal;
    currentDriver.gain = gain;
    currentDriver.PressureisActive = pressureActive;
    currentDriver.ImpedanzisActive = impedanceActive;
    currentDriver.SummaryisActive = summaryActive;
    currentDriver.ScalarSummaryisActive = scalarSummaryActive;
    currentDriver.ImpedanzSummaryisActive = impedanceSummaryActive;
    currentDriver.InvertPhase = invertPhase;
    currentDriver.setFullCircuit(fullCircuit);
    return true;
}

bool jsonToDriverNetwork(const QJsonObject& network,
                         driver& currentDriver,
                         const QString& context,
                         QString* errorMessage)
{
    int unitBaseIndex = 1;
    if (!readRequiredInt(network,
                         QStringLiteral("unitBaseIndex"),
                         unitBaseIndex,
                         context,
                         errorMessage)) {
        return false;
    }

    if (unitBaseIndex != 1) {
        setError(errorMessage,
                 QStringLiteral("Unsupported network unit base index %1 in '%2'.")
                     .arg(unitBaseIndex)
                     .arg(context));
        return false;
    }

    const QJsonValue valuesValue = network.value(QStringLiteral("values"));
    if (!valuesValue.isArray()) {
        setError(errorMessage,
                 QStringLiteral("Missing or invalid array '%1.values'.").arg(context));
        return false;
    }

    const QJsonArray values = valuesValue.toArray();
    if (values.size() != KFilterProjectIo::NetworkUnitCount) {
        setError(errorMessage,
                 QStringLiteral("Array '%1.values' must contain exactly %2 entries.")
                     .arg(context)
                     .arg(KFilterProjectIo::NetworkUnitCount));
        return false;
    }

    for (int index = 0; index < values.size(); ++index) {
        const QJsonValue value = values.at(index);
        if (!value.isDouble() || !std::isfinite(value.toDouble())) {
            setError(errorMessage,
                     QStringLiteral("Invalid network value '%1.values[%2]'.")
                         .arg(context)
                         .arg(index));
            return false;
        }
        currentDriver.setUnit(index + 1, value.toDouble());
    }

    return true;
}

bool jsonToSplCorrectionCurve(const QJsonObject& correctionObject,
                              KFilterMeasurementCurve& curve,
                              bool& measurementHidden,
                              int formatVersion,
                              const QString& context,
                              QString* errorMessage)
{
    measurementHidden = false;

    QString type;
    if (!readRequiredString(correctionObject,
                            QStringLiteral("type"),
                            type,
                            context,
                            errorMessage)) {
        return false;
    }

    if (type != QStringLiteral("splCorrection")) {
        setError(errorMessage,
                 QStringLiteral("Unsupported measurement curve type '%1' in '%2'.")
                     .arg(type, context));
        return false;
    }

    bool parsedHidden = false;
    if (formatVersion >= 4 &&
        !readRequiredBool(correctionObject,
                          QStringLiteral("hidden"),
                          parsedHidden,
                          context,
                          errorMessage)) {
        return false;
    }

    const QJsonValue pointsValue = correctionObject.value(QStringLiteral("points"));
    if (!pointsValue.isArray()) {
        setError(errorMessage,
                 QStringLiteral("Missing or invalid array '%1.points'.").arg(context));
        return false;
    }

    KFilterMeasurementCurve parsedCurve;
    const QJsonArray points = pointsValue.toArray();
    for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
        const QJsonValue pointValue = points.at(pointIndex);
        const QString pointContext = QStringLiteral("%1.points[%2]").arg(context).arg(pointIndex);
        if (!pointValue.isObject()) {
            setError(errorMessage, QStringLiteral("Entry '%1' must be an object.").arg(pointContext));
            return false;
        }

        const QJsonObject pointObject = pointValue.toObject();
        double frequencyHz = 0.0;
        double valueDb = 0.0;
        if (!readRequiredDouble(pointObject,
                                QStringLiteral("frequencyHz"),
                                frequencyHz,
                                pointContext,
                                errorMessage) ||
            !readRequiredDouble(pointObject,
                                QStringLiteral("valueDb"),
                                valueDb,
                                pointContext,
                                errorMessage)) {
            return false;
        }

        if (!parsedCurve.appendPoint(frequencyHz, valueDb)) {
            setError(errorMessage,
                     QStringLiteral("Measurement points in '%1.points' must have finite, positive and strictly increasing frequencies.")
                         .arg(context));
            return false;
        }
    }

    curve = parsedCurve;
    measurementHidden = parsedHidden && !curve.isEmpty();
    return true;
}

bool jsonToDriverMeasurements(const QJsonObject& driverObject,
                              KFilterMeasurementCurve& curve,
                              bool& measurementHidden,
                              int formatVersion,
                              const QString& driverContext,
                              QString* errorMessage)
{
    curve.clear();
    measurementHidden = false;
    const QJsonValue measurementsValue = driverObject.value(QStringLiteral("measurements"));
    if (measurementsValue.isUndefined() || measurementsValue.isNull()) {
        return true;
    }
    if (!measurementsValue.isObject()) {
        setError(errorMessage,
                 QStringLiteral("Invalid object '%1.measurements'.").arg(driverContext));
        return false;
    }

    const QJsonObject measurements = measurementsValue.toObject();
    const QJsonValue correctionValue = measurements.value(QStringLiteral("splCorrection"));
    if (correctionValue.isUndefined() || correctionValue.isNull()) {
        return true;
    }
    if (!correctionValue.isObject()) {
        setError(errorMessage,
                 QStringLiteral("Invalid object '%1.measurements.splCorrection'.").arg(driverContext));
        return false;
    }

    return jsonToSplCorrectionCurve(correctionValue.toObject(),
                                    curve,
                                    measurementHidden,
                                    formatVersion,
                                    driverContext + QStringLiteral(".measurements.splCorrection"),
                                    errorMessage);
}

bool jsonToMeasurementSettings(const QJsonObject& project,
                               int formatVersion,
                               bool& mergeMeasurementsEnabled,
                               bool& legacyMeasurementsHidden,
                               QString* errorMessage)
{
    mergeMeasurementsEnabled = false;
    legacyMeasurementsHidden = false;
    const QJsonValue settingsValue = project.value(QStringLiteral("measurementSettings"));
    if (settingsValue.isUndefined() || settingsValue.isNull()) {
        if (formatVersion >= 3) {
            setError(errorMessage,
                     QStringLiteral("Missing object 'root.project.measurementSettings'."));
            return false;
        }
        return true;
    }
    if (!settingsValue.isObject()) {
        setError(errorMessage, QStringLiteral("Invalid object 'root.project.measurementSettings'."));
        return false;
    }

    const QJsonObject settings = settingsValue.toObject();
    if (!readRequiredBool(settings,
                          QStringLiteral("mergeCorrectionCurves"),
                          mergeMeasurementsEnabled,
                          QStringLiteral("root.project.measurementSettings"),
                          errorMessage)) {
        return false;
    }

    if (formatVersion != 3) {
        return true;
    }

    return readRequiredBool(settings,
                            QStringLiteral("hideMeasurements"),
                            legacyMeasurementsHidden,
                            QStringLiteral("root.project.measurementSettings"),
                            errorMessage);
}

bool loadJsonProject(const QByteArray& data,
                     driver (&drivers)[KFilterProjectIo::DriverCount],
                     KFilterProjectIo::MeasurementCurves& splCorrectionCurves,
                     bool& mergeMeasurementsEnabled,
                     KFilterProjectIo::MeasurementHiddenStates& measurementHiddenForDrivers,
                     QString* errorMessage)
{
    mergeMeasurementsEnabled = false;
    measurementHiddenForDrivers.fill(false);

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(errorMessage,
                 QStringLiteral("Cannot parse JSON project at offset %1: %2")
                     .arg(parseError.offset)
                     .arg(parseError.errorString()));
        return false;
    }

    if (!document.isObject()) {
        setError(errorMessage, QStringLiteral("JSON project root must be an object."));
        return false;
    }

    const QJsonObject root = document.object();
    QString format;
    int formatVersion = 0;
    if (!readRequiredString(root,
                            QStringLiteral("format"),
                            format,
                            QStringLiteral("root"),
                            errorMessage) ||
        !readRequiredInt(root,
                         QStringLiteral("formatVersion"),
                         formatVersion,
                         QStringLiteral("root"),
                         errorMessage)) {
        return false;
    }

    if (format != JsonFormatName) {
        setError(errorMessage, QStringLiteral("Unsupported project format '%1'.").arg(format));
        return false;
    }

    if (formatVersion < KFilterProjectIo::LegacyJsonFormatVersion ||
        formatVersion > KFilterProjectIo::JsonFormatVersion) {
        setError(errorMessage,
                 QStringLiteral("Unsupported project format version %1; this build supports versions %2 through %3.")
                     .arg(formatVersion)
                     .arg(KFilterProjectIo::LegacyJsonFormatVersion)
                     .arg(KFilterProjectIo::JsonFormatVersion));
        return false;
    }

    QJsonObject project;
    if (!readObject(root,
                    QStringLiteral("project"),
                    project,
                    QStringLiteral("root"),
                    errorMessage)) {
        return false;
    }

    bool legacyMeasurementsHidden = false;
    if (formatVersion >= 2 &&
        !jsonToMeasurementSettings(project,
                                   formatVersion,
                                   mergeMeasurementsEnabled,
                                   legacyMeasurementsHidden,
                                   errorMessage)) {
        return false;
    }

    const QJsonValue driversValue = project.value(QStringLiteral("drivers"));
    if (!driversValue.isArray()) {
        setError(errorMessage, QStringLiteral("Missing or invalid array 'root.project.drivers'."));
        return false;
    }

    const QJsonArray driverArray = driversValue.toArray();
    if (driverArray.size() != KFilterProjectIo::DriverCount) {
        setError(errorMessage,
                 QStringLiteral("Array 'root.project.drivers' must contain exactly %1 entries.")
                     .arg(KFilterProjectIo::DriverCount));
        return false;
    }

    for (int driverIndex = 0; driverIndex < driverArray.size(); ++driverIndex) {
        const QJsonValue entryValue = driverArray.at(driverIndex);
        const QString driverContext = QStringLiteral("root.project.drivers[%1]").arg(driverIndex);
        if (!entryValue.isObject()) {
            setError(errorMessage, QStringLiteral("Entry '%1' must be an object.").arg(driverContext));
            return false;
        }

        const QJsonObject entry = entryValue.toObject();
        QJsonObject parameters;
        QJsonObject network;
        if (!readObject(entry,
                        QStringLiteral("parameters"),
                        parameters,
                        driverContext,
                        errorMessage) ||
            !readObject(entry,
                        QStringLiteral("network"),
                        network,
                        driverContext,
                        errorMessage) ||
            !jsonToDriverParameters(parameters,
                                    drivers[driverIndex],
                                    driverContext + QStringLiteral(".parameters"),
                                    errorMessage) ||
            !jsonToDriverNetwork(network,
                                 drivers[driverIndex],
                                 driverContext + QStringLiteral(".network"),
                                 errorMessage)) {
            return false;
        }

        if (formatVersion >= 2 &&
            !jsonToDriverMeasurements(
                entry,
                splCorrectionCurves[static_cast<std::size_t>(driverIndex)],
                measurementHiddenForDrivers[static_cast<std::size_t>(driverIndex)],
                formatVersion,
                driverContext,
                errorMessage)) {
            return false;
        }
    }

    if (formatVersion == 3 && legacyMeasurementsHidden) {
        for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
            const std::size_t index = static_cast<std::size_t>(driverIndex);
            measurementHiddenForDrivers[index] = !splCorrectionCurves[index].isEmpty();
        }
    }

    return true;
}

QJsonObject driverParametersToJson(const driver& currentDriver)
{
    QJsonObject parameters;
    parameters.insert(QStringLiteral("title"), currentDriver.GetTitle());
    parameters.insert(QStringLiteral("rdc_ohm"), currentDriver.getRdc());
    parameters.insert(QStringLiteral("lsp_h"), currentDriver.getLsp());
    parameters.insert(QStringLiteral("fs_hz"), currentDriver.getF0());
    parameters.insert(QStringLiteral("qts"), currentDriver.getQtc());
    parameters.insert(QStringLiteral("qes"), currentDriver.getQes());
    parameters.insert(QStringLiteral("qms"), currentDriver.getQms());
    parameters.insert(QStringLiteral("vas_l"), currentDriver.getVas());
    parameters.insert(QStringLiteral("diameter_cm"), currentDriver.getDm());
    parameters.insert(QStringLiteral("vb_l"), currentDriver.Vb);
    parameters.insert(QStringLiteral("ql"), currentDriver.getQl());
    parameters.insert(QStringLiteral("fb_hz"), currentDriver.Fb);
    parameters.insert(QStringLiteral("v2_l"), currentDriver.V2);
    parameters.insert(QStringLiteral("enclosureTypeProposal"), currentDriver.GTypProposal);
    parameters.insert(QStringLiteral("gainLinear"), currentDriver.gain);
    parameters.insert(QStringLiteral("pressureActive"), currentDriver.PressureisActive);
    parameters.insert(QStringLiteral("impedanceActive"), currentDriver.ImpedanzisActive);
    parameters.insert(QStringLiteral("summaryActive"), currentDriver.SummaryisActive);
    parameters.insert(QStringLiteral("scalarSummaryActive"), currentDriver.ScalarSummaryisActive);
    parameters.insert(QStringLiteral("impedanceSummaryActive"), currentDriver.ImpedanzSummaryisActive);
    parameters.insert(QStringLiteral("invertPhase"), currentDriver.InvertPhase);
    parameters.insert(QStringLiteral("fullCircuit"), currentDriver.getFullCircuit());
    return parameters;
}

QJsonObject driverNetworkToJson(const driver& currentDriver)
{
    QJsonArray values;
    for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
        values.append(currentDriver.getUnit(unitIndex));
    }

    QJsonObject network;
    network.insert(QStringLiteral("unitBaseIndex"), 1);
    network.insert(QStringLiteral("values"), values);
    return network;
}

QJsonObject splCorrectionCurveToJson(const KFilterMeasurementCurve& curve, bool measurementHidden)
{
    QJsonArray points;
    for (const KFilterMeasurementPoint& point : curve.points()) {
        QJsonObject pointObject;
        pointObject.insert(QStringLiteral("frequencyHz"), point.frequencyHz);
        pointObject.insert(QStringLiteral("valueDb"), point.value);
        points.append(pointObject);
    }

    QJsonObject correction;
    correction.insert(QStringLiteral("type"), QStringLiteral("splCorrection"));
    correction.insert(QStringLiteral("hidden"), measurementHidden);
    correction.insert(QStringLiteral("points"), points);
    return correction;
}

QJsonObject driverMeasurementsToJson(const KFilterMeasurementCurve& curve, bool measurementHidden)
{
    QJsonObject measurements;
    if (!curve.isEmpty()) {
        measurements.insert(QStringLiteral("splCorrection"),
                            splCorrectionCurveToJson(curve, measurementHidden));
    }
    return measurements;
}

bool validateMeasurementCurve(const KFilterMeasurementCurve& curve,
                              int driverIndex,
                              QString* errorMessage)
{
    double previousFrequencyHz = 0.0;
    bool havePreviousFrequency = false;
    for (qsizetype pointIndex = 0; pointIndex < curve.points().size(); ++pointIndex) {
        const KFilterMeasurementPoint& point = curve.points().at(pointIndex);
        if (!std::isfinite(point.frequencyHz) || point.frequencyHz <= 0.0 ||
            !std::isfinite(point.value)) {
            setError(errorMessage,
                     QStringLiteral("Driver %1 measurement point %2 contains an invalid frequency or dB value.")
                         .arg(driverIndex + 1)
                         .arg(pointIndex + 1));
            return false;
        }
        if (havePreviousFrequency && point.frequencyHz <= previousFrequencyHz) {
            setError(errorMessage,
                     QStringLiteral("Driver %1 measurement frequencies must be strictly increasing.")
                         .arg(driverIndex + 1));
            return false;
        }

        previousFrequencyHz = point.frequencyHz;
        havePreviousFrequency = true;
    }

    return true;
}

bool validateFiniteDriverData(const driver& currentDriver,
                              int driverIndex,
                              QString* errorMessage)
{
    const double scalarValues[] = {
        currentDriver.getRdc(),
        currentDriver.getLsp(),
        currentDriver.getF0(),
        currentDriver.getQtc(),
        currentDriver.getQes(),
        currentDriver.getQms(),
        currentDriver.getVas(),
        currentDriver.getDm(),
        currentDriver.Vb,
        currentDriver.getQl(),
        currentDriver.Fb,
        currentDriver.V2,
        currentDriver.gain
    };

    for (double value : scalarValues) {
        if (!std::isfinite(value)) {
            setError(errorMessage,
                     QStringLiteral("Driver %1 contains a non-finite parameter value.")
                         .arg(driverIndex + 1));
            return false;
        }
    }

    if (currentDriver.getQl() <= 0.0) {
        setError(errorMessage,
                 QStringLiteral("Driver %1 has an invalid Ql value; Ql must be greater than zero.")
                     .arg(driverIndex + 1));
        return false;
    }

    for (int unitIndex = 1; unitIndex <= KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
        if (!std::isfinite(currentDriver.getUnit(unitIndex))) {
            setError(errorMessage,
                     QStringLiteral("Driver %1 contains a non-finite network value at unit %2.")
                         .arg(driverIndex + 1)
                         .arg(unitIndex));
            return false;
        }
    }

    return true;
}

qsizetype firstSignificantByte(const QByteArray& data)
{
    qsizetype index = 0;
    if (data.startsWith("\xEF\xBB\xBF")) {
        index = 3;
    }

    while (index < data.size()) {
        const char character = data.at(index);
        if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
            return index;
        }
        ++index;
    }

    return -1;
}

void finalizeDrivers(driver (&drivers)[KFilterProjectIo::DriverCount])
{
    for (driver& currentDriver : drivers) {
        currentDriver.Berechneparameter();
        currentDriver.setmodified();
    }
}
}

bool KFilterProjectIo::loadFromFile(const QString& filePath,
                                    driver (&drivers)[DriverCount],
                                    MeasurementCurves& splCorrectionCurves,
                                    bool& mergeMeasurementsEnabled,
                                    MeasurementHiddenStates& measurementHiddenForDrivers,
                                    QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("Cannot open project file '%1' for reading: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    const QByteArray data = file.readAll();
    const qsizetype firstByteIndex = firstSignificantByte(data);
    if (firstByteIndex < 0) {
        setError(errorMessage, QStringLiteral("Project file '%1' is empty.").arg(filePath));
        return false;
    }

    driver parsedDrivers[DriverCount];
    MeasurementCurves parsedSplCorrectionCurves;
    bool parsedMergeMeasurementsEnabled = false;
    MeasurementHiddenStates parsedMeasurementHiddenForDrivers{};
    const char firstByte = data.at(firstByteIndex);
    const bool isJson = firstByte == '{' || firstByte == '[';
    const QByteArray significantData = data.mid(firstByteIndex);
    const bool loaded = isJson
        ? loadJsonProject(significantData,
                          parsedDrivers,
                          parsedSplCorrectionCurves,
                          parsedMergeMeasurementsEnabled,
                          parsedMeasurementHiddenForDrivers,
                          errorMessage)
        : loadLegacyProject(significantData, parsedDrivers, errorMessage);
    if (!loaded) {
        return false;
    }

    finalizeDrivers(parsedDrivers);
    for (int driverIndex = 0; driverIndex < DriverCount; ++driverIndex) {
        const std::size_t index = static_cast<std::size_t>(driverIndex);
        drivers[driverIndex] = parsedDrivers[driverIndex];
        splCorrectionCurves[index] = parsedSplCorrectionCurves[index];
        measurementHiddenForDrivers[index] =
            parsedMeasurementHiddenForDrivers[index] && !splCorrectionCurves[index].isEmpty();
    }
    mergeMeasurementsEnabled = parsedMergeMeasurementsEnabled &&
        std::any_of(splCorrectionCurves.cbegin(),
                    splCorrectionCurves.cend(),
                    [](const KFilterMeasurementCurve& curve) { return curve.size() >= 2; });

    return true;
}

bool KFilterProjectIo::saveToFile(const QString& filePath,
                                  driver (&drivers)[DriverCount],
                                  const MeasurementCurves& splCorrectionCurves,
                                  bool mergeMeasurementsEnabled,
                                  const MeasurementHiddenStates& measurementHiddenForDrivers,
                                  QString* errorMessage)
{
    QJsonArray driverArray;
    for (int driverIndex = 0; driverIndex < DriverCount; ++driverIndex) {
        const driver& currentDriver = drivers[driverIndex];
        const KFilterMeasurementCurve& correctionCurve =
            splCorrectionCurves[static_cast<std::size_t>(driverIndex)];
        if (!validateFiniteDriverData(currentDriver, driverIndex, errorMessage) ||
            !validateMeasurementCurve(correctionCurve, driverIndex, errorMessage)) {
            return false;
        }

        QJsonObject driverObject;
        driverObject.insert(QStringLiteral("parameters"), driverParametersToJson(currentDriver));
        driverObject.insert(QStringLiteral("network"), driverNetworkToJson(currentDriver));
        driverObject.insert(
            QStringLiteral("measurements"),
            driverMeasurementsToJson(
                correctionCurve,
                measurementHiddenForDrivers[static_cast<std::size_t>(driverIndex)] &&
                    !correctionCurve.isEmpty()));
        driverArray.append(driverObject);
    }

    QJsonObject measurementSettings;
    measurementSettings.insert(QStringLiteral("mergeCorrectionCurves"),
                               mergeMeasurementsEnabled &&
                                   std::any_of(splCorrectionCurves.cbegin(),
                                               splCorrectionCurves.cend(),
                                               [](const KFilterMeasurementCurve& curve) {
                                                   return curve.size() >= 2;
                                               }));
    QJsonObject project;
    project.insert(QStringLiteral("drivers"), driverArray);
    project.insert(QStringLiteral("measurementSettings"), measurementSettings);

    QJsonObject root;
    root.insert(QStringLiteral("format"), JsonFormatName);
    root.insert(QStringLiteral("formatVersion"), JsonFormatVersion);
    root.insert(QStringLiteral("application"), ApplicationName);
    root.insert(QStringLiteral("project"), project);

    const QByteArray output = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(errorMessage,
                 QStringLiteral("Cannot open project file '%1' for writing: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    if (file.write(output) != static_cast<qint64>(output.size())) {
        setError(errorMessage,
                 QStringLiteral("Cannot write project file '%1': %2")
                     .arg(filePath, file.errorString()));
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        setError(errorMessage,
                 QStringLiteral("Cannot finalize project file '%1': %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    return true;
}
