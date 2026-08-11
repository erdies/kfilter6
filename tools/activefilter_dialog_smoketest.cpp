/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "activefilterparametersdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>

#include <cmath>
#include <iostream>
#include <variant>

namespace
{
bool near(double left, double right)
{
    return std::abs(left - right) < 1.0e-6;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    ActiveFilterParametersDialog::ActiveFilterChains chains;

    // Patch 187: Gain, Delay and Polarity are all supported sections and the
    // existing type-specific editor controls must load their persisted values.
    ActiveFilterChain& driver1 = chains.at(0);
    driver1.setEnabled(true);
    driver1.addSection(ActiveFilterType::Gain);
    std::get<ActiveFilterGainParameters>(driver1.section(0).parameters()).gainDb = -3.5;
    driver1.addSection(ActiveFilterType::Delay);
    std::get<ActiveFilterDelayParameters>(driver1.section(1).parameters()).delayMs = 0.625;
    driver1.addSection(ActiveFilterType::Polarity);
    std::get<ActiveFilterPolarityParameters>(driver1.section(2).parameters()).inverted = true;

    // Patch 188: AP1 uses Frequency only, AP2 uses Frequency + Q.
    ActiveFilterChain& driver2 = chains.at(1);
    driver2.setEnabled(true);
    driver2.addSection(ActiveFilterType::AllPass);
    auto& ap1 = std::get<ActiveFilterAllPassParameters>(driver2.section(0).parameters());
    ap1.order = 1;
    ap1.frequencyHz = 1200.0;
    ap1.q = 4.0; // persisted metadata; deliberately unused by AP1
    driver2.addSection(ActiveFilterType::AllPass);
    auto& ap2 = std::get<ActiveFilterAllPassParameters>(driver2.section(1).parameters());
    ap2.order = 2;
    ap2.frequencyHz = 2400.0;
    ap2.q = 1.25;

    ActiveFilterChain& driver3 = chains.at(2);
    driver3.setEnabled(true);
    driver3.setShowResponseInPlot(true);

    driver3.addSection(ActiveFilterType::HighPass);
    auto& highPass = std::get<ActiveFilterHighPassParameters>(driver3.section(0).parameters());
    highPass.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
    highPass.order = 4;
    highPass.frequencyHz = 80.0;

    driver3.addSection(ActiveFilterType::Notch);
    auto& notch = std::get<ActiveFilterNotchParameters>(driver3.section(1).parameters());
    notch.centerFrequencyHz = 4200.0;
    notch.q = 3.5;
    notch.gainDb = -6.0; // persisted reserved metadata; not used by Patch-182 transfer

    ActiveFilterChain& driver4 = chains.at(3);
    driver4.setEnabled(true);
    driver4.setShowResponseInPlot(true);
    driver4.addSection(ActiveFilterType::BandPass);
    auto& driver4BandPass =
        std::get<ActiveFilterBandPassParameters>(driver4.section(0).parameters());
    driver4BandPass.characteristic = ActiveFilterCharacteristic::Butterworth;
    driver4BandPass.order = 3;
    driver4BandPass.lowerFrequencyHz = 300.0;
    driver4BandPass.upperFrequencyHz = 3200.0;

    driver4.addSection(ActiveFilterType::Notch);
    auto& driver4Notch =
        std::get<ActiveFilterNotchParameters>(driver4.section(1).parameters());
    driver4Notch.centerFrequencyHz = 2500.0;
    driver4Notch.q = 5.0;

    ActiveFilterParametersDialog dialog(chains, nullptr, 2);

    if (dialog.size().height() < 800) {
        std::cerr << "active-filter dialog initial height is too small\n";
        return 1;
    }

    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("activeFilterDriverTabs"));
    auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("activeFilterEnableDriver3"));
    auto *showResponse = dialog.findChild<QCheckBox *>(QStringLiteral("activeFilterShowResponse3"));
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("activeFilterSectionTable3"));
    auto *responseStatus = dialog.findChild<QLabel *>(QStringLiteral("activeFilterResponseStatus3"));
    if (tabs == nullptr || tabs->count() != KFilterProjectIo::DriverCount || tabs->currentIndex() != 2 ||
        enabled == nullptr || !enabled->isChecked() ||
        showResponse == nullptr || !showResponse->isChecked() ||
        table == nullptr || table->rowCount() != 2 ||
        responseStatus == nullptr || !responseStatus->text().contains(QStringLiteral("valid and applied"))) {
        std::cerr << "active-filter model was not loaded into the dialog or LR4 was not recognized as supported\n";
        return 1;
    }

    if (table->item(0, 1) == nullptr || table->item(0, 1)->text() != QStringLiteral("High-pass") ||
        table->item(1, 1) == nullptr || table->item(1, 1)->text() != QStringLiteral("Notch")) {
        std::cerr << "active-filter table does not reflect model ordering/types\n";
        return 2;
    }

    auto *driver1Status = dialog.findChild<QLabel *>(QStringLiteral("activeFilterResponseStatus1"));
    auto *driver1Table = dialog.findChild<QTableWidget *>(QStringLiteral("activeFilterSectionTable1"));
    auto *driver1Characteristic = dialog.findChild<QComboBox *>(QStringLiteral("activeFilterCharacteristicCombo1"));
    auto *driver1Order = dialog.findChild<QSpinBox *>(QStringLiteral("activeFilterOrderSpin1"));
    auto *driver1Frequency1 = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterFrequency1Spin1"));
    auto *driver1Q = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterQSpin1"));
    auto *driver1Gain = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterGainSpin1"));
    auto *driver1Delay = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterDelaySpin1"));
    auto *driver1Polarity = dialog.findChild<QComboBox *>(QStringLiteral("activeFilterPolarityCombo1"));
    if (driver1Status == nullptr || !driver1Status->text().contains(QStringLiteral("valid and applied")) ||
        driver1Table == nullptr || driver1Table->rowCount() != 3 ||
        driver1Characteristic == nullptr || driver1Order == nullptr || driver1Frequency1 == nullptr ||
        driver1Q == nullptr || driver1Gain == nullptr || driver1Delay == nullptr || driver1Polarity == nullptr) {
        std::cerr << "Gain/Delay/Polarity chain was not loaded as a supported active-filter chain\n";
        return 2;
    }

    driver1Table->selectRow(0);
    driver1Table->setCurrentCell(0, 1);
    QApplication::processEvents();
    if (!driver1Gain->isEnabled() || driver1Delay->isEnabled() || driver1Polarity->isEnabled() ||
        driver1Characteristic->isEnabled() || driver1Order->isEnabled() ||
        driver1Frequency1->isEnabled() || driver1Q->isEnabled() ||
        !near(driver1Gain->value(), -3.5)) {
        std::cerr << "Gain editor does not expose exactly the Gain control\n";
        return 2;
    }

    driver1Table->selectRow(1);
    driver1Table->setCurrentCell(1, 1);
    QApplication::processEvents();
    if (driver1Gain->isEnabled() || !driver1Delay->isEnabled() || driver1Polarity->isEnabled() ||
        driver1Characteristic->isEnabled() || driver1Order->isEnabled() ||
        driver1Frequency1->isEnabled() || driver1Q->isEnabled() ||
        !near(driver1Delay->value(), 0.625)) {
        std::cerr << "Delay editor does not expose exactly the Delay control\n";
        return 2;
    }

    driver1Table->selectRow(2);
    driver1Table->setCurrentCell(2, 1);
    QApplication::processEvents();
    if (driver1Gain->isEnabled() || driver1Delay->isEnabled() || !driver1Polarity->isEnabled() ||
        driver1Characteristic->isEnabled() || driver1Order->isEnabled() ||
        driver1Frequency1->isEnabled() || driver1Q->isEnabled() ||
        !driver1Polarity->currentData().toBool()) {
        std::cerr << "Polarity editor does not expose exactly the Polarity control\n";
        return 2;
    }

    auto *driver2Status = dialog.findChild<QLabel *>(QStringLiteral("activeFilterResponseStatus2"));
    auto *driver2Table = dialog.findChild<QTableWidget *>(QStringLiteral("activeFilterSectionTable2"));
    auto *driver2Characteristic = dialog.findChild<QComboBox *>(QStringLiteral("activeFilterCharacteristicCombo2"));
    auto *driver2Order = dialog.findChild<QSpinBox *>(QStringLiteral("activeFilterOrderSpin2"));
    auto *driver2Frequency1 = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterFrequency1Spin2"));
    auto *driver2Q = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterQSpin2"));
    auto *driver2Gain = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterGainSpin2"));
    auto *driver2Delay = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterDelaySpin2"));
    auto *driver2Polarity = dialog.findChild<QComboBox *>(QStringLiteral("activeFilterPolarityCombo2"));
    if (driver2Status == nullptr || !driver2Status->text().contains(QStringLiteral("valid and applied")) ||
        driver2Table == nullptr || driver2Table->rowCount() != 2 ||
        driver2Characteristic == nullptr || driver2Order == nullptr || driver2Frequency1 == nullptr ||
        driver2Q == nullptr || driver2Gain == nullptr || driver2Delay == nullptr || driver2Polarity == nullptr) {
        std::cerr << "AP1/AP2 chain was not loaded as a supported active-filter chain\n";
        return 2;
    }

    driver2Table->selectRow(0);
    driver2Table->setCurrentCell(0, 1);
    QApplication::processEvents();
    if (driver2Characteristic->isEnabled() || !driver2Order->isEnabled() ||
        !driver2Frequency1->isEnabled() || driver2Q->isEnabled() ||
        driver2Gain->isEnabled() || driver2Delay->isEnabled() || driver2Polarity->isEnabled() ||
        driver2Order->value() != 1 || !near(driver2Frequency1->value(), 1200.0) ||
        !driver2Order->toolTip().contains(QStringLiteral("AP1")) ||
        !driver2Order->toolTip().contains(QStringLiteral("AP2"))) {
        std::cerr << "AP1 editor must expose Order/Frequency and keep Q disabled\n";
        return 2;
    }

    driver2Table->selectRow(1);
    driver2Table->setCurrentCell(1, 1);
    QApplication::processEvents();
    if (driver2Order->value() != 2 || !near(driver2Frequency1->value(), 2400.0) ||
        !driver2Q->isEnabled() || !near(driver2Q->value(), 1.25)) {
        std::cerr << "AP2 editor must expose Frequency and Q\n";
        return 2;
    }

    driver2Order->setValue(1);
    QApplication::processEvents();
    if (driver2Q->isEnabled() ||
        std::get<ActiveFilterAllPassParameters>(driver2.section(1).parameters()).order != 1) {
        std::cerr << "changing AP2 to AP1 must disable Q immediately\n";
        return 2;
    }
    driver2Order->setValue(2);
    QApplication::processEvents();
    if (!driver2Q->isEnabled()) {
        std::cerr << "changing AP1 back to AP2 must re-enable Q immediately\n";
        return 2;
    }

    auto *driver4Status = dialog.findChild<QLabel *>(QStringLiteral("activeFilterResponseStatus4"));
    if (driver4Status == nullptr || !driver4Status->text().contains(QStringLiteral("valid and applied"))) {
        std::cerr << "supported Band-pass/Notch chain was not reported as valid\n";
        return 2;
    }

    auto *driver4Table = dialog.findChild<QTableWidget *>(QStringLiteral("activeFilterSectionTable4"));
    auto *driver4Characteristic = dialog.findChild<QComboBox *>(QStringLiteral("activeFilterCharacteristicCombo4"));
    auto *driver4Order = dialog.findChild<QSpinBox *>(QStringLiteral("activeFilterOrderSpin4"));
    auto *driver4Frequency1 = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterFrequency1Spin4"));
    auto *driver4Frequency2 = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterFrequency2Spin4"));
    auto *driver4Q = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterQSpin4"));
    if (driver4Table == nullptr || driver4Table->rowCount() != 2 ||
        driver4Characteristic == nullptr || driver4Order == nullptr ||
        driver4Frequency1 == nullptr || driver4Frequency2 == nullptr || driver4Q == nullptr) {
        std::cerr << "Band-pass dialog controls are missing\n";
        return 3;
    }
    driver4Table->selectRow(0);
    driver4Table->setCurrentCell(0, 1);
    QApplication::processEvents();
    if (!driver4Characteristic->isEnabled() || !driver4Order->isEnabled() ||
        !driver4Frequency1->isEnabled() || !driver4Frequency2->isEnabled() ||
        driver4Q->isEnabled() || driver4Order->value() != 3 ||
        !near(driver4Frequency1->value(), 300.0) ||
        !near(driver4Frequency2->value(), 3200.0)) {
        std::cerr << "Butterworth Band-pass editor does not expose lower/upper cutoff and order correctly\n";
        return 3;
    }

    table->selectRow(0);
    table->setCurrentCell(0, 1);
    QApplication::processEvents();

    auto *characteristic = dialog.findChild<QComboBox *>(QStringLiteral("activeFilterCharacteristicCombo3"));
    auto *order = dialog.findChild<QSpinBox *>(QStringLiteral("activeFilterOrderSpin3"));
    auto *frequency1 = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterFrequency1Spin3"));
    auto *q = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterQSpin3"));
    auto *gain = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("activeFilterGainSpin3"));
    if (characteristic == nullptr || order == nullptr || frequency1 == nullptr || q == nullptr || gain == nullptr ||
        characteristic->currentData().toInt() != static_cast<int>(ActiveFilterCharacteristic::LinkwitzRiley) ||
        order->value() != 4 || !near(frequency1->value(), 80.0) ||
        !characteristic->isEnabled() || !order->isEnabled() || !frequency1->isEnabled() || q->isEnabled() ||
        !order->toolTip().contains(QStringLiteral("LR2")) ||
        !order->toolTip().contains(QStringLiteral("LR4"))) {
        std::cerr << "Linkwitz-Riley editor state was not loaded correctly\n";
        return 3;
    }

    table->selectRow(1);
    table->setCurrentCell(1, 1);
    QApplication::processEvents();
    if (!near(frequency1->value(), 4200.0) || !near(q->value(), 3.5) ||
        characteristic->isEnabled() || order->isEnabled() || gain->isEnabled() ||
        !frequency1->isEnabled() || !q->isEnabled()) {
        std::cerr << "Notch editor does not expose exactly center frequency and Q\n";
        return 3;
    }

    auto *addButton = dialog.findChild<QPushButton *>(QStringLiteral("activeFilterAddButton3"));
    auto *moveUpButton = dialog.findChild<QPushButton *>(QStringLiteral("activeFilterMoveUpButton3"));
    auto *removeButton = dialog.findChild<QPushButton *>(QStringLiteral("activeFilterRemoveButton3"));
    auto *type = dialog.findChild<QComboBox *>(QStringLiteral("activeFilterTypeCombo3"));
    if (addButton == nullptr || moveUpButton == nullptr || removeButton == nullptr ||
        type == nullptr || gain == nullptr) {
        std::cerr << "active-filter editing controls are missing\n";
        return 4;
    }

    int previewSignalCount = 0;
    QObject::connect(&dialog, &ActiveFilterParametersDialog::parametersPreviewChanged,
                     [&previewSignalCount]() { ++previewSignalCount; });

    showResponse->setChecked(false);
    QApplication::processEvents();
    if (driver3.showResponseInPlot() || previewSignalCount == 0) {
        std::cerr << "show-response checkbox did not update the live preview model\n";
        return 5;
    }
    showResponse->setChecked(true);
    QApplication::processEvents();
    if (!driver3.showResponseInPlot()) {
        std::cerr << "show-response checkbox did not restore the live preview state\n";
        return 5;
    }

    addButton->click();
    if (table->rowCount() != 3 || driver3.sectionCount() != 3 || previewSignalCount == 0) {
        std::cerr << "dialog change was not mirrored into the live preview model\n";
        return 5;
    }

    type->setCurrentIndex(type->findData(static_cast<int>(ActiveFilterType::Gain)));
    gain->setValue(-2.5);
    QApplication::processEvents();
    if (table->item(2, 1)->text() != QStringLiteral("Gain")) {
        std::cerr << "GUI type change did not update the working model/table\n";
        return 6;
    }

    moveUpButton->click();
    QApplication::processEvents();
    if (table->item(1, 1)->text() != QStringLiteral("Gain") ||
        table->item(2, 1)->text() != QStringLiteral("Notch")) {
        std::cerr << "GUI move operation did not reorder the working model\n";
        return 7;
    }

    auto *buttonBox = dialog.findChild<QDialogButtonBox *>(QStringLiteral("activeFilterDialogButtons"));
    if (buttonBox == nullptr || buttonBox->button(QDialogButtonBox::Apply) == nullptr) {
        std::cerr << "active-filter Apply button is missing\n";
        return 8;
    }

    buttonBox->button(QDialogButtonBox::Apply)->click();
    if (driver3.sectionCount() != 3 ||
        driver3.section(0).type() != ActiveFilterType::HighPass ||
        driver3.section(1).type() != ActiveFilterType::Gain ||
        driver3.section(2).type() != ActiveFilterType::Notch ||
        !near(std::get<ActiveFilterGainParameters>(driver3.section(1).parameters()).gainDb, -2.5)) {
        std::cerr << "Apply did not commit the dialog working copy to the model\n";
        return 9;
    }

    // Verify live-preview Apply/Cancel semantics: edits after Apply are previewed
    // immediately, but rejecting the dialog restores the last applied model.
    removeButton->click();
    if (table->rowCount() != 2 || driver3.sectionCount() != 2) {
        std::cerr << "post-Apply edit was not mirrored into the live preview model\n";
        return 10;
    }
    dialog.reject();
    if (driver3.sectionCount() != 3 || driver3.section(1).type() != ActiveFilterType::Gain) {
        std::cerr << "Cancel did not restore the last applied active-filter model\n";
        return 11;
    }

    std::cout << "active-filter model/dialog round-trip smoke test passed\n";
    return 0;
}
