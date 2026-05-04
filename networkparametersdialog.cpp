/***************************************************************************
                          networkparametersdialog.cpp  -  Qt6 network editor
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "networkparametersdialog.h"

#include "driver.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <QLocale>

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
    return QLocale::c().toString(value, 'g', 8);
}
}

NetworkParametersDialog::NetworkParametersDialog(driver (&drivers)[KFilterProjectIo::DriverCount], QWidget *parent)
    : QDialog(parent),
      m_drivers(drivers)
{
    setWindowTitle(tr("Network / Filter Parameters"));
    resize(820, 440);

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

QWidget *NetworkParametersDialog::createDriverPage(int index)
{
    DriverPage& page = m_pages.at(index);
    page.page = new QWidget(this);

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
                if (!readCellValue(table, row, column, value)) {
                    QMessageBox::warning(this,
                                         tr("Network / Filter Parameters"),
                                         tr("Invalid numeric value in driver %1, row '%2', section %3.")
                                             .arg(driverIndex + 1)
                                             .arg(rowLabels().at(row))
                                             .arg(column + 1));
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

bool NetworkParametersDialog::readCellValue(QTableWidget *table, int row, int column, double& value) const
{
    const QTableWidgetItem *item = table->item(row, column);
    const QString text = item == nullptr ? QString() : item->text().trimmed();
    if (text.isEmpty()) {
        value = 0.0;
        return true;
    }

    bool ok = false;
    value = QLocale::c().toDouble(text, &ok);
    return ok;
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
