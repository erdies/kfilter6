/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

/*
 * Patch 298 regression test.
 *
 * The acoustic core divides by Rdc, Qts, Qes, Qms and, on the tuned-enclosure
 * paths, by Vas. Before Patch 298 a project or driver file carrying a zero or
 * negative value loaded successfully and produced non-finite pressure and
 * impedance samples across the whole frequency grid. This test pins both
 * directions of the new contract: rejected files must not load, and every
 * combination that produced finite results before Patch 298 must keep loading.
 */

#include "driver.h"
#include "driverparametervalidation.h"
#include "kfilterdriverio.h"
#include "kfilterprojectio.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QString>
#include <QTextStream>

#include <cmath>
#include <complex>

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

KFilterDriverValidation::Parameters defaultParameters()
{
    // The historical default driver: Open Baffle, no enclosure volume.
    KFilterDriverValidation::Parameters parameters;
    parameters.rdcOhm = 5.1;
    parameters.lspH = 0.00017;
    parameters.fsHz = 307.0;
    parameters.qts = 1.14;
    parameters.qes = 2.87;
    parameters.qms = 1.9;
    parameters.vasLitres = 10.0;
    parameters.diameterCm = 7.3;
    parameters.vbLitres = 0.0;
    parameters.fbHz = 0.0;
    parameters.v2Litres = 0.0;
    parameters.enclosureTypeProposal = static_cast<int>(EnclosureType::OpenBaffle);
    return parameters;
}

void expectAccepted(const KFilterDriverValidation::Parameters& parameters, const QString& label)
{
    QString reason;
    if (!KFilterDriverValidation::validateDriverParameters(parameters, &reason)) {
        reportFailure(QStringLiteral("%1 should be accepted but was rejected: %2")
                          .arg(label, reason));
    }
}

void expectRejected(const KFilterDriverValidation::Parameters& parameters, const QString& label)
{
    QString reason;
    if (KFilterDriverValidation::validateDriverParameters(parameters, &reason)) {
        reportFailure(QStringLiteral("%1 should be rejected but was accepted").arg(label));
        return;
    }
    if (reason.isEmpty()) {
        reportFailure(QStringLiteral("%1 was rejected without a reason").arg(label));
    }
}

void testValidationRules()
{
    expectAccepted(defaultParameters(), QStringLiteral("Historical default driver"));

    {
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.rdcOhm = 0.0;
        expectRejected(parameters, QStringLiteral("Rdc == 0"));
        parameters.rdcOhm = -5.1;
        expectRejected(parameters, QStringLiteral("Rdc < 0"));
    }

    {
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.qes = 0.0;
        expectRejected(parameters, QStringLiteral("Qes == 0 with F0 != 0"));
    }

    {
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.qms = 0.0;
        expectRejected(parameters, QStringLiteral("Qms == 0 with F0 != 0"));
    }

    {
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.qts = 0.0;
        expectRejected(parameters, QStringLiteral("Qts == 0 with F0 != 0"));
    }

    {
        // F0 == 0 bypasses the acoustic path entirely; the quality factors are
        // then unused and a file that never carried them must keep loading.
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.fsHz = 0.0;
        parameters.qts = 0.0;
        parameters.qes = 0.0;
        parameters.qms = 0.0;
        parameters.vasLitres = 0.0;
        parameters.diameterCm = 0.0;
        expectAccepted(parameters, QStringLiteral("F0 == 0 without Thiele/Small data"));
    }

    {
        // Sealed does not divide by Vas, so Vas == 0 stays acceptable there.
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.vasLitres = 0.0;
        parameters.vbLitres = 30.0;
        parameters.enclosureTypeProposal = static_cast<int>(EnclosureType::Sealed);
        expectAccepted(parameters, QStringLiteral("Sealed with Vas == 0"));
    }

    {
        // The Vented branch evaluates Vb * motionalInductance / Vas.
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.vasLitres = 0.0;
        parameters.vbLitres = 30.0;
        parameters.fbHz = 40.0;
        parameters.enclosureTypeProposal = static_cast<int>(EnclosureType::Vented);
        expectRejected(parameters, QStringLiteral("Vented with Vas == 0"));

        parameters.enclosureTypeProposal = static_cast<int>(EnclosureType::Bandpass);
        parameters.v2Litres = 15.0;
        expectRejected(parameters, QStringLiteral("Bandpass with Vas == 0"));

        parameters.vasLitres = 10.0;
        expectAccepted(parameters, QStringLiteral("Bandpass with Vas > 0"));
    }

    {
        // A Vented proposal without a tuning frequency never reaches the
        // division and therefore stays acceptable.
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.vasLitres = 0.0;
        parameters.vbLitres = 30.0;
        parameters.fbHz = 0.0;
        parameters.enclosureTypeProposal = static_cast<int>(EnclosureType::Vented);
        expectAccepted(parameters, QStringLiteral("Vented proposal without Fb and Vas == 0"));
    }

    {
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.diameterCm = -7.3;
        expectRejected(parameters, QStringLiteral("Dm < 0"));
    }

    {
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.vbLitres = -30.0;
        expectRejected(parameters, QStringLiteral("Vb < 0"));
    }

    {
        KFilterDriverValidation::Parameters parameters = defaultParameters();
        parameters.enclosureTypeProposal = 4;
        expectRejected(parameters, QStringLiteral("Enclosure type proposal out of range"));
    }
}

bool responsesAreFinite(driver& subject)
{
    subject.calculatePressureResponse();
    subject.calculateImpedanceResponse();

    for (const std::complex<double>& sample : subject.pressureResponse()) {
        if (!std::isfinite(sample.real()) || !std::isfinite(sample.imag())) {
            return false;
        }
    }
    for (const std::complex<double>& sample : subject.impedanceResponse()) {
        if (!std::isfinite(sample.real()) || !std::isfinite(sample.imag())) {
            return false;
        }
    }
    return true;
}

void testRejectedParametersWouldBreakTheCore()
{
    // Demonstrates why the contract exists: the very value the validator
    // rejects drives the acoustic core into non-finite samples.
    driver subject;
    subject.setQes(0.0);
    expect(!responsesAreFinite(subject),
           QStringLiteral("Qes == 0 was expected to produce non-finite samples"));

    driver reference;
    expect(responsesAreFinite(reference),
           QStringLiteral("The default driver must produce finite samples"));
}

bool writeTextFile(const QString& filePath, const QString& content, QString& error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        error = QStringLiteral("Could not create '%1': %2").arg(filePath, file.errorString());
        return false;
    }
    if (file.write(content.toUtf8()) < 0) {
        error = QStringLiteral("Could not write '%1': %2").arg(filePath, file.errorString());
        return false;
    }
    return true;
}

bool readJsonRoot(const QString& filePath, QJsonObject& root, QString& error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Could not read '%1': %2").arg(filePath, file.errorString());
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("'%1' is not a valid JSON object: %2")
                    .arg(filePath, parseError.errorString());
        return false;
    }

    root = document.object();
    return true;
}

struct ProjectLoadTargets
{
    driver drivers[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves measurements;
    bool mergeMeasurementsEnabled = false;
    KFilterProjectIo::MeasurementHiddenStates hiddenStates{};
    KFilterProjectIo::ActiveFilterChains activeFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver baffleSettings{};
    KFilterProjectIo::FloorReflectionSettingsPerDriver floorReflectionSettings{};
};

bool loadProject(const QString& filePath, QString* errorMessage)
{
    ProjectLoadTargets targets;
    return KFilterProjectIo::loadFromFile(filePath,
                                          targets.drivers,
                                          targets.measurements,
                                          targets.mergeMeasurementsEnabled,
                                          targets.hiddenStates,
                                          targets.activeFilters,
                                          targets.baffleSettings,
                                          targets.floorReflectionSettings,
                                          errorMessage);
}

void testJsonProjectPath(const QString& filePath)
{
    driver drivers[KFilterProjectIo::DriverCount];
    KFilterProjectIo::MeasurementCurves measurements;
    KFilterProjectIo::MeasurementHiddenStates hiddenStates{};
    KFilterProjectIo::ActiveFilterChains activeFilters{};
    KFilterProjectIo::BaffleSettingsPerDriver baffleSettings{};
    KFilterProjectIo::FloorReflectionSettingsPerDriver floorReflectionSettings{};

    QString errorMessage;
    if (!KFilterProjectIo::saveToFile(filePath, drivers, measurements, false, hiddenStates,
                                      activeFilters, baffleSettings, floorReflectionSettings,
                                      &errorMessage)) {
        reportFailure(QStringLiteral("Could not save the reference project: %1").arg(errorMessage));
        return;
    }

    QJsonObject validRoot;
    if (!readJsonRoot(filePath, validRoot, errorMessage)) {
        reportFailure(errorMessage);
        return;
    }

    expect(loadProject(filePath, &errorMessage),
           QStringLiteral("The unmodified reference project must load: %1").arg(errorMessage));

    const auto writeWithDriverField = [&](const QString& key, double value) {
        QJsonObject root = validRoot;
        QJsonObject project = root.value(QStringLiteral("project")).toObject();
        QJsonArray jsonDrivers = project.value(QStringLiteral("drivers")).toArray();
        QJsonObject firstDriver = jsonDrivers.at(0).toObject();
        QJsonObject parameters = firstDriver.value(QStringLiteral("parameters")).toObject();
        parameters.insert(key, value);
        firstDriver.insert(QStringLiteral("parameters"), parameters);
        jsonDrivers[0] = firstDriver;
        project.insert(QStringLiteral("drivers"), jsonDrivers);
        root.insert(QStringLiteral("project"), project);

        QString writeError;
        if (!writeTextFile(filePath,
                           QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)),
                           writeError)) {
            reportFailure(writeError);
            return false;
        }
        return true;
    };

    const struct
    {
        const char* key;
        double value;
    } rejectedFields[] = {
        {"rdc_ohm", 0.0},
        {"qes", 0.0},
        {"qms", 0.0},
        {"qts", 0.0},
        {"vas_l", -10.0},
        {"diameter_cm", -7.3},
    };

    for (const auto& entry : rejectedFields) {
        const QString key = QLatin1String(entry.key);
        if (!writeWithDriverField(key, entry.value)) {
            continue;
        }
        QString loadError;
        expect(!loadProject(filePath, &loadError),
               QStringLiteral("JSON project with %1 = %2 must be rejected")
                   .arg(key)
                   .arg(entry.value));
    }
}

QString createLegacyProject(double qes)
{
    QString content;
    QTextStream stream(&content);
    stream.setLocale(QLocale::c());
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(6);

    stream << "# KFilter datafile\n[Network values]";
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        stream << "\n# Driver " << (driverIndex + 1);
        for (int unitIndex = 0; unitIndex < KFilterProjectIo::NetworkUnitCount; ++unitIndex) {
            stream << "\n0";
        }
    }

    stream << "\n[Driver parameters]";
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        stream << "\n# Driver " << (driverIndex + 1)
               << "\nRdc=" << 5.1
               << "\nLsp=" << 0.00017
               << "\nF0=" << 307.0
               << "\nQts=" << 1.14
               << "\nQe=" << qes
               << "\nQms=" << 1.9
               << "\nVas=" << 10.0
               << "\nDm=" << 7.3
               << "\nVb=" << 0.0
               << "\nFb=" << 0.0
               << "\nV2=" << 0.0
               << "\nGTypProposal=" << 0
               << "\nGain=" << 1.0
               << "\nPressure=0"
               << "\nImpedanz=0"
               << "\nSummary=0"
               << "\nScalarSummary=0"
               << "\nImpedanzSummary=0"
               << "\nInvertPhase=0"
               << "\nTitle=Driver " << (driverIndex + 1);
    }

    stream << "\n[Driver enclosure losses]";
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        stream << "\n# Driver " << (driverIndex + 1) << "\nQl=" << 10.0;
    }

    stream << '\n';
    return content;
}

void testLegacyProjectPath(const QString& filePath)
{
    QString errorMessage;

    if (!writeTextFile(filePath, createLegacyProject(2.87), errorMessage)) {
        reportFailure(errorMessage);
        return;
    }
    expect(loadProject(filePath, &errorMessage),
           QStringLiteral("A valid legacy project must still load: %1").arg(errorMessage));

    if (!writeTextFile(filePath, createLegacyProject(0.0), errorMessage)) {
        reportFailure(errorMessage);
        return;
    }
    expect(!loadProject(filePath, &errorMessage),
           QStringLiteral("A legacy project with Qe = 0 must be rejected"));
}

void testDriverSlotPath(const QString& filePath)
{
    KFilterDriverIo::DriverSlot slot;
    QString errorMessage;

    if (!KFilterDriverIo::saveDriverSlotToFile(filePath, slot, &errorMessage)) {
        reportFailure(QStringLiteral("Could not save the reference driver slot: %1").arg(errorMessage));
        return;
    }

    QJsonObject validRoot;
    if (!readJsonRoot(filePath, validRoot, errorMessage)) {
        reportFailure(errorMessage);
        return;
    }

    KFilterDriverIo::DriverSlot loadedSlot;
    expect(KFilterDriverIo::loadDriverSlotFromFile(filePath, loadedSlot, &errorMessage),
           QStringLiteral("The unmodified reference driver slot must load: %1").arg(errorMessage));

    const auto rejectField = [&](const QString& key, double value) {
        QJsonObject root = validRoot;
        QJsonObject driverObject = root.value(QStringLiteral("driver")).toObject();
        if (driverObject.isEmpty()) {
            reportFailure(QStringLiteral("Unexpected driver slot layout; 'driver' object missing"));
            return;
        }
        driverObject.insert(key, value);
        root.insert(QStringLiteral("driver"), driverObject);

        QString writeError;
        if (!writeTextFile(filePath,
                           QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)),
                           writeError)) {
            reportFailure(writeError);
            return;
        }

        KFilterDriverIo::DriverSlot rejectedSlot;
        QString loadError;
        expect(!KFilterDriverIo::loadDriverSlotFromFile(filePath, rejectedSlot, &loadError),
               QStringLiteral("Driver slot with %1 = %2 must be rejected").arg(key).arg(value));
    };

    rejectField(QStringLiteral("rdc_ohm"), -5.1);
    rejectField(QStringLiteral("qts"), 0.0);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    (void)app;

    testValidationRules();
    testRejectedParametersWouldBreakTheCore();
    testJsonProjectPath(
        QDir::temp().filePath(QStringLiteral("kfilter_driver_parameter_validation.kfp")));
    testLegacyProjectPath(
        QDir::temp().filePath(QStringLiteral("kfilter_driver_parameter_validation_legacy.kfp")));
    testDriverSlotPath(
        QDir::temp().filePath(QStringLiteral("kfilter_driver_parameter_validation.kfd")));

    if (failures != 0) {
        QTextStream(stderr) << failures << " driver parameter validation check(s) failed\n";
        return 1;
    }

    QTextStream(stdout) << "Driver parameter validation smoke test passed.\n";
    return 0;
}
