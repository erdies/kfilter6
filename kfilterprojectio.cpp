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
        int enclosureTypeProposalValue = 0;
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
        READ_INT_FIELD("GTypProposal", enclosureTypeProposalValue);
        currentDriver.enclosureTypeProposal = static_cast<EnclosureType>(enclosureTypeProposalValue);
        READ_DOUBLE_FIELD("Gain", currentDriver.gain = doubleValue);
        READ_BOOL_FIELD("Pressure", currentDriver.pressureIsActive);
        READ_BOOL_FIELD("Impedanz", currentDriver.impedanceIsActive);
        READ_BOOL_FIELD("Summary", currentDriver.summaryIsActive);
        READ_BOOL_FIELD("ScalarSummary", currentDriver.ScalarSummaryisActive);
        READ_BOOL_FIELD("ImpedanzSummary", currentDriver.ImpedanceSummaryisActive);
        READ_BOOL_FIELD("InvertPhase", currentDriver.InvertPhase);

        if (!readRequiredDataLine(lines, index, line, QStringLiteral("Title"), driverNumber, errorMessage)) {
            return false;
        }
        currentDriver.setTitle(valueAfterEquals(line));

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

QString baffleModelToString(BaffleModel model)
{
    switch (model) {
    case BaffleModel::SimpleBaffleStep:
        return QStringLiteral("simpleBaffleStep");
    case BaffleModel::RectangularEdgeDiffraction:
        return QStringLiteral("rectangularEdgeDiffraction");
    }

    return {};
}

bool baffleModelFromString(const QString& value, BaffleModel& model)
{
    if (value == QStringLiteral("simpleBaffleStep")) {
        model = BaffleModel::SimpleBaffleStep;
    } else if (value == QStringLiteral("rectangularEdgeDiffraction")) {
        model = BaffleModel::RectangularEdgeDiffraction;
    } else {
        return false;
    }

    return true;
}

QString baffleSideEdgeTreatmentToString(BaffleSideEdgeTreatment treatment)
{
    switch (treatment) {
    case BaffleSideEdgeTreatment::Sharp:
        return QStringLiteral("sharp");
    case BaffleSideEdgeTreatment::Chamfer45:
        return QStringLiteral("chamfer45");
    }

    return {};
}

bool baffleSideEdgeTreatmentFromString(const QString& value,
                                       BaffleSideEdgeTreatment& treatment)
{
    if (value == QStringLiteral("sharp")) {
        treatment = BaffleSideEdgeTreatment::Sharp;
    } else if (value == QStringLiteral("chamfer45")) {
        treatment = BaffleSideEdgeTreatment::Chamfer45;
    } else {
        return false;
    }
    return true;
}

QString baffleBoundaryConditionToString(BaffleBoundaryCondition condition)
{
    switch (condition) {
    case BaffleBoundaryCondition::FreeField:
        return QStringLiteral("freeField");
    case BaffleBoundaryCondition::RigidFloorContactDiffractionOnly:
        return QStringLiteral("rigidFloorContactDiffractionOnly");
    }

    return {};
}

bool baffleBoundaryConditionFromString(const QString& value,
                                       BaffleBoundaryCondition& condition)
{
    if (value == QStringLiteral("freeField")) {
        condition = BaffleBoundaryCondition::FreeField;
    } else if (value == QStringLiteral("rigidFloorContactDiffractionOnly")) {
        condition = BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    } else {
        return false;
    }
    return true;
}

QString floorSurfacePresetToString(FloorSurfacePreset preset)
{
    switch (preset) {
    case FloorSurfacePreset::HardRigid:
        return QStringLiteral("hardRigid");
    case FloorSurfacePreset::MikiReference10mm100k:
        return QStringLiteral("mikiReference10mm100k");
    }

    return {};
}

bool floorSurfacePresetFromString(const QString& value, FloorSurfacePreset& preset)
{
    if (value == QStringLiteral("hardRigid")) {
        preset = FloorSurfacePreset::HardRigid;
        return true;
    }
    if (value == QStringLiteral("mikiReference10mm100k")) {
        preset = FloorSurfacePreset::MikiReference10mm100k;
        return true;
    }
    return false;
}

bool jsonToBaffleSettings(const QJsonObject& driverObject,
                          BaffleSettings& settings,
                          int formatVersion,
                          const QString& driverContext,
                          QString* errorMessage)
{
    QJsonObject baffleObject;
    if (!readObject(driverObject,
                    QStringLiteral("baffle"),
                    baffleObject,
                    driverContext,
                    errorMessage)) {
        return false;
    }

    const QString context = driverContext + QStringLiteral(".baffle");
    BaffleSettings parsed;
    QString modelString;
    int edgeSourceCount = 0;
    if (!readRequiredBool(baffleObject, QStringLiteral("enabled"), parsed.enabled, context, errorMessage) ||
        !readRequiredString(baffleObject, QStringLiteral("model"), modelString, context, errorMessage) ||
        !readRequiredDouble(baffleObject, QStringLiteral("widthMm"), parsed.widthMm, context, errorMessage) ||
        !readRequiredDouble(baffleObject, QStringLiteral("heightMm"), parsed.heightMm, context, errorMessage) ||
        !readRequiredDouble(baffleObject, QStringLiteral("driverXmm"), parsed.driverXmm, context, errorMessage) ||
        !readRequiredDouble(baffleObject, QStringLiteral("driverYmm"), parsed.driverYmm, context, errorMessage) ||
        !readRequiredBool(baffleObject, QStringLiteral("showResponseInPlot"), parsed.showResponseInPlot, context, errorMessage) ||
        !readRequiredInt(baffleObject, QStringLiteral("edgeSourceCount"), edgeSourceCount, context, errorMessage)) {
        return false;
    }

    if (!baffleModelFromString(modelString, parsed.model)) {
        setError(errorMessage,
                 QStringLiteral("Unsupported baffle model '%1' in '%2.model'.")
                     .arg(modelString, context));
        return false;
    }
    if (edgeSourceCount <= 0 || edgeSourceCount > 100000) {
        setError(errorMessage,
                 QStringLiteral("Field '%1.edgeSourceCount' must be between 1 and 100000.")
                     .arg(context));
        return false;
    }
    parsed.edgeSourceCount = static_cast<std::size_t>(edgeSourceCount);

    if (formatVersion >= 8) {
        QString boundaryString;
        if (!readRequiredString(baffleObject, QStringLiteral("boundaryCondition"),
                                boundaryString, context, errorMessage)) {
            return false;
        }
        if (!baffleBoundaryConditionFromString(boundaryString, parsed.boundaryCondition)) {
            setError(errorMessage,
                     QStringLiteral("Unsupported baffle boundary condition '%1' in '%2.boundaryCondition'.")
                         .arg(boundaryString, context));
            return false;
        }
    }

    if (formatVersion >= 7) {
        QString leftTreatmentString;
        QString rightTreatmentString;
        if (!readRequiredString(baffleObject, QStringLiteral("leftEdgeTreatment"),
                                leftTreatmentString, context, errorMessage) ||
            !readRequiredDouble(baffleObject, QStringLiteral("leftChamferSetbackMm"),
                                parsed.leftChamferSetbackMm, context, errorMessage) ||
            !readRequiredString(baffleObject, QStringLiteral("rightEdgeTreatment"),
                                rightTreatmentString, context, errorMessage) ||
            !readRequiredDouble(baffleObject, QStringLiteral("rightChamferSetbackMm"),
                                parsed.rightChamferSetbackMm, context, errorMessage)) {
            return false;
        }
        if (!baffleSideEdgeTreatmentFromString(leftTreatmentString, parsed.leftEdgeTreatment)) {
            setError(errorMessage,
                     QStringLiteral("Unsupported left edge treatment '%1' in '%2.leftEdgeTreatment'.")
                         .arg(leftTreatmentString, context));
            return false;
        }
        if (!baffleSideEdgeTreatmentFromString(rightTreatmentString, parsed.rightEdgeTreatment)) {
            setError(errorMessage,
                     QStringLiteral("Unsupported right edge treatment '%1' in '%2.rightEdgeTreatment'.")
                         .arg(rightTreatmentString, context));
            return false;
        }
    }

    if (parsed.widthMm <= 0.0) {
        setError(errorMessage, QStringLiteral("Field '%1.widthMm' must be greater than zero.").arg(context));
        return false;
    }
    if (!std::isfinite(parsed.leftChamferSetbackMm) ||
        !std::isfinite(parsed.rightChamferSetbackMm) ||
        parsed.leftChamferSetbackMm < 0.0 ||
        parsed.rightChamferSetbackMm < 0.0 ||
        (parsed.leftEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45 &&
         parsed.leftChamferSetbackMm < 5.0) ||
        (parsed.rightEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45 &&
         parsed.rightChamferSetbackMm < 5.0)) {
        setError(errorMessage,
                 QStringLiteral("Chamfer metadata in '%1' is invalid; active 45-degree chamfers must be at least 5 mm and setbacks must be finite/non-negative.")
                     .arg(context));
        return false;
    }
    if (parsed.model == BaffleModel::RectangularEdgeDiffraction) {
        if (parsed.boundaryCondition == BaffleBoundaryCondition::RigidFloorContactDiffractionOnly &&
            (parsed.leftEdgeTreatment != BaffleSideEdgeTreatment::Sharp ||
             parsed.rightEdgeTreatment != BaffleSideEdgeTreatment::Sharp)) {
            setError(errorMessage,
                     QStringLiteral("Rigid floor contact in '%1' currently supports Sharp side edges only.")
                         .arg(context));
            return false;
        }
        const double leftSetback = parsed.leftEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45
                                       ? parsed.leftChamferSetbackMm
                                       : 0.0;
        const double rightSetback = parsed.rightEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45
                                        ? parsed.rightChamferSetbackMm
                                        : 0.0;
        if (parsed.heightMm <= 0.0 ||
            parsed.driverXmm <= 0.0 || parsed.driverXmm >= parsed.widthMm ||
            parsed.driverYmm <= 0.0 || parsed.driverYmm >= parsed.heightMm ||
            parsed.edgeSourceCount < 4 ||
            leftSetback + rightSetback >= parsed.widthMm ||
            parsed.driverXmm <= leftSetback ||
            parsed.driverXmm >= parsed.widthMm - rightSetback) {
            setError(errorMessage,
                     QStringLiteral("Rectangular geometry in '%1' is invalid; the driver centre must lie on the flat front surface, active 45-degree chamfers must be at least 5 mm, opposing chamfers must leave front width, and edgeSourceCount must be at least 4.")
                         .arg(context));
            return false;
        }
    }

    settings = parsed;
    return true;
}

QString activeFilterTypeToString(ActiveFilterType type)
{
    switch (type) {
    case ActiveFilterType::LowPass:
        return QStringLiteral("lowPass");
    case ActiveFilterType::HighPass:
        return QStringLiteral("highPass");
    case ActiveFilterType::BandPass:
        return QStringLiteral("bandPass");
    case ActiveFilterType::Notch:
        return QStringLiteral("notch");
    case ActiveFilterType::AllPass:
        return QStringLiteral("allPass");
    case ActiveFilterType::Gain:
        return QStringLiteral("gain");
    case ActiveFilterType::Delay:
        return QStringLiteral("delay");
    case ActiveFilterType::Polarity:
        return QStringLiteral("polarity");
    case ActiveFilterType::PeakingEq:
        return QStringLiteral("peakingEq");
    case ActiveFilterType::LowShelf:
        return QStringLiteral("lowShelf");
    case ActiveFilterType::HighShelf:
        return QStringLiteral("highShelf");
    }

    return {};
}

bool activeFilterTypeFromString(const QString& value, ActiveFilterType& type)
{
    if (value == QStringLiteral("lowPass")) {
        type = ActiveFilterType::LowPass;
    } else if (value == QStringLiteral("highPass")) {
        type = ActiveFilterType::HighPass;
    } else if (value == QStringLiteral("bandPass")) {
        type = ActiveFilterType::BandPass;
    } else if (value == QStringLiteral("notch")) {
        type = ActiveFilterType::Notch;
    } else if (value == QStringLiteral("allPass")) {
        type = ActiveFilterType::AllPass;
    } else if (value == QStringLiteral("gain")) {
        type = ActiveFilterType::Gain;
    } else if (value == QStringLiteral("delay")) {
        type = ActiveFilterType::Delay;
    } else if (value == QStringLiteral("polarity")) {
        type = ActiveFilterType::Polarity;
    } else if (value == QStringLiteral("peakingEq")) {
        type = ActiveFilterType::PeakingEq;
    } else if (value == QStringLiteral("lowShelf")) {
        type = ActiveFilterType::LowShelf;
    } else if (value == QStringLiteral("highShelf")) {
        type = ActiveFilterType::HighShelf;
    } else {
        return false;
    }

    return true;
}

QString activeFilterCharacteristicToString(ActiveFilterCharacteristic characteristic)
{
    switch (characteristic) {
    case ActiveFilterCharacteristic::Butterworth:
        return QStringLiteral("butterworth");
    case ActiveFilterCharacteristic::Bessel:
        return QStringLiteral("bessel");
    case ActiveFilterCharacteristic::LinkwitzRiley:
        return QStringLiteral("linkwitzRiley");
    case ActiveFilterCharacteristic::GenericQ:
        return QStringLiteral("genericQ");
    }

    return {};
}

bool activeFilterCharacteristicFromString(const QString& value,
                                          ActiveFilterCharacteristic& characteristic)
{
    if (value == QStringLiteral("butterworth")) {
        characteristic = ActiveFilterCharacteristic::Butterworth;
    } else if (value == QStringLiteral("bessel")) {
        characteristic = ActiveFilterCharacteristic::Bessel;
    } else if (value == QStringLiteral("linkwitzRiley")) {
        characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
    } else if (value == QStringLiteral("genericQ")) {
        characteristic = ActiveFilterCharacteristic::GenericQ;
    } else {
        return false;
    }

    return true;
}

bool readActiveFilterCharacteristic(const QJsonObject& parameters,
                                    ActiveFilterCharacteristic& characteristic,
                                    const QString& context,
                                    QString* errorMessage)
{
    QString value;
    if (!readRequiredString(parameters,
                            QStringLiteral("characteristic"),
                            value,
                            context,
                            errorMessage)) {
        return false;
    }

    if (!activeFilterCharacteristicFromString(value, characteristic)) {
        setError(errorMessage,
                 QStringLiteral("Unsupported active-filter characteristic '%1' in '%2.characteristic'.")
                     .arg(value, context));
        return false;
    }

    return true;
}

bool jsonToActiveFilterParameters(const QJsonObject& parameters,
                                  ActiveFilterType type,
                                  ActiveFilterParameters& target,
                                  const QString& context,
                                  QString* errorMessage)
{
    switch (type) {
    case ActiveFilterType::LowPass: {
        ActiveFilterLowPassParameters parsed;
        if (!readActiveFilterCharacteristic(parameters, parsed.characteristic, context, errorMessage) ||
            !readRequiredInt(parameters, QStringLiteral("order"), parsed.order, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("frequencyHz"),
                                parsed.frequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::HighPass: {
        ActiveFilterHighPassParameters parsed;
        if (!readActiveFilterCharacteristic(parameters, parsed.characteristic, context, errorMessage) ||
            !readRequiredInt(parameters, QStringLiteral("order"), parsed.order, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("frequencyHz"),
                                parsed.frequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::BandPass: {
        ActiveFilterBandPassParameters parsed;
        if (!readActiveFilterCharacteristic(parameters, parsed.characteristic, context, errorMessage) ||
            !readRequiredInt(parameters, QStringLiteral("order"), parsed.order, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("lowerFrequencyHz"),
                                parsed.lowerFrequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("upperFrequencyHz"),
                                parsed.upperFrequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::Notch: {
        ActiveFilterNotchParameters parsed;
        if (!readRequiredDouble(parameters,
                                QStringLiteral("centerFrequencyHz"),
                                parsed.centerFrequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("gainDb"),
                                parsed.gainDb,
                                context,
                                errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::PeakingEq: {
        ActiveFilterPeakingEqParameters parsed;
        if (!readRequiredDouble(parameters,
                                QStringLiteral("centerFrequencyHz"),
                                parsed.centerFrequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("gainDb"),
                                parsed.gainDb,
                                context,
                                errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::LowShelf: {
        ActiveFilterLowShelfParameters parsed;
        if (!readRequiredDouble(parameters,
                                QStringLiteral("transitionFrequencyHz"),
                                parsed.transitionFrequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("gainDb"),
                                parsed.gainDb,
                                context,
                                errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::HighShelf: {
        ActiveFilterHighShelfParameters parsed;
        if (!readRequiredDouble(parameters,
                                QStringLiteral("transitionFrequencyHz"),
                                parsed.transitionFrequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("gainDb"),
                                parsed.gainDb,
                                context,
                                errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::AllPass: {
        ActiveFilterAllPassParameters parsed;
        if (!readRequiredInt(parameters, QStringLiteral("order"), parsed.order, context, errorMessage) ||
            !readRequiredDouble(parameters,
                                QStringLiteral("frequencyHz"),
                                parsed.frequencyHz,
                                context,
                                errorMessage) ||
            !readRequiredDouble(parameters, QStringLiteral("q"), parsed.q, context, errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::Gain: {
        ActiveFilterGainParameters parsed;
        if (!readRequiredDouble(parameters,
                                QStringLiteral("gainDb"),
                                parsed.gainDb,
                                context,
                                errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::Delay: {
        ActiveFilterDelayParameters parsed;
        if (!readRequiredDouble(parameters,
                                QStringLiteral("delayMs"),
                                parsed.delayMs,
                                context,
                                errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    case ActiveFilterType::Polarity: {
        ActiveFilterPolarityParameters parsed;
        if (!readRequiredBool(parameters,
                              QStringLiteral("inverted"),
                              parsed.inverted,
                              context,
                              errorMessage)) {
            return false;
        }
        target = parsed;
        return true;
    }
    }

    return false;
}

bool jsonToActiveFilterChain(const QJsonObject& driverObject,
                             ActiveFilterChain& chain,
                             const QString& driverContext,
                             QString* errorMessage)
{
    QJsonObject activeFilterObject;
    if (!readObject(driverObject,
                    QStringLiteral("activeFilter"),
                    activeFilterObject,
                    driverContext,
                    errorMessage)) {
        return false;
    }

    bool enabled = false;
    bool showResponseInPlot = false;
    const QString activeFilterContext = driverContext + QStringLiteral(".activeFilter");
    if (!readRequiredBool(activeFilterObject,
                          QStringLiteral("enabled"),
                          enabled,
                          activeFilterContext,
                          errorMessage) ||
        !readRequiredBool(activeFilterObject,
                          QStringLiteral("showResponseInPlot"),
                          showResponseInPlot,
                          activeFilterContext,
                          errorMessage)) {
        return false;
    }

    const QJsonValue sectionsValue = activeFilterObject.value(QStringLiteral("sections"));
    if (!sectionsValue.isArray()) {
        setError(errorMessage,
                 QStringLiteral("Missing or invalid array '%1.sections'.").arg(activeFilterContext));
        return false;
    }

    ActiveFilterChain parsedChain;
    parsedChain.setEnabled(enabled);
    parsedChain.setShowResponseInPlot(showResponseInPlot);

    const QJsonArray sections = sectionsValue.toArray();
    for (int sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
        const QJsonValue sectionValue = sections.at(sectionIndex);
        const QString sectionContext =
            QStringLiteral("%1.sections[%2]").arg(activeFilterContext).arg(sectionIndex);
        if (!sectionValue.isObject()) {
            setError(errorMessage, QStringLiteral("Entry '%1' must be an object.").arg(sectionContext));
            return false;
        }

        const QJsonObject sectionObject = sectionValue.toObject();
        bool sectionEnabled = true;
        QString typeString;
        QJsonObject parameters;
        if (!readRequiredBool(sectionObject,
                              QStringLiteral("enabled"),
                              sectionEnabled,
                              sectionContext,
                              errorMessage) ||
            !readRequiredString(sectionObject,
                                QStringLiteral("type"),
                                typeString,
                                sectionContext,
                                errorMessage) ||
            !readObject(sectionObject,
                        QStringLiteral("parameters"),
                        parameters,
                        sectionContext,
                        errorMessage)) {
            return false;
        }

        ActiveFilterType type = ActiveFilterType::LowPass;
        if (!activeFilterTypeFromString(typeString, type)) {
            setError(errorMessage,
                     QStringLiteral("Unsupported active-filter type '%1' in '%2.type'.")
                         .arg(typeString, sectionContext));
            return false;
        }

        ActiveFilterSection section(type);
        section.setEnabled(sectionEnabled);
        if (!jsonToActiveFilterParameters(parameters,
                                          type,
                                          section.parameters(),
                                          sectionContext + QStringLiteral(".parameters"),
                                          errorMessage)) {
            return false;
        }

        const std::size_t insertedIndex = parsedChain.addSection(type);
        parsedChain.section(insertedIndex) = section;
    }

    chain = parsedChain;
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

    currentDriver.setTitle(title);
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
    currentDriver.enclosureTypeProposal = static_cast<EnclosureType>(enclosureTypeProposal);
    currentDriver.gain = gain;
    currentDriver.pressureIsActive = pressureActive;
    currentDriver.impedanceIsActive = impedanceActive;
    currentDriver.summaryIsActive = summaryActive;
    currentDriver.ScalarSummaryisActive = scalarSummaryActive;
    currentDriver.ImpedanceSummaryisActive = impedanceSummaryActive;
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

bool jsonToFloorReflectionSettings(const QJsonObject& driverObject,
                                   FloorReflectionSettings& settings,
                                   const QString& driverContext,
                                   QString* errorMessage);

bool loadJsonProject(const QByteArray& data,
                     driver (&drivers)[KFilterProjectIo::DriverCount],
                     KFilterProjectIo::MeasurementCurves& splCorrectionCurves,
                     bool& mergeMeasurementsEnabled,
                     KFilterProjectIo::MeasurementHiddenStates& measurementHiddenForDrivers,
                     KFilterProjectIo::ActiveFilterChains& activeFilterChains,
                     KFilterProjectIo::BaffleSettingsPerDriver& baffleSettings,
                     KFilterProjectIo::FloorReflectionSettingsPerDriver& floorReflectionSettings,
                     QString* errorMessage)
{
    mergeMeasurementsEnabled = false;
    measurementHiddenForDrivers.fill(false);
    activeFilterChains = KFilterProjectIo::ActiveFilterChains{};
    baffleSettings = KFilterProjectIo::BaffleSettingsPerDriver{};
    floorReflectionSettings = KFilterProjectIo::FloorReflectionSettingsPerDriver{};

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

        if (formatVersion >= 5 &&
            !jsonToActiveFilterChain(
                entry,
                activeFilterChains[static_cast<std::size_t>(driverIndex)],
                driverContext,
                errorMessage)) {
            return false;
        }

        if (formatVersion >= 6 &&
            !jsonToBaffleSettings(
                entry,
                baffleSettings[static_cast<std::size_t>(driverIndex)],
                formatVersion,
                driverContext,
                errorMessage)) {
            return false;
        }

        if (formatVersion >= 9 &&
            !jsonToFloorReflectionSettings(
                entry,
                floorReflectionSettings[static_cast<std::size_t>(driverIndex)],
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

bool jsonToFloorReflectionSettings(const QJsonObject& driverObject,
                                   FloorReflectionSettings& settings,
                                   const QString& driverContext,
                                   QString* errorMessage)
{
    QJsonObject floorObject;
    if (!readObject(driverObject,
                    QStringLiteral("floorReflection"),
                    floorObject,
                    driverContext,
                    errorMessage)) {
        return false;
    }

    const QString context = driverContext + QStringLiteral(".floorReflection");
    FloorReflectionSettings parsed;
    QString surfaceString;
    if (!readRequiredBool(floorObject, QStringLiteral("enabled"), parsed.enabled, context, errorMessage) ||
        !readRequiredDouble(floorObject, QStringLiteral("cabinetBottomAboveFloorMm"),
                            parsed.cabinetBottomAboveFloorMm, context, errorMessage) ||
        !readRequiredDouble(floorObject, QStringLiteral("listenerHeightAboveFloorMm"),
                            parsed.listenerHeightAboveFloorMm, context, errorMessage) ||
        !readRequiredDouble(floorObject, QStringLiteral("horizontalDistanceMm"),
                            parsed.horizontalDistanceMm, context, errorMessage) ||
        !readRequiredString(floorObject, QStringLiteral("surfacePreset"),
                            surfaceString, context, errorMessage)) {
        return false;
    }

    if (!floorSurfacePresetFromString(surfaceString, parsed.surfacePreset)) {
        setError(errorMessage,
                 QStringLiteral("Unsupported floor surface preset '%1' in '%2.surfacePreset'.")
                     .arg(surfaceString, context));
        return false;
    }

    if (!std::isfinite(parsed.cabinetBottomAboveFloorMm) ||
        !std::isfinite(parsed.listenerHeightAboveFloorMm) ||
        !std::isfinite(parsed.horizontalDistanceMm) ||
        parsed.cabinetBottomAboveFloorMm < 0.0 ||
        parsed.listenerHeightAboveFloorMm < 0.0 ||
        parsed.horizontalDistanceMm < 0.0) {
        setError(errorMessage,
                 QStringLiteral("Floor-reflection placement fields in '%1' must be finite and non-negative.")
                     .arg(context));
        return false;
    }

    settings = parsed;
    return true;
}

QJsonObject driverParametersToJson(const driver& currentDriver)
{
    QJsonObject parameters;
    parameters.insert(QStringLiteral("title"), currentDriver.getTitle());
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
    parameters.insert(QStringLiteral("enclosureTypeProposal"), static_cast<int>(currentDriver.enclosureTypeProposal));
    parameters.insert(QStringLiteral("gainLinear"), currentDriver.gain);
    parameters.insert(QStringLiteral("pressureActive"), currentDriver.pressureIsActive);
    parameters.insert(QStringLiteral("impedanceActive"), currentDriver.impedanceIsActive);
    parameters.insert(QStringLiteral("summaryActive"), currentDriver.summaryIsActive);
    parameters.insert(QStringLiteral("scalarSummaryActive"), currentDriver.ScalarSummaryisActive);
    parameters.insert(QStringLiteral("impedanceSummaryActive"), currentDriver.ImpedanceSummaryisActive);
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

QJsonObject activeFilterParametersToJson(const ActiveFilterSection& section)
{
    QJsonObject parameters;
    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        const auto& value = std::get<ActiveFilterLowPassParameters>(section.parameters());
        parameters.insert(QStringLiteral("characteristic"),
                          activeFilterCharacteristicToString(value.characteristic));
        parameters.insert(QStringLiteral("order"), value.order);
        parameters.insert(QStringLiteral("frequencyHz"), value.frequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        break;
    }
    case ActiveFilterType::HighPass: {
        const auto& value = std::get<ActiveFilterHighPassParameters>(section.parameters());
        parameters.insert(QStringLiteral("characteristic"),
                          activeFilterCharacteristicToString(value.characteristic));
        parameters.insert(QStringLiteral("order"), value.order);
        parameters.insert(QStringLiteral("frequencyHz"), value.frequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        break;
    }
    case ActiveFilterType::BandPass: {
        const auto& value = std::get<ActiveFilterBandPassParameters>(section.parameters());
        parameters.insert(QStringLiteral("characteristic"),
                          activeFilterCharacteristicToString(value.characteristic));
        parameters.insert(QStringLiteral("order"), value.order);
        parameters.insert(QStringLiteral("lowerFrequencyHz"), value.lowerFrequencyHz);
        parameters.insert(QStringLiteral("upperFrequencyHz"), value.upperFrequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        break;
    }
    case ActiveFilterType::Notch: {
        const auto& value = std::get<ActiveFilterNotchParameters>(section.parameters());
        parameters.insert(QStringLiteral("centerFrequencyHz"), value.centerFrequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        parameters.insert(QStringLiteral("gainDb"), value.gainDb);
        break;
    }
    case ActiveFilterType::PeakingEq: {
        const auto& value = std::get<ActiveFilterPeakingEqParameters>(section.parameters());
        parameters.insert(QStringLiteral("centerFrequencyHz"), value.centerFrequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        parameters.insert(QStringLiteral("gainDb"), value.gainDb);
        break;
    }
    case ActiveFilterType::LowShelf: {
        const auto& value = std::get<ActiveFilterLowShelfParameters>(section.parameters());
        parameters.insert(QStringLiteral("transitionFrequencyHz"), value.transitionFrequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        parameters.insert(QStringLiteral("gainDb"), value.gainDb);
        break;
    }
    case ActiveFilterType::HighShelf: {
        const auto& value = std::get<ActiveFilterHighShelfParameters>(section.parameters());
        parameters.insert(QStringLiteral("transitionFrequencyHz"), value.transitionFrequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        parameters.insert(QStringLiteral("gainDb"), value.gainDb);
        break;
    }
    case ActiveFilterType::AllPass: {
        const auto& value = std::get<ActiveFilterAllPassParameters>(section.parameters());
        parameters.insert(QStringLiteral("order"), value.order);
        parameters.insert(QStringLiteral("frequencyHz"), value.frequencyHz);
        parameters.insert(QStringLiteral("q"), value.q);
        break;
    }
    case ActiveFilterType::Gain: {
        const auto& value = std::get<ActiveFilterGainParameters>(section.parameters());
        parameters.insert(QStringLiteral("gainDb"), value.gainDb);
        break;
    }
    case ActiveFilterType::Delay: {
        const auto& value = std::get<ActiveFilterDelayParameters>(section.parameters());
        parameters.insert(QStringLiteral("delayMs"), value.delayMs);
        break;
    }
    case ActiveFilterType::Polarity: {
        const auto& value = std::get<ActiveFilterPolarityParameters>(section.parameters());
        parameters.insert(QStringLiteral("inverted"), value.inverted);
        break;
    }
    }

    return parameters;
}

QJsonObject activeFilterChainToJson(const ActiveFilterChain& chain)
{
    QJsonArray sections;
    for (std::size_t sectionIndex = 0; sectionIndex < chain.sectionCount(); ++sectionIndex) {
        const ActiveFilterSection& section = chain.section(sectionIndex);
        QJsonObject sectionObject;
        sectionObject.insert(QStringLiteral("enabled"), section.enabled());
        sectionObject.insert(QStringLiteral("type"), activeFilterTypeToString(section.type()));
        sectionObject.insert(QStringLiteral("parameters"), activeFilterParametersToJson(section));
        sections.append(sectionObject);
    }

    QJsonObject activeFilter;
    activeFilter.insert(QStringLiteral("enabled"), chain.enabled());
    activeFilter.insert(QStringLiteral("showResponseInPlot"), chain.showResponseInPlot());
    activeFilter.insert(QStringLiteral("sections"), sections);
    return activeFilter;
}

QJsonObject baffleSettingsToJson(const BaffleSettings& settings)
{
    QJsonObject baffle;
    baffle.insert(QStringLiteral("enabled"), settings.enabled);
    baffle.insert(QStringLiteral("model"), baffleModelToString(settings.model));
    baffle.insert(QStringLiteral("widthMm"), settings.widthMm);
    baffle.insert(QStringLiteral("heightMm"), settings.heightMm);
    baffle.insert(QStringLiteral("driverXmm"), settings.driverXmm);
    baffle.insert(QStringLiteral("driverYmm"), settings.driverYmm);
    baffle.insert(QStringLiteral("boundaryCondition"),
                  baffleBoundaryConditionToString(settings.boundaryCondition));
    baffle.insert(QStringLiteral("showResponseInPlot"), settings.showResponseInPlot);
    baffle.insert(QStringLiteral("edgeSourceCount"), static_cast<int>(settings.edgeSourceCount));
    baffle.insert(QStringLiteral("leftEdgeTreatment"),
                  baffleSideEdgeTreatmentToString(settings.leftEdgeTreatment));
    baffle.insert(QStringLiteral("leftChamferSetbackMm"), settings.leftChamferSetbackMm);
    baffle.insert(QStringLiteral("rightEdgeTreatment"),
                  baffleSideEdgeTreatmentToString(settings.rightEdgeTreatment));
    baffle.insert(QStringLiteral("rightChamferSetbackMm"), settings.rightChamferSetbackMm);
    return baffle;
}

bool validateBaffleSettings(const BaffleSettings& settings,
                            int driverIndex,
                            QString* errorMessage)
{
    const QString context = QStringLiteral("Driver %1 baffle settings").arg(driverIndex + 1);
    const auto requireFinite = [&](double value, const QString& field) {
        if (std::isfinite(value)) {
            return true;
        }
        setError(errorMessage, QStringLiteral("%1 field '%2' must be finite.").arg(context, field));
        return false;
    };

    if (!requireFinite(settings.widthMm, QStringLiteral("widthMm")) ||
        !requireFinite(settings.heightMm, QStringLiteral("heightMm")) ||
        !requireFinite(settings.driverXmm, QStringLiteral("driverXmm")) ||
        !requireFinite(settings.driverYmm, QStringLiteral("driverYmm")) ||
        !requireFinite(settings.leftChamferSetbackMm, QStringLiteral("leftChamferSetbackMm")) ||
        !requireFinite(settings.rightChamferSetbackMm, QStringLiteral("rightChamferSetbackMm"))) {
        return false;
    }
    if (settings.widthMm <= 0.0) {
        setError(errorMessage, QStringLiteral("%1 widthMm must be greater than zero.").arg(context));
        return false;
    }
    if (settings.edgeSourceCount == 0 || settings.edgeSourceCount > 100000) {
        setError(errorMessage, QStringLiteral("%1 edgeSourceCount must be between 1 and 100000.").arg(context));
        return false;
    }
    if (settings.leftChamferSetbackMm < 0.0 || settings.rightChamferSetbackMm < 0.0) {
        setError(errorMessage, QStringLiteral("%1 chamfer setbacks must not be negative.").arg(context));
        return false;
    }
    if ((settings.leftEdgeTreatment != BaffleSideEdgeTreatment::Sharp &&
         settings.leftEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45) ||
        (settings.rightEdgeTreatment != BaffleSideEdgeTreatment::Sharp &&
         settings.rightEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45)) {
        setError(errorMessage, QStringLiteral("%1 contains an unsupported side-edge treatment.").arg(context));
        return false;
    }
    if ((settings.leftEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45 &&
         settings.leftChamferSetbackMm < 5.0) ||
        (settings.rightEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45 &&
         settings.rightChamferSetbackMm < 5.0)) {
        setError(errorMessage, QStringLiteral("%1 active 45-degree chamfers must be at least 5 mm.").arg(context));
        return false;
    }
    if (settings.boundaryCondition != BaffleBoundaryCondition::FreeField &&
        settings.boundaryCondition != BaffleBoundaryCondition::RigidFloorContactDiffractionOnly) {
        setError(errorMessage, QStringLiteral("%1 contains an unsupported boundary condition.").arg(context));
        return false;
    }

    switch (settings.model) {
    case BaffleModel::SimpleBaffleStep:
        return true;
    case BaffleModel::RectangularEdgeDiffraction: {
        if (settings.boundaryCondition == BaffleBoundaryCondition::RigidFloorContactDiffractionOnly &&
            (settings.leftEdgeTreatment != BaffleSideEdgeTreatment::Sharp ||
             settings.rightEdgeTreatment != BaffleSideEdgeTreatment::Sharp)) {
            setError(errorMessage,
                     QStringLiteral("%1 Rigid floor contact currently supports Sharp side edges only.")
                         .arg(context));
            return false;
        }
        const double leftSetback =
            settings.leftEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45
                ? settings.leftChamferSetbackMm
                : 0.0;
        const double rightSetback =
            settings.rightEdgeTreatment == BaffleSideEdgeTreatment::Chamfer45
                ? settings.rightChamferSetbackMm
                : 0.0;
        if (settings.heightMm <= 0.0 ||
            settings.driverXmm <= 0.0 || settings.driverXmm >= settings.widthMm ||
            settings.driverYmm <= 0.0 || settings.driverYmm >= settings.heightMm ||
            settings.edgeSourceCount < 4 ||
            leftSetback + rightSetback >= settings.widthMm ||
            settings.driverXmm <= leftSetback ||
            settings.driverXmm >= settings.widthMm - rightSetback) {
            setError(errorMessage,
                     QStringLiteral("%1 rectangular geometry is invalid; the driver centre must lie on the flat front surface, opposing chamfers must leave front width, and edgeSourceCount must be at least 4.")
                         .arg(context));
            return false;
        }
        return true;
    }
    }

    setError(errorMessage, QStringLiteral("%1 contains an unsupported model value.").arg(context));
    return false;
}


QJsonObject floorReflectionSettingsToJson(const FloorReflectionSettings& settings)
{
    QJsonObject floor;
    floor.insert(QStringLiteral("enabled"), settings.enabled);
    floor.insert(QStringLiteral("cabinetBottomAboveFloorMm"), settings.cabinetBottomAboveFloorMm);
    floor.insert(QStringLiteral("listenerHeightAboveFloorMm"), settings.listenerHeightAboveFloorMm);
    floor.insert(QStringLiteral("horizontalDistanceMm"), settings.horizontalDistanceMm);
    floor.insert(QStringLiteral("surfacePreset"), floorSurfacePresetToString(settings.surfacePreset));
    return floor;
}

bool validateFloorReflectionSettings(const FloorReflectionSettings& settings,
                                     int driverIndex,
                                     QString* errorMessage)
{
    const QString context = QStringLiteral("Driver %1 floor-reflection settings").arg(driverIndex + 1);
    if (!std::isfinite(settings.cabinetBottomAboveFloorMm) ||
        !std::isfinite(settings.listenerHeightAboveFloorMm) ||
        !std::isfinite(settings.horizontalDistanceMm)) {
        setError(errorMessage, QStringLiteral("%1 placement fields must be finite.").arg(context));
        return false;
    }
    if (settings.cabinetBottomAboveFloorMm < 0.0 ||
        settings.listenerHeightAboveFloorMm < 0.0 ||
        settings.horizontalDistanceMm < 0.0) {
        setError(errorMessage, QStringLiteral("%1 placement fields must not be negative.").arg(context));
        return false;
    }
    if (settings.surfacePreset != FloorSurfacePreset::HardRigid &&
        settings.surfacePreset != FloorSurfacePreset::MikiReference10mm100k) {
        setError(errorMessage, QStringLiteral("%1 contains an unsupported surface preset.").arg(context));
        return false;
    }
    return true;
}

bool validateActiveFilterSection(const ActiveFilterSection& section,
                                 int driverIndex,
                                 std::size_t sectionIndex,
                                 QString* errorMessage)
{
    auto requireFinite = [driverIndex, sectionIndex, errorMessage](double value,
                                                                   const QString& fieldName) {
        if (std::isfinite(value)) {
            return true;
        }
        setError(errorMessage,
                 QStringLiteral("Driver %1 active-filter section %2 contains a non-finite '%3' value.")
                     .arg(driverIndex + 1)
                     .arg(static_cast<qulonglong>(sectionIndex + 1))
                     .arg(fieldName));
        return false;
    };
    auto requireCharacteristic = [driverIndex, sectionIndex, errorMessage](
                                     ActiveFilterCharacteristic characteristic) {
        if (!activeFilterCharacteristicToString(characteristic).isEmpty()) {
            return true;
        }
        setError(errorMessage,
                 QStringLiteral("Driver %1 active-filter section %2 contains an invalid characteristic.")
                     .arg(driverIndex + 1)
                     .arg(static_cast<qulonglong>(sectionIndex + 1)));
        return false;
    };

    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        const auto& value = std::get<ActiveFilterLowPassParameters>(section.parameters());
        return requireCharacteristic(value.characteristic) &&
               requireFinite(value.frequencyHz, QStringLiteral("frequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q"));
    }
    case ActiveFilterType::HighPass: {
        const auto& value = std::get<ActiveFilterHighPassParameters>(section.parameters());
        return requireCharacteristic(value.characteristic) &&
               requireFinite(value.frequencyHz, QStringLiteral("frequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q"));
    }
    case ActiveFilterType::BandPass: {
        const auto& value = std::get<ActiveFilterBandPassParameters>(section.parameters());
        return requireCharacteristic(value.characteristic) &&
               requireFinite(value.lowerFrequencyHz, QStringLiteral("lowerFrequencyHz")) &&
               requireFinite(value.upperFrequencyHz, QStringLiteral("upperFrequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q"));
    }
    case ActiveFilterType::Notch: {
        const auto& value = std::get<ActiveFilterNotchParameters>(section.parameters());
        return requireFinite(value.centerFrequencyHz, QStringLiteral("centerFrequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q")) &&
               requireFinite(value.gainDb, QStringLiteral("gainDb"));
    }
    case ActiveFilterType::PeakingEq: {
        const auto& value = std::get<ActiveFilterPeakingEqParameters>(section.parameters());
        return requireFinite(value.centerFrequencyHz, QStringLiteral("centerFrequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q")) &&
               requireFinite(value.gainDb, QStringLiteral("gainDb"));
    }
    case ActiveFilterType::LowShelf: {
        const auto& value = std::get<ActiveFilterLowShelfParameters>(section.parameters());
        return requireFinite(value.transitionFrequencyHz, QStringLiteral("transitionFrequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q")) &&
               requireFinite(value.gainDb, QStringLiteral("gainDb"));
    }
    case ActiveFilterType::HighShelf: {
        const auto& value = std::get<ActiveFilterHighShelfParameters>(section.parameters());
        return requireFinite(value.transitionFrequencyHz, QStringLiteral("transitionFrequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q")) &&
               requireFinite(value.gainDb, QStringLiteral("gainDb"));
    }
    case ActiveFilterType::AllPass: {
        const auto& value = std::get<ActiveFilterAllPassParameters>(section.parameters());
        return requireFinite(value.frequencyHz, QStringLiteral("frequencyHz")) &&
               requireFinite(value.q, QStringLiteral("q"));
    }
    case ActiveFilterType::Gain:
        return requireFinite(std::get<ActiveFilterGainParameters>(section.parameters()).gainDb,
                             QStringLiteral("gainDb"));
    case ActiveFilterType::Delay:
        return requireFinite(std::get<ActiveFilterDelayParameters>(section.parameters()).delayMs,
                             QStringLiteral("delayMs"));
    case ActiveFilterType::Polarity:
        return true;
    }

    return false;
}

bool validateActiveFilterChain(const ActiveFilterChain& chain,
                               int driverIndex,
                               QString* errorMessage)
{
    for (std::size_t sectionIndex = 0; sectionIndex < chain.sectionCount(); ++sectionIndex) {
        if (!validateActiveFilterSection(chain.section(sectionIndex),
                                         driverIndex,
                                         sectionIndex,
                                         errorMessage)) {
            return false;
        }
    }
    return true;
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
        currentDriver.calculateParameters();
        currentDriver.setModified();
    }
}
}

bool KFilterProjectIo::writeDriverSupplementToJson(
    QJsonObject& driverObject,
    const KFilterMeasurementCurve& measurementCurve,
    bool measurementHidden,
    const ActiveFilterChain& activeFilterChain,
    const BaffleSettings& baffleSettings,
    const FloorReflectionSettings& floorReflectionSettings,
    int driverIndex,
    QString* errorMessage)
{
    if (!validateMeasurementCurve(measurementCurve, driverIndex, errorMessage) ||
        !validateActiveFilterChain(activeFilterChain, driverIndex, errorMessage) ||
        !validateBaffleSettings(baffleSettings, driverIndex, errorMessage) ||
        !validateFloorReflectionSettings(floorReflectionSettings, driverIndex, errorMessage)) {
        return false;
    }

    driverObject.insert(
        QStringLiteral("measurements"),
        driverMeasurementsToJson(measurementCurve, measurementHidden && !measurementCurve.isEmpty()));
    driverObject.insert(QStringLiteral("activeFilter"), activeFilterChainToJson(activeFilterChain));
    driverObject.insert(QStringLiteral("baffle"), baffleSettingsToJson(baffleSettings));
    driverObject.insert(QStringLiteral("floorReflection"),
                        floorReflectionSettingsToJson(floorReflectionSettings));
    return true;
}

bool KFilterProjectIo::readDriverSupplementFromJson(
    const QJsonObject& driverObject,
    KFilterMeasurementCurve& measurementCurve,
    bool& measurementHidden,
    ActiveFilterChain& activeFilterChain,
    BaffleSettings& baffleSettings,
    FloorReflectionSettings& floorReflectionSettings,
    const QString& context,
    QString* errorMessage)
{
    QJsonObject measurementsObject;
    if (!readObject(driverObject,
                    QStringLiteral("measurements"),
                    measurementsObject,
                    context,
                    errorMessage)) {
        return false;
    }

    KFilterMeasurementCurve parsedMeasurementCurve;
    bool parsedMeasurementHidden = false;
    ActiveFilterChain parsedActiveFilterChain;
    BaffleSettings parsedBaffleSettings;
    FloorReflectionSettings parsedFloorReflectionSettings;

    if (!jsonToDriverMeasurements(driverObject,
                                  parsedMeasurementCurve,
                                  parsedMeasurementHidden,
                                  JsonFormatVersion,
                                  context,
                                  errorMessage) ||
        !jsonToActiveFilterChain(driverObject,
                                 parsedActiveFilterChain,
                                 context,
                                 errorMessage) ||
        !jsonToBaffleSettings(driverObject,
                              parsedBaffleSettings,
                              JsonFormatVersion,
                              context,
                              errorMessage) ||
        !jsonToFloorReflectionSettings(driverObject,
                                       parsedFloorReflectionSettings,
                                       context,
                                       errorMessage)) {
        return false;
    }

    measurementCurve = parsedMeasurementCurve;
    measurementHidden = parsedMeasurementHidden && !measurementCurve.isEmpty();
    activeFilterChain = parsedActiveFilterChain;
    baffleSettings = parsedBaffleSettings;
    floorReflectionSettings = parsedFloorReflectionSettings;
    return true;
}

bool KFilterProjectIo::loadFromFile(const QString& filePath,
                                    driver (&drivers)[DriverCount],
                                    MeasurementCurves& splCorrectionCurves,
                                    bool& mergeMeasurementsEnabled,
                                    MeasurementHiddenStates& measurementHiddenForDrivers,
                                    ActiveFilterChains& activeFilterChains,
                                    BaffleSettingsPerDriver& baffleSettings,
                                    FloorReflectionSettingsPerDriver& floorReflectionSettings,
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
    ActiveFilterChains parsedActiveFilterChains{};
    BaffleSettingsPerDriver parsedBaffleSettings{};
    FloorReflectionSettingsPerDriver parsedFloorReflectionSettings{};
    const char firstByte = data.at(firstByteIndex);
    const bool isJson = firstByte == '{' || firstByte == '[';
    const QByteArray significantData = data.mid(firstByteIndex);
    const bool loaded = isJson
        ? loadJsonProject(significantData,
                          parsedDrivers,
                          parsedSplCorrectionCurves,
                          parsedMergeMeasurementsEnabled,
                          parsedMeasurementHiddenForDrivers,
                          parsedActiveFilterChains,
                          parsedBaffleSettings,
                          parsedFloorReflectionSettings,
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
    activeFilterChains = parsedActiveFilterChains;
    baffleSettings = parsedBaffleSettings;
    floorReflectionSettings = parsedFloorReflectionSettings;

    return true;
}

bool KFilterProjectIo::saveToFile(const QString& filePath,
                                  driver (&drivers)[DriverCount],
                                  const MeasurementCurves& splCorrectionCurves,
                                  bool mergeMeasurementsEnabled,
                                  const MeasurementHiddenStates& measurementHiddenForDrivers,
                                  const ActiveFilterChains& activeFilterChains,
                                  const BaffleSettingsPerDriver& baffleSettings,
                                  const FloorReflectionSettingsPerDriver& floorReflectionSettings,
                                  QString* errorMessage)
{
    QJsonArray driverArray;
    for (int driverIndex = 0; driverIndex < DriverCount; ++driverIndex) {
        const driver& currentDriver = drivers[driverIndex];
        const KFilterMeasurementCurve& correctionCurve =
            splCorrectionCurves[static_cast<std::size_t>(driverIndex)];
        if (!validateFiniteDriverData(currentDriver, driverIndex, errorMessage)) {
            return false;
        }

        QJsonObject driverObject;
        driverObject.insert(QStringLiteral("parameters"), driverParametersToJson(currentDriver));
        driverObject.insert(QStringLiteral("network"), driverNetworkToJson(currentDriver));
        if (!writeDriverSupplementToJson(
                driverObject,
                correctionCurve,
                measurementHiddenForDrivers[static_cast<std::size_t>(driverIndex)],
                activeFilterChains[static_cast<std::size_t>(driverIndex)],
                baffleSettings[static_cast<std::size_t>(driverIndex)],
                floorReflectionSettings[static_cast<std::size_t>(driverIndex)],
                driverIndex,
                errorMessage)) {
            return false;
        }
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
