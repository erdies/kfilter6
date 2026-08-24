/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "driverparametersdialog.h"
#include "kfilterdoc.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTabWidget>

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>

namespace
{
struct ExpectedState
{
    bool vb = false;
    bool ql = false;
    bool fb = false;
    bool tube = false;
    bool v2 = false;
};

bool near(double left, double right, double tolerance)
{
    return std::abs(left - right) <= tolerance;
}

double expectedTubeLengthCm(double diameterCm, double fbHz, double vbLiter)
{
    constexpr double pi = 3.14159265358979323846;
    const double tubeAreaCm2 = pi * std::pow(diameterCm * 0.5, 2.0);
    return 29830.0 * tubeAreaCm2 / (fbHz * fbHz * vbLiter) -
           0.825 * std::sqrt(tubeAreaCm2);
}

double expectedTubeDiameterCm(double lengthCm, double fbHz, double vbLiter)
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double endCorrectionFactor = 0.825;
    const double areaCoefficient = 29830.0 / (fbHz * fbHz * vbLiter);
    const double areaRoot = (endCorrectionFactor +
                             std::sqrt(endCorrectionFactor * endCorrectionFactor +
                                       4.0 * areaCoefficient * lengthCm)) /
                            (2.0 * areaCoefficient);
    return 2.0 * areaRoot / std::sqrt(pi);
}

bool verifyDriverPage(DriverParametersDialog& dialog,
                      int driverIndex,
                      EnclosureType expectedType,
                      const ExpectedState& expected)
{
    const QString suffix = QString::number(driverIndex + 1);
    auto *enclosure = dialog.findChild<QComboBox *>(
        QStringLiteral("driverEnclosureTypeCombo") + suffix);
    auto *vb = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverVbSpin") + suffix);
    auto *ql = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverQlSpin") + suffix);
    auto *fb = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverFbSpin") + suffix);
    auto *hoge = dialog.findChild<QPushButton *>(QStringLiteral("driverHogeButton") + suffix);
    auto *tubeDiameter = dialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("driverTubeDiameterSpin") + suffix);
    auto *tubeLength = dialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("driverTubeLengthSpin") + suffix);
    auto *v2 = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverV2Spin") + suffix);
    auto *gain = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverGainSpin") + suffix);

    if (enclosure == nullptr || vb == nullptr || ql == nullptr || fb == nullptr ||
        hoge == nullptr || tubeDiameter == nullptr || tubeLength == nullptr ||
        v2 == nullptr || gain == nullptr) {
        std::cerr << "Driver Parameters enclosure controls are missing for Driver "
                  << driverIndex + 1 << '\n';
        return false;
    }

    if (enclosure->currentData().toInt() != static_cast<int>(expectedType) ||
        vb->isEnabled() != expected.vb ||
        ql->isEnabled() != expected.ql ||
        fb->isEnabled() != expected.fb ||
        hoge->isEnabled() != expected.fb ||
        tubeDiameter->isEnabled() != expected.tube ||
        tubeLength->isEnabled() != expected.tube ||
        v2->isEnabled() != expected.v2 ||
        !gain->isEnabled()) {
        std::cerr << "Unexpected enclosure-control state for Driver "
                  << driverIndex + 1 << '\n';
        return false;
    }

    return true;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KFilterDoc document;
    const std::array<EnclosureType, KFilterProjectIo::DriverCount> enclosureTypes{
        EnclosureType::OpenBaffle,
        EnclosureType::Sealed,
        EnclosureType::Vented,
        EnclosureType::Bandpass
    };

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        driver& drv = document.m_driverDriver[index];
        drv.setVb(25.0 + index);
        drv.setQl(7.0 + index);
        drv.setFb(35.0 + index);
        drv.setV2(10.0 + index);
        drv.setEnclosureTypeProposal(enclosureTypes.at(static_cast<std::size_t>(index)));
    }

    DriverParametersDialog dialog(document);
    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("driverParametersTabs"));
    if (tabs == nullptr || tabs->count() != KFilterProjectIo::DriverCount) {
        std::cerr << "Driver Parameters tabs are missing or incomplete\n";
        return 1;
    }

    const std::array<ExpectedState, KFilterProjectIo::DriverCount> expectedStates{
        ExpectedState{false, false, false, false, false},
        ExpectedState{true, true, false, false, false},
        ExpectedState{true, true, true, true, false},
        ExpectedState{true, true, true, true, true}
    };

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        if (!verifyDriverPage(dialog,
                              index,
                              enclosureTypes.at(static_cast<std::size_t>(index)),
                              expectedStates.at(static_cast<std::size_t>(index)))) {
            return 2;
        }
    }

    // Exercise live transitions on one page and prove that disabled fields keep
    // their values so switching enclosure type is non-destructive.
    auto *enclosure = dialog.findChild<QComboBox *>(QStringLiteral("driverEnclosureTypeCombo2"));
    auto *fb = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverFbSpin2"));
    auto *tubeDiameter = dialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("driverTubeDiameterSpin2"));
    auto *tubeLength = dialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("driverTubeLengthSpin2"));
    auto *v2 = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverV2Spin2"));
    auto *vb = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("driverVbSpin2"));
    if (enclosure == nullptr || fb == nullptr || tubeDiameter == nullptr ||
        tubeLength == nullptr || v2 == nullptr || vb == nullptr) {
        std::cerr << "Driver 2 transition controls are missing\n";
        return 3;
    }

    fb->setValue(47.0);
    tubeDiameter->setValue(8.5);
    v2->setValue(12.0);
    const double tubeLengthBeforeTransitions = tubeLength->value();

    const auto selectType = [enclosure](EnclosureType type) {
        const int itemIndex = enclosure->findData(static_cast<int>(type));
        if (itemIndex >= 0) {
            enclosure->setCurrentIndex(itemIndex);
            QApplication::processEvents();
        }
        return itemIndex >= 0;
    };

    if (!selectType(EnclosureType::OpenBaffle) ||
        !verifyDriverPage(dialog, 1, EnclosureType::OpenBaffle,
                          ExpectedState{false, false, false, false, false}) ||
        !selectType(EnclosureType::Vented) ||
        !verifyDriverPage(dialog, 1, EnclosureType::Vented,
                          ExpectedState{true, true, true, true, false}) ||
        !selectType(EnclosureType::Bandpass) ||
        !verifyDriverPage(dialog, 1, EnclosureType::Bandpass,
                          ExpectedState{true, true, true, true, true})) {
        std::cerr << "Live enclosure-type transitions are inconsistent\n";
        return 4;
    }

    if (fb->value() != 47.0 || tubeDiameter->value() != 8.5 ||
        tubeLength->value() != tubeLengthBeforeTransitions || v2->value() != 12.0) {
        std::cerr << "Enclosure-type transitions discarded inactive field values\n";
        return 5;
    }

    // Patch 290: diameter and length are reciprocal editors. Vb/Fb continue to
    // define the alignment, while only the diameter remains persisted.
    int diameterChangeCount = 0;
    int lengthChangeCount = 0;
    QObject::connect(tubeDiameter,
                     static_cast<void (QDoubleSpinBox::*)(double)>(
                         &QDoubleSpinBox::valueChanged),
                     [&diameterChangeCount](double) { ++diameterChangeCount; });
    QObject::connect(tubeLength,
                     static_cast<void (QDoubleSpinBox::*)(double)>(
                         &QDoubleSpinBox::valueChanged),
                     [&lengthChangeCount](double) { ++lengthChangeCount; });

    vb->setValue(30.0);
    fb->setValue(47.0);
    tubeDiameter->setValue(10.0);
    QApplication::processEvents();
    const double forwardLength = expectedTubeLengthCm(10.0, 47.0, 30.0);
    if (!near(tubeLength->value(), forwardLength, 0.051) ||
        diameterChangeCount != 1 || lengthChangeCount != 0) {
        std::cerr << "Tube diameter did not recalculate tube length\n";
        return 6;
    }

    diameterChangeCount = 0;
    lengthChangeCount = 0;
    constexpr double RequestedLengthCm = 35.0;
    tubeLength->setValue(RequestedLengthCm);
    QApplication::processEvents();
    const double inverseDiameter = expectedTubeDiameterCm(RequestedLengthCm, 47.0, 30.0);
    if (!near(tubeDiameter->value(), inverseDiameter, 0.001) ||
        !near(tubeLength->value(), RequestedLengthCm, 0.051) ||
        diameterChangeCount != 0 || lengthChangeCount != 1) {
        std::cerr << "Tube length did not recalculate a consistent tube diameter\n";
        return 7;
    }

    const double diameterBeforeInvalidInput = tubeDiameter->value();
    fb->setValue(0.0);
    tubeLength->setValue(25.0);
    QApplication::processEvents();
    if (!near(tubeDiameter->value(), diameterBeforeInvalidInput, 0.0001) ||
        tubeLength->value() != 0.0) {
        std::cerr << "Invalid Vb/Fb state changed the tube diameter or retained a false length\n";
        return 8;
    }

    std::cout << "Driver Parameters enclosure-control state smoke test passed\n";
    return 0;
}
