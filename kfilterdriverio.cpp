/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterdriverio.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>

namespace
{
constexpr int FormatVersion = 1;
const QString FormatName = QStringLiteral("KFilter driver slot");
const QString ApplicationName = QStringLiteral("KFilter6");

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool readObject(const QJsonObject& parent,
                const QString& key,
                QJsonObject& object,
                QString* errorMessage)
{
    const QJsonValue value = parent.value(key);
    if (!value.isObject()) {
        setError(errorMessage, QStringLiteral("Missing or invalid object '%1'.").arg(key));
        return false;
    }

    object = value.toObject();
    return true;
}

bool readRequiredString(const QJsonObject& object,
                        const QString& key,
                        QString& value,
                        QString* errorMessage)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isString()) {
        setError(errorMessage, QStringLiteral("Missing or invalid string field '%1'.").arg(key));
        return false;
    }

    value = jsonValue.toString();
    return true;
}

bool readRequiredDouble(const QJsonObject& object,
                        const QString& key,
                        double& value,
                        QString* errorMessage)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble()) {
        setError(errorMessage, QStringLiteral("Missing or invalid numeric field '%1'.").arg(key));
        return false;
    }

    value = jsonValue.toDouble();
    if (!std::isfinite(value)) {
        setError(errorMessage, QStringLiteral("Field '%1' must be finite.").arg(key));
        return false;
    }

    return true;
}

bool readOptionalDouble(const QJsonObject& object,
                        const QString& key,
                        double& value,
                        bool& present,
                        QString* errorMessage)
{
    present = false;
    if (!object.contains(key)) {
        return true;
    }

    present = true;
    return readRequiredDouble(object, key, value, errorMessage);
}

bool readRequiredInt(const QJsonObject& object,
                     const QString& key,
                     int& value,
                     QString* errorMessage)
{
    double doubleValue = 0.0;
    if (!readRequiredDouble(object, key, doubleValue, errorMessage)) {
        return false;
    }

    const int intValue = static_cast<int>(doubleValue);
    if (std::abs(doubleValue - static_cast<double>(intValue)) > 0.0000001) {
        setError(errorMessage, QStringLiteral("Field '%1' must be an integer.").arg(key));
        return false;
    }

    value = intValue;
    return true;
}

bool readRequiredBool(const QJsonObject& object,
                      const QString& key,
                      bool& value,
                      QString* errorMessage)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isBool()) {
        setError(errorMessage, QStringLiteral("Missing or invalid boolean field '%1'.").arg(key));
        return false;
    }

    value = jsonValue.toBool();
    return true;
}

void writeBool(QJsonObject& object, const QString& key, bool value)
{
    object.insert(key, value);
}

bool loadNetworkValues(const QJsonObject& networkObject,
                       driver& drv,
                       QString* errorMessage)
{
    int unitBaseIndex = 1;
    if (networkObject.contains(QStringLiteral("unitBaseIndex"))) {
        if (!readRequiredInt(networkObject, QStringLiteral("unitBaseIndex"), unitBaseIndex, errorMessage)) {
            return false;
        }
    }

    const QJsonValue valuesValue = networkObject.value(QStringLiteral("values"));
    if (!valuesValue.isArray()) {
        setError(errorMessage, QStringLiteral("Missing or invalid network values array."));
        return false;
    }

    const QJsonArray values = valuesValue.toArray();
    if (unitBaseIndex == 1) {
        if (values.size() != KFilterDriverIo::NetworkUnitCount) {
            setError(errorMessage,
                     QStringLiteral("Network values array must contain %1 entries.")
                         .arg(KFilterDriverIo::NetworkUnitCount));
            return false;
        }

        for (int index = 0; index < values.size(); ++index) {
            const QJsonValue jsonValue = values.at(index);
            if (!jsonValue.isDouble() || !std::isfinite(jsonValue.toDouble())) {
                setError(errorMessage,
                         QStringLiteral("Invalid network value at unit %1.").arg(index + 1));
                return false;
            }
            drv.setUnit(index + 1, jsonValue.toDouble());
        }
        return true;
    }

    if (unitBaseIndex == 0) {
        if (values.size() != KFilterDriverIo::NetworkUnitCount + 1) {
            setError(errorMessage,
                     QStringLiteral("Zero-based network values array must contain %1 entries.")
                         .arg(KFilterDriverIo::NetworkUnitCount + 1));
            return false;
        }

        for (int index = 1; index < values.size(); ++index) {
            const QJsonValue jsonValue = values.at(index);
            if (!jsonValue.isDouble() || !std::isfinite(jsonValue.toDouble())) {
                setError(errorMessage,
                         QStringLiteral("Invalid network value at unit %1.").arg(index));
                return false;
            }
            drv.setUnit(index, jsonValue.toDouble());
        }
        return true;
    }

    setError(errorMessage, QStringLiteral("Unsupported network unitBaseIndex %1.").arg(unitBaseIndex));
    return false;
}

QJsonObject driverToJson(const driver& drv)
{
    QJsonObject driverObject;
    driverObject.insert(QStringLiteral("title"), drv.GetTitle());
    driverObject.insert(QStringLiteral("rdc_ohm"), drv.getRdc());
    driverObject.insert(QStringLiteral("lsp_h"), drv.getLsp());
    driverObject.insert(QStringLiteral("fs_hz"), drv.getF0());
    driverObject.insert(QStringLiteral("qts"), drv.getQtc());
    driverObject.insert(QStringLiteral("qes"), drv.getQes());
    driverObject.insert(QStringLiteral("qms"), drv.getQms());
    driverObject.insert(QStringLiteral("vas_l"), drv.getVas());
    driverObject.insert(QStringLiteral("diameter_cm"), drv.getDm());
    driverObject.insert(QStringLiteral("vb_l"), drv.Vb);
    driverObject.insert(QStringLiteral("ql"), drv.getQl());
    driverObject.insert(QStringLiteral("fb_hz"), drv.Fb);
    driverObject.insert(QStringLiteral("v2_l"), drv.V2);
    driverObject.insert(QStringLiteral("enclosureTypeProposal"), drv.GTypProposal);
    driverObject.insert(QStringLiteral("gainLinear"), drv.gain);
    writeBool(driverObject, QStringLiteral("pressureActive"), drv.PressureisActive);
    writeBool(driverObject, QStringLiteral("impedanceActive"), drv.ImpedanzisActive);
    writeBool(driverObject, QStringLiteral("summaryActive"), drv.SummaryisActive);
    writeBool(driverObject, QStringLiteral("scalarSummaryActive"), drv.ScalarSummaryisActive);
    writeBool(driverObject, QStringLiteral("impedanceSummaryActive"), drv.ImpedanzSummaryisActive);
    writeBool(driverObject, QStringLiteral("invertPhase"), drv.InvertPhase);
    writeBool(driverObject, QStringLiteral("fullCircuit"), drv.getFullCircuit());
    return driverObject;
}

bool jsonToDriver(const QJsonObject& driverObject,
                  driver& drv,
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

    if (!readRequiredString(driverObject, QStringLiteral("title"), title, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("rdc_ohm"), rdc, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("lsp_h"), lsp, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("fs_hz"), fs, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("qts"), qts, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("qes"), qes, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("qms"), qms, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("vas_l"), vas, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("diameter_cm"), diameter, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("vb_l"), vb, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("ql"), ql, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("fb_hz"), fb, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("v2_l"), v2, errorMessage) ||
        !readRequiredInt(driverObject, QStringLiteral("enclosureTypeProposal"), enclosureTypeProposal, errorMessage) ||
        !readRequiredDouble(driverObject, QStringLiteral("gainLinear"), gain, errorMessage) ||
        !readRequiredBool(driverObject, QStringLiteral("pressureActive"), pressureActive, errorMessage) ||
        !readRequiredBool(driverObject, QStringLiteral("impedanceActive"), impedanceActive, errorMessage) ||
        !readRequiredBool(driverObject, QStringLiteral("summaryActive"), summaryActive, errorMessage) ||
        !readRequiredBool(driverObject, QStringLiteral("scalarSummaryActive"), scalarSummaryActive, errorMessage) ||
        !readRequiredBool(driverObject, QStringLiteral("impedanceSummaryActive"), impedanceSummaryActive, errorMessage) ||
        !readRequiredBool(driverObject, QStringLiteral("invertPhase"), invertPhase, errorMessage) ||
        !readRequiredBool(driverObject, QStringLiteral("fullCircuit"), fullCircuit, errorMessage)) {
        return false;
    }

    if (ql <= 0.0) {
        setError(errorMessage, QStringLiteral("Field 'ql' must be greater than zero."));
        return false;
    }

    if (enclosureTypeProposal < 0 || enclosureTypeProposal > 3) {
        setError(errorMessage, QStringLiteral("Field 'enclosureTypeProposal' must be between 0 and 3."));
        return false;
    }

    if (gain <= 0.0) {
        setError(errorMessage, QStringLiteral("Field 'gainLinear' must be greater than zero."));
        return false;
    }

    drv.SetTitle(title);
    drv.setRdc(rdc);
    drv.setLsp(lsp);
    drv.setF0(fs);
    drv.setQtc(qts);
    drv.setQes(qes);
    drv.setQms(qms);
    drv.setVas(vas);
    drv.setDm(diameter);
    drv.Vb = vb;
    drv.setQl(ql);
    drv.Fb = fb;
    drv.V2 = v2;
    drv.GTypProposal = enclosureTypeProposal;
    drv.gain = gain;
    drv.PressureisActive = pressureActive;
    drv.ImpedanzisActive = impedanceActive;
    drv.SummaryisActive = summaryActive;
    drv.ScalarSummaryisActive = scalarSummaryActive;
    drv.ImpedanzSummaryisActive = impedanceSummaryActive;
    drv.InvertPhase = invertPhase;
    drv.setFullCircuit(fullCircuit);
    drv.Berechneparameter();
    drv.setmodified();

    return true;
}
}

bool KFilterDriverIo::loadDriverSlotFromFile(const QString& filePath,
                                             DriverSlot& slot,
                                             QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(errorMessage,
                 QStringLiteral("Cannot open driver slot file '%1' for reading: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(errorMessage,
                 QStringLiteral("Cannot parse driver slot file '%1': %2")
                     .arg(filePath, parseError.errorString()));
        return false;
    }

    if (!document.isObject()) {
        setError(errorMessage, QStringLiteral("Driver slot file root must be a JSON object."));
        return false;
    }

    const QJsonObject root = document.object();
    QString format;
    int formatVersion = 0;
    if (!readRequiredString(root, QStringLiteral("format"), format, errorMessage) ||
        !readRequiredInt(root, QStringLiteral("formatVersion"), formatVersion, errorMessage)) {
        return false;
    }

    if (format != FormatName) {
        setError(errorMessage,
                 QStringLiteral("Unsupported driver slot format '%1'.").arg(format));
        return false;
    }

    if (formatVersion != FormatVersion) {
        setError(errorMessage,
                 QStringLiteral("Unsupported driver slot format version %1.").arg(formatVersion));
        return false;
    }

    QJsonObject driverObject;
    QJsonObject networkObject;
    if (!readObject(root, QStringLiteral("driver"), driverObject, errorMessage) ||
        !readObject(root, QStringLiteral("network"), networkObject, errorMessage)) {
        return false;
    }

    DriverSlot parsedSlot;
    if (!jsonToDriver(driverObject, parsedSlot.driverData, errorMessage) ||
        !loadNetworkValues(networkObject, parsedSlot.driverData, errorMessage)) {
        return false;
    }

    const QJsonValue dialogHintsValue = root.value(QStringLiteral("dialogHints"));
    if (dialogHintsValue.isObject()) {
        const QJsonObject dialogHints = dialogHintsValue.toObject();
        bool tubeDiameterPresent = false;
        double tubeDiameterCm = 0.0;
        if (!readOptionalDouble(dialogHints,
                                QStringLiteral("tubeDiameter_cm"),
                                tubeDiameterCm,
                                tubeDiameterPresent,
                                errorMessage)) {
            return false;
        }
        if (tubeDiameterPresent) {
            if (tubeDiameterCm < 0.0) {
                setError(errorMessage, QStringLiteral("Field 'tubeDiameter_cm' must not be negative."));
                return false;
            }
            parsedSlot.hasTubeDiameterCm = true;
            parsedSlot.tubeDiameterCm = tubeDiameterCm;
        }
    }

    parsedSlot.driverData.Berechneparameter();
    parsedSlot.driverData.setmodified();
    slot = parsedSlot;
    return true;
}

bool KFilterDriverIo::saveDriverSlotToFile(const QString& filePath,
                                           const DriverSlot& slot,
                                           QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setError(errorMessage,
                 QStringLiteral("Cannot open driver slot file '%1' for writing: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), FormatName);
    root.insert(QStringLiteral("formatVersion"), FormatVersion);
    root.insert(QStringLiteral("application"), ApplicationName);
    root.insert(QStringLiteral("driver"), driverToJson(slot.driverData));

    QJsonArray networkValues;
    for (int unitIndex = 1; unitIndex <= NetworkUnitCount; ++unitIndex) {
        networkValues.append(slot.driverData.getUnit(unitIndex));
    }

    QJsonObject networkObject;
    networkObject.insert(QStringLiteral("unitBaseIndex"), 1);
    networkObject.insert(QStringLiteral("values"), networkValues);
    root.insert(QStringLiteral("network"), networkObject);

    if (slot.hasTubeDiameterCm) {
        QJsonObject dialogHints;
        dialogHints.insert(QStringLiteral("tubeDiameter_cm"), slot.tubeDiameterCm);
        root.insert(QStringLiteral("dialogHints"), dialogHints);
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
