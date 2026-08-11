/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleparametersdialog.h"
#include "networkvalueutils.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>

#include <cmath>
#include <iostream>

namespace
{
bool near(double left, double right, double tolerance = 1.0e-6)
{
    return std::abs(left - right) <= tolerance;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    BaffleParametersDialog::BaffleSettingsPerDriver settings{};
    settings[0].enabled = true;
    settings[0].widthMm = 231.0;
    settings[0].showResponseInPlot = true;

    BaffleParametersDialog dialog(settings, nullptr, 2);

    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("baffleDriverTabs"));
    if (tabs == nullptr || tabs->count() != KFilterProjectIo::DriverCount || dialog.currentDriverIndex() != 2) {
        std::cerr << "Baffle driver tabs or initial selection are invalid\n";
        return 1;
    }

    auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("baffleEnableDriver1"));
    auto *model = dialog.findChild<QComboBox *>(QStringLiteral("baffleModelCombo1"));
    auto *width = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleWidthSpin1"));
    auto *height = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleHeightSpin1"));
    auto *driverX = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleDriverXSpin1"));
    auto *driverY = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleDriverYSpin1"));
    auto *midpoint = dialog.findChild<QLabel *>(QStringLiteral("baffleMidpointLabel1"));
    auto *showResponse = dialog.findChild<QCheckBox *>(QStringLiteral("baffleShowResponseDriver1"));
    auto *status = dialog.findChild<QLabel *>(QStringLiteral("baffleResponseStatus1"));
    if (enabled == nullptr || model == nullptr || width == nullptr || height == nullptr ||
        driverX == nullptr || driverY == nullptr || midpoint == nullptr ||
        showResponse == nullptr || status == nullptr) {
        std::cerr << "Baffle controls are missing\n";
        return 2;
    }

    if (!enabled->isChecked() || model->count() != 2 ||
        model->currentData().toInt() != static_cast<int>(BaffleModel::SimpleBaffleStep) ||
        !near(width->value(), 231.0) || !showResponse->isChecked() ||
        !midpoint->text().contains(QStringLiteral("497")) ||
        !status->text().contains(QStringLiteral("valid complex Simple")) ||
        height->isEnabled() || driverX->isEnabled() || driverY->isEnabled()) {
        std::cerr << "Persisted Stage-1 state was not loaded into the dialog correctly\n";
        return 3;
    }

    int previewCount = 0;
    int applyCount = 0;
    QObject::connect(&dialog, &BaffleParametersDialog::parametersPreviewChanged,
                     [&previewCount]() { ++previewCount; });
    QObject::connect(&dialog, &BaffleParametersDialog::parametersApplied,
                     [&applyCount]() { ++applyCount; });

    width->setValue(300.0);
    QApplication::processEvents();
    if (!near(settings[0].widthMm, 300.0) || previewCount == 0 ||
        !midpoint->text().contains(QStringLiteral("383"))) {
        std::cerr << "Baffle width change was not mirrored into the live preview model\n";
        return 4;
    }

    // Patch 195/197: Baffle geometry entry must match the other numeric
    // dialogs: accept decimal comma as well as decimal point, including the
    // displayed unit suffix. Mixed separators are a parser-level contract;
    // test that directly rather than relying on QDoubleSpinBox correction of
    // programmatically injected invalid QLineEdit text.
    auto *widthEditor = width->findChild<QLineEdit *>();
    if (widthEditor == nullptr) {
        std::cerr << "Baffle width editor is missing\n";
        return 14;
    }

    widthEditor->setText(QStringLiteral("231,5 mm"));
    width->interpretText();
    QApplication::processEvents();
    if (!near(width->value(), 231.5) || !near(settings[0].widthMm, 231.5)) {
        std::cerr << "Decimal-comma Baffle input was not accepted\n";
        return 15;
    }

    widthEditor->setText(QStringLiteral("232.5 mm"));
    width->interpretText();
    QApplication::processEvents();
    if (!near(width->value(), 232.5) || !near(settings[0].widthMm, 232.5)) {
        std::cerr << "Decimal-point Baffle input regressed\n";
        return 16;
    }

    double mixedSeparatorValue = 0.0;
    if (NetworkValueUtils::parseDisplayValue(QStringLiteral("23,1.5"),
                                             mixedSeparatorValue,
                                             0.0,
                                             10000.0)) {
        std::cerr << "Mixed decimal separators were accepted by the shared parser\n";
        return 17;
    }

    // Patch 192: Stage 2 is selectable and exposes only the minimum geometry
    // fields required by the productive rectangular DSP.
    const int rectangularIndex = model->findData(static_cast<int>(BaffleModel::RectangularEdgeDiffraction));
    if (rectangularIndex < 0) {
        std::cerr << "Rectangular Edge Diffraction model is missing\n";
        return 5;
    }
    model->setCurrentIndex(rectangularIndex);
    QApplication::processEvents();
    if (settings[0].model != BaffleModel::RectangularEdgeDiffraction ||
        !height->isEnabled() || !driverX->isEnabled() || !driverY->isEnabled() ||
        !midpoint->text().contains(QStringLiteral("not applicable")) ||
        !status->text().contains(QStringLiteral("invalid geometry"))) {
        std::cerr << "Stage-2 selection or initial invalid-geometry state is inconsistent\n";
        return 6;
    }

    width->setValue(231.0);
    height->setValue(900.0);
    driverX->setValue(90.0);
    driverY->setValue(310.0);
    QApplication::processEvents();
    if (!near(settings[0].widthMm, 231.0) || !near(settings[0].heightMm, 900.0) ||
        !near(settings[0].driverXmm, 90.0) || !near(settings[0].driverYmm, 310.0) ||
        !status->text().contains(QStringLiteral("valid complex Rectangular"))) {
        std::cerr << "Valid Rectangular Edge Diffraction geometry was not previewed\n";
        return 7;
    }

    showResponse->setChecked(false);
    QApplication::processEvents();
    if (settings[0].showResponseInPlot || previewCount < 6) {
        std::cerr << "Diagnostic visibility did not update the live preview model\n";
        return 8;
    }

    enabled->setChecked(false);
    QApplication::processEvents();
    if (settings[0].enabled || width->isEnabled() || height->isEnabled() ||
        driverX->isEnabled() || driverY->isEnabled() || model->isEnabled() ||
        !status->text().contains(QStringLiteral("bypassed"))) {
        std::cerr << "Baffle master bypass state is inconsistent\n";
        return 9;
    }

    enabled->setChecked(true);
    showResponse->setChecked(true);
    QApplication::processEvents();

    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("baffleDialogButtons"));
    if (buttons == nullptr || buttons->button(QDialogButtonBox::Apply) == nullptr) {
        std::cerr << "Baffle Apply button is missing\n";
        return 10;
    }
    buttons->button(QDialogButtonBox::Apply)->click();
    if (applyCount != 1 || !settings[0].enabled ||
        settings[0].model != BaffleModel::RectangularEdgeDiffraction ||
        !near(settings[0].widthMm, 231.0) || !near(settings[0].heightMm, 900.0) ||
        !near(settings[0].driverXmm, 90.0) || !near(settings[0].driverYmm, 310.0) ||
        !settings[0].showResponseInPlot) {
        std::cerr << "Apply did not commit the Stage-2 Baffle working copy\n";
        return 11;
    }

    height->setValue(700.0);
    driverY->setValue(250.0);
    showResponse->setChecked(false);
    QApplication::processEvents();
    if (!near(settings[0].heightMm, 700.0) || !near(settings[0].driverYmm, 250.0) ||
        settings[0].showResponseInPlot) {
        std::cerr << "Post-Apply Stage-2 changes were not previewed\n";
        return 12;
    }

    dialog.reject();
    if (!settings[0].enabled ||
        settings[0].model != BaffleModel::RectangularEdgeDiffraction ||
        !near(settings[0].widthMm, 231.0) || !near(settings[0].heightMm, 900.0) ||
        !near(settings[0].driverXmm, 90.0) || !near(settings[0].driverYmm, 310.0) ||
        !settings[0].showResponseInPlot) {
        std::cerr << "Cancel did not restore the last applied Stage-2 Baffle state\n";
        return 13;
    }

    std::cout << "Baffle / Diffraction Stage-1/Stage-2 dialog smoke test passed\n";
    return 0;
}
