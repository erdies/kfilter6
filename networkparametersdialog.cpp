/***************************************************************************
                          networkparametersdialog.cpp  -  Qt6 network editor
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "networkparametersdialog.h"

#include "driver.h"
#include "networkvalueutils.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>


#include <array>
#include <cmath>

namespace
{
constexpr int NetworkSectionCount = 8;
constexpr int NetworkRowsPerSection = 6;

QStringList rowLabels()
{
    return {
        QObject::tr("Series R [Ohm]"),
        QObject::tr("Series C [uF]"),
        QObject::tr("Series L [mH]"),
        QObject::tr("Shunt R [Ohm]"),
        QObject::tr("Shunt C [uF]"),
        QObject::tr("Shunt L [mH]")
    };
}

QStringList columnLabels()
{
    QStringList labels;
    for (int column = 0; column < NetworkSectionCount; ++column) {
        labels << QObject::tr("Section %1").arg(column + 1);
    }
    return labels;
}

QString displayNumber(double value)
{
    return NetworkValueUtils::displayNumber(value, 8);
}

QString orderLabel(int order)
{
    switch (order) {
    case 1:
        return QObject::tr("1st order");
    case 2:
        return QObject::tr("2nd order");
    case 3:
        return QObject::tr("3rd order");
    case 4:
        return QObject::tr("4th order");
    default:
        return QObject::tr("%1th order").arg(order);
    }
}

int requiredSectionCountForOrder(int order)
{
    return (order + 1) / 2;
}

enum PresetFilterType
{
    PresetLowPass = 0,
    PresetHighPass = 1
};

enum NetworkRow
{
    RowSeriesR = 0,
    RowSeriesC = 1,
    RowSeriesL = 2,
    RowShuntR = 3,
    RowShuntC = 4,
    RowShuntL = 5
};

constexpr double Pi = 3.14159265358979323846;

std::array<double, 4> butterworthSinglyTerminatedLowPassCoefficients(int order)
{
    // Normalized passive Butterworth ladder coefficients for an ideal voltage source
    // and a resistive load. The topology starts with a series element and alternates
    // between series and shunt elements. Unused entries stay at zero.
    switch (order) {
    case 1:
        return {1.0, 0.0, 0.0, 0.0};
    case 2:
        return {std::sqrt(2.0), 1.0 / std::sqrt(2.0), 0.0, 0.0};
    case 3:
        return {1.5, 4.0 / 3.0, 0.5, 0.0};
    case 4:
        return {1.53073372946036, 1.57716101494948, 1.08239220029239, 0.38268343236509};
    default:
        return {0.0, 0.0, 0.0, 0.0};
    }
}
}

NetworkParametersDialog::NetworkParametersDialog(driver (&drivers)[KFilterProjectIo::DriverCount],
                                                   QWidget *parent,
                                                   int initialDriverIndex)
    : QDialog(parent),
      m_drivers(drivers)
{
    setWindowTitle(tr("Network / Filter Parameters"));
    resize(900, 560);

    auto *mainLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        tabs->addTab(createDriverPage(index), tr("Driver %1").arg(index + 1));
    }
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index >= 0 && index < KFilterProjectIo::DriverCount) {
            m_currentDriverIndex = index;
        }
    });

    const int safeInitialDriverIndex = (initialDriverIndex >= 0 && initialDriverIndex < KFilterProjectIo::DriverCount)
                                           ? initialDriverIndex
                                           : 0;
    tabs->setCurrentIndex(safeInitialDriverIndex);
    m_currentDriverIndex = tabs->currentIndex();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Apply |
                                           QDialogButtonBox::Cancel,
                                           this);
    auto *resetCurrentButton = buttonBox->addButton(tr("Reset Current Driver"), QDialogButtonBox::ResetRole);
    auto *resetAllButton = buttonBox->addButton(tr("Reset All Drivers"), QDialogButtonBox::ResetRole);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &NetworkParametersDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &NetworkParametersDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &NetworkParametersDialog::applyClicked);
    connect(resetCurrentButton, &QPushButton::clicked, this, &NetworkParametersDialog::resetCurrentDriver);
    connect(resetAllButton, &QPushButton::clicked, this, &NetworkParametersDialog::resetAllDrivers);

    mainLayout->addWidget(tabs);
    mainLayout->addWidget(buttonBox);

    loadFromDrivers();
}

int NetworkParametersDialog::currentDriverIndex() const
{
    return m_currentDriverIndex;
}

QWidget *NetworkParametersDialog::createDriverPage(int index)
{
    DriverPage& page = m_pages.at(index);
    page.page = new QWidget(this);

    auto *presetGroup = new QGroupBox(tr("Standard Filter Preset"), page.page);
    auto *presetLayout = new QGridLayout(presetGroup);

    page.presetTypeCombo = new QComboBox(presetGroup);
    page.presetTypeCombo->addItem(tr("Low-pass"), PresetLowPass);
    page.presetTypeCombo->addItem(tr("High-pass"), PresetHighPass);

    page.presetOrderCombo = new QComboBox(presetGroup);
    for (int order = 1; order <= 4; ++order) {
        page.presetOrderCombo->addItem(orderLabel(order), order);
    }

    page.presetCharacteristicCombo = new QComboBox(presetGroup);
    page.presetCharacteristicCombo->addItem(tr("Butterworth"));
    page.presetCharacteristicCombo->setToolTip(tr("Currently only Butterworth start values are available. Additional alignments can be added later."));

    page.presetFrequencySpin = new QDoubleSpinBox(presetGroup);
    page.presetFrequencySpin->setRange(1.0, 200000.0);
    page.presetFrequencySpin->setDecimals(1);
    page.presetFrequencySpin->setSingleStep(100.0);
    page.presetFrequencySpin->setSuffix(tr(" Hz"));
    page.presetFrequencySpin->setValue(2000.0);

    page.presetImpedanceSpin = new QDoubleSpinBox(presetGroup);
    page.presetImpedanceSpin->setRange(0.1, 1000.0);
    page.presetImpedanceSpin->setDecimals(3);
    page.presetImpedanceSpin->setSingleStep(0.5);
    page.presetImpedanceSpin->setSuffix(tr(" Ohm"));
    const double driverRdc = m_drivers[index].getRdc();
    page.presetImpedanceSpin->setValue(driverRdc > 0.0 ? driverRdc : 8.0);
    page.presetImpedanceSpin->setToolTip(tr("The preset assumes an ideal resistive load with this impedance. Real driver impedance is still simulated by KFilter after applying."));

    auto *insertButton = new QPushButton(tr("Insert Preset"), presetGroup);
    insertButton->setToolTip(tr("Insert the preset into the rightmost free network section block."));
    connect(insertButton, &QPushButton::clicked, this, [this, index]() {
        insertStandardFilterPreset(index);
    });

    auto *filterLayout = new QGridLayout();
    filterLayout->addWidget(new QLabel(tr("Filter type:"), presetGroup), 0, 0);
    filterLayout->addWidget(page.presetTypeCombo, 0, 1);
    filterLayout->addWidget(new QLabel(tr("Characteristic:"), presetGroup), 1, 0);
    filterLayout->addWidget(page.presetCharacteristicCombo, 1, 1);
    filterLayout->setColumnStretch(1, 1);

    auto *valueLayout = new QGridLayout();
    valueLayout->addWidget(new QLabel(tr("Frequency:"), presetGroup), 0, 0);
    valueLayout->addWidget(page.presetFrequencySpin, 0, 1);
    valueLayout->addWidget(new QLabel(tr("Impedance:"), presetGroup), 1, 0);
    valueLayout->addWidget(page.presetImpedanceSpin, 1, 1);
    valueLayout->setColumnStretch(1, 1);

    auto *actionLayout = new QGridLayout();
    actionLayout->addWidget(new QLabel(tr("Order:"), presetGroup), 0, 0);
    actionLayout->addWidget(page.presetOrderCombo, 0, 1);
    actionLayout->addWidget(insertButton, 1, 1, Qt::AlignLeft);
    actionLayout->setColumnStretch(1, 1);

    presetLayout->addLayout(filterLayout, 0, 0);
    presetLayout->addLayout(valueLayout, 0, 1);
    presetLayout->addLayout(actionLayout, 0, 2);
    presetLayout->setColumnStretch(0, 1);
    presetLayout->setColumnStretch(1, 1);
    presetLayout->setColumnStretch(2, 1);

    page.table = new QTableWidget(NetworkRowsPerSection, NetworkSectionCount, page.page);
    page.table->setHorizontalHeaderLabels(columnLabels());
    page.table->setVerticalHeaderLabels(rowLabels());
    page.table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    page.table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    page.table->setAlternatingRowColors(true);
    page.table->setSelectionMode(QAbstractItemView::SingleSelection);
    page.table->setSelectionBehavior(QAbstractItemView::SelectItems);

    for (int row = 0; row < NetworkRowsPerSection; ++row) {
        for (int column = 0; column < NetworkSectionCount; ++column) {
            auto *item = new QTableWidgetItem(QStringLiteral("0"));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            page.table->setItem(row, column, item);
        }
    }

    auto *layout = new QVBoxLayout(page.page);
    layout->addWidget(presetGroup);
    layout->addWidget(page.table);

    return page.page;
}

void NetworkParametersDialog::loadFromDrivers()
{
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        QTableWidget *table = m_pages.at(driverIndex).table;
        driver& drv = m_drivers[driverIndex];

        for (int row = 0; row < NetworkRowsPerSection; ++row) {
            for (int column = 0; column < NetworkSectionCount; ++column) {
                const double internalValue = drv.getUnit(unitIndex(row, column));
                setCellValue(table, row, column, displayFromInternal(row, internalValue));
            }
        }
    }
}

bool NetworkParametersDialog::applyToDrivers()
{
    double displayValues[KFilterProjectIo::DriverCount][NetworkRowsPerSection][NetworkSectionCount] = {};

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        QTableWidget *table = m_pages.at(driverIndex).table;
        for (int row = 0; row < NetworkRowsPerSection; ++row) {
            for (int column = 0; column < NetworkSectionCount; ++column) {
                double value = 0.0;
                QString parseError;
                if (!readCellValue(table, row, column, value, &parseError)) {
                    table->setCurrentCell(row, column);
                    QMessageBox::warning(this,
                                         tr("Network / Filter Parameters"),
                                         tr("Invalid numeric value in driver %1, row '%2', section %3.\n\n%4")
                                             .arg(driverIndex + 1)
                                             .arg(rowLabels().at(row))
                                             .arg(column + 1)
                                             .arg(parseError));
                    return false;
                }
                displayValues[driverIndex][row][column] = value;
            }
        }
    }

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        driver& drv = m_drivers[driverIndex];
        for (int row = 0; row < NetworkRowsPerSection; ++row) {
            for (int column = 0; column < NetworkSectionCount; ++column) {
                drv.setUnit(unitIndex(row, column), internalFromDisplay(row, displayValues[driverIndex][row][column]));
            }
        }
        drv.Berechneparameter();
        drv.setmodified();
    }

    return true;
}

void NetworkParametersDialog::applyClicked()
{
    if (applyToDrivers()) {
        emit parametersApplied();
    }
}

void NetworkParametersDialog::accept()
{
    if (applyToDrivers()) {
        emit parametersApplied();
        QDialog::accept();
    }
}

void NetworkParametersDialog::resetCurrentDriver()
{
    if (m_currentDriverIndex >= 0 && m_currentDriverIndex < KFilterProjectIo::DriverCount) {
        resetTable(m_pages.at(m_currentDriverIndex).table);
    }
}

void NetworkParametersDialog::resetAllDrivers()
{
    for (DriverPage& page : m_pages) {
        resetTable(page.table);
    }
}

void NetworkParametersDialog::resetTable(QTableWidget *table)
{
    for (int row = 0; row < NetworkRowsPerSection; ++row) {
        for (int column = 0; column < NetworkSectionCount; ++column) {
            setCellValue(table, row, column, 0.0);
        }
    }
}


bool NetworkParametersDialog::tableContainsInvalidValues(QTableWidget *table) const
{
    for (int row = 0; row < NetworkRowsPerSection; ++row) {
        for (int column = 0; column < NetworkSectionCount; ++column) {
            double value = 0.0;
            if (!readCellValue(table, row, column, value)) {
                return true;
            }
        }
    }
    return false;
}

bool NetworkParametersDialog::sectionIsEmpty(QTableWidget *table, int section) const
{
    if (section < 0 || section >= NetworkSectionCount) {
        return false;
    }

    for (int row = 0; row < NetworkRowsPerSection; ++row) {
        double value = 0.0;
        if (!readCellValue(table, row, section, value)) {
            return false;
        }
        if (std::abs(value) > 0.0) {
            return false;
        }
    }
    return true;
}

int NetworkParametersDialog::findRightmostFreeSectionBlock(QTableWidget *table, int requiredSections) const
{
    if (requiredSections <= 0 || requiredSections > NetworkSectionCount) {
        return -1;
    }

    for (int startSection = NetworkSectionCount - requiredSections; startSection >= 0; --startSection) {
        bool blockIsEmpty = true;
        for (int offset = 0; offset < requiredSections; ++offset) {
            if (!sectionIsEmpty(table, startSection + offset)) {
                blockIsEmpty = false;
                break;
            }
        }
        if (blockIsEmpty) {
            return startSection;
        }
    }

    return -1;
}

void NetworkParametersDialog::clearSectionBlock(QTableWidget *table, int startSection, int sectionCount) const
{
    for (int column = startSection; column < startSection + sectionCount; ++column) {
        for (int row = 0; row < NetworkRowsPerSection; ++row) {
            setCellValue(table, row, column, 0.0);
        }
    }
}

void NetworkParametersDialog::insertStandardFilterPreset(int driverIndex)
{
    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        return;
    }

    DriverPage& page = m_pages.at(driverIndex);
    const int filterType = page.presetTypeCombo->currentData().toInt();
    const int order = page.presetOrderCombo->currentData().toInt();
    const double frequencyHz = page.presetFrequencySpin->value();
    const double impedanceOhm = page.presetImpedanceSpin->value();

    if (order < 1 || order > 4 || frequencyHz <= 0.0 || impedanceOhm <= 0.0) {
        QMessageBox::warning(this,
                             tr("Standard Filter Preset"),
                             tr("Invalid filter preset parameters. Frequency and impedance must be greater than zero."));
        return;
    }

    if (tableContainsInvalidValues(page.table)) {
        QMessageBox::warning(this,
                             tr("Standard Filter Preset"),
                             tr("Cannot determine a free preset slot because the table contains invalid numeric values."));
        return;
    }

    const int requiredSections = requiredSectionCountForOrder(order);
    const int startSection = findRightmostFreeSectionBlock(page.table, requiredSections);
    if (startSection < 0) {
        QMessageBox::warning(this,
                             tr("Standard Filter Preset"),
                             requiredSections == 1
                                 ? tr("No free network section is available for this preset.")
                                 : tr("No %1 adjacent free network sections are available for this preset.").arg(requiredSections));
        return;
    }

    const std::array<double, 4> coefficients = butterworthSinglyTerminatedLowPassCoefficients(order);
    const double omega = 2.0 * Pi * frequencyHz;

    clearSectionBlock(page.table, startSection, requiredSections);

    for (int elementIndex = 0; elementIndex < order; ++elementIndex) {
        const double coefficient = coefficients.at(elementIndex);
        if (coefficient <= 0.0) {
            continue;
        }

        const int section = startSection + elementIndex / 2;
        if (section >= NetworkSectionCount) {
            break;
        }

        if (filterType == PresetLowPass) {
            if ((elementIndex % 2) == 0) {
                const double inductanceMilliHenry = impedanceOhm * coefficient / omega * 1000.0;
                setCellValue(page.table, RowSeriesL, section, inductanceMilliHenry);
            } else {
                const double capacitanceMicroFarad = coefficient / (impedanceOhm * omega) * 1000000.0;
                setCellValue(page.table, RowShuntC, section, capacitanceMicroFarad);
            }
        } else {
            if ((elementIndex % 2) == 0) {
                const double capacitanceMicroFarad = 1.0 / (impedanceOhm * omega * coefficient) * 1000000.0;
                setCellValue(page.table, RowSeriesC, section, capacitanceMicroFarad);
            } else {
                const double inductanceMilliHenry = impedanceOhm / (omega * coefficient) * 1000.0;
                setCellValue(page.table, RowShuntL, section, inductanceMilliHenry);
            }
        }
    }

    if (filterType == PresetLowPass) {
        page.table->setCurrentCell(RowSeriesL, startSection);
    } else {
        page.table->setCurrentCell(RowSeriesC, startSection);
    }
}

bool NetworkParametersDialog::readCellValue(QTableWidget *table,
                                            int row,
                                            int column,
                                            double& value,
                                            QString *errorMessage) const
{
    const QTableWidgetItem *item = table->item(row, column);
    const QString text = item == nullptr ? QString() : item->text().trimmed();
    if (NetworkValueUtils::parseNonNegativeDisplayValue(text, value)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = NetworkValueUtils::validationError(rowLabels().at(row));
    }
    return false;
}

void NetworkParametersDialog::setCellValue(QTableWidget *table, int row, int column, double value) const
{
    QTableWidgetItem *item = table->item(row, column);
    if (item == nullptr) {
        item = new QTableWidgetItem;
        table->setItem(row, column, item);
    }
    item->setText(displayNumber(value));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

int NetworkParametersDialog::unitIndex(int row, int column)
{
    return column * NetworkRowsPerSection + row + 1;
}

double NetworkParametersDialog::displayFromInternal(int row, double value)
{
    if (row == 1 || row == 4) {
        return value * 1000000.0; // F -> uF
    }
    if (row == 2 || row == 5) {
        return value * 1000.0; // H -> mH
    }
    return value;
}

double NetworkParametersDialog::internalFromDisplay(int row, double value)
{
    if (row == 1 || row == 4) {
        return value / 1000000.0; // uF -> F
    }
    if (row == 2 || row == 5) {
        return value / 1000.0; // mH -> H
    }
    return value;
}
