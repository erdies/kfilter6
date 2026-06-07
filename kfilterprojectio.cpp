/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterprojectio.h"

#include <QFile>
#include <QLocale>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

namespace
{
constexpr auto CaseInsensitive = Qt::CaseInsensitive;

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
    return ok;
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
            if (!ok) {
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

void writeBool(QTextStream& stream, const char* name, bool value)
{
    stream << '\n' << name << " = " << (value ? 1 : 0);
}
}

bool KFilterProjectIo::loadFromFile(const QString& filePath,
                                    driver (&drivers)[DriverCount],
                                    QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(errorMessage,
                 QStringLiteral("Cannot open project file '%1' for reading: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QStringList lines = stream.readAll().split(QLatin1Char('\n'));

    qsizetype index = 0;
    while (index < lines.size()) {
        QString line;
        if (!readNextDataLine(lines, index, line)) {
            break;
        }

        if (line.contains(QStringLiteral("Network values"), CaseInsensitive)) {
            if (!parseNetworkValues(lines, index, drivers, errorMessage)) {
                return false;
            }
        } else if (line.contains(QStringLiteral("Driver parameters"), CaseInsensitive)) {
            if (!parseDriverParameters(lines, index, drivers, errorMessage)) {
                return false;
            }
        } else if (line.contains(QStringLiteral("Driver enclosure losses"), CaseInsensitive)) {
            if (!parseDriverEnclosureLosses(lines, index, drivers, errorMessage)) {
                return false;
            }
        }
    }

    for (driver& currentDriver : drivers) {
        currentDriver.Berechneparameter();
        currentDriver.setmodified();
    }

    return true;
}

bool KFilterProjectIo::saveToFile(const QString& filePath,
                                  driver (&drivers)[DriverCount],
                                  QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setError(errorMessage,
                 QStringLiteral("Cannot open project file '%1' for writing: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setLocale(QLocale::c());
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(9);

    stream << "# KFilter datafile";
    stream << "\n[Network values]";

    for (int driverIndex = 0; driverIndex < DriverCount; ++driverIndex) {
        stream << "\n# Driver " << (driverIndex + 1);
        for (int unitIndex = 1; unitIndex <= NetworkUnitCount; ++unitIndex) {
            stream << '\n' << qSetFieldWidth(14) << drivers[driverIndex].getUnit(unitIndex) << qSetFieldWidth(0);
        }
    }

    stream.setRealNumberPrecision(6);
    stream << "\n[Driver parameters]";
    for (int driverIndex = 0; driverIndex < DriverCount; ++driverIndex) {
        driver& currentDriver = drivers[driverIndex];
        stream << "\n# Driver " << (driverIndex + 1);
        stream << "\nRdc=" << qSetFieldWidth(10) << currentDriver.getRdc() << qSetFieldWidth(0);
        stream << "\nLsp=" << qSetFieldWidth(10) << currentDriver.getLsp() << qSetFieldWidth(0);
        stream << "\nF0 =" << qSetFieldWidth(10) << currentDriver.getF0() << qSetFieldWidth(0);
        stream << "\nQts=" << qSetFieldWidth(10) << currentDriver.getQtc() << qSetFieldWidth(0);
        stream << "\nQe =" << qSetFieldWidth(10) << currentDriver.getQes() << qSetFieldWidth(0);
        stream << "\nQms=" << qSetFieldWidth(10) << currentDriver.getQms() << qSetFieldWidth(0);
        stream << "\nVas=" << qSetFieldWidth(10) << currentDriver.getVas() << qSetFieldWidth(0);
        stream << "\nDm =" << qSetFieldWidth(10) << currentDriver.getDm() << qSetFieldWidth(0);
        stream << "\nVb =" << qSetFieldWidth(10) << currentDriver.Vb << qSetFieldWidth(0);
        stream << "\nFb =" << qSetFieldWidth(10) << currentDriver.Fb << qSetFieldWidth(0);
        stream << "\nV2 =" << qSetFieldWidth(10) << currentDriver.V2 << qSetFieldWidth(0);
        stream << "\nGTypProposal =" << qSetFieldWidth(3) << currentDriver.GTypProposal << qSetFieldWidth(0);
        stream << "\nGain =" << qSetFieldWidth(10) << currentDriver.gain << qSetFieldWidth(0);

        writeBool(stream, "Pressure", currentDriver.PressureisActive);
        writeBool(stream, "Impedanz", currentDriver.ImpedanzisActive);
        writeBool(stream, "Summary", currentDriver.SummaryisActive);
        writeBool(stream, "ScalarSummary", currentDriver.ScalarSummaryisActive);
        writeBool(stream, "ImpedanzSummary", currentDriver.ImpedanzSummaryisActive);
        writeBool(stream, "InvertPhase", currentDriver.InvertPhase);
        stream << "\nTitle = " << currentDriver.GetTitle();
    }

    stream << "\n[Driver enclosure losses]";
    for (int driverIndex = 0; driverIndex < DriverCount; ++driverIndex) {
        driver& currentDriver = drivers[driverIndex];
        stream << "\n# Driver " << (driverIndex + 1);
        stream << "\nQl =" << qSetFieldWidth(10) << currentDriver.getQl() << qSetFieldWidth(0);
    }

    stream << '\n';
    return true;
}
