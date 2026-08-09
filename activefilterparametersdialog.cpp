/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "activefilterparametersdialog.h"

#include "activefilterresponse.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <variant>

namespace
{
constexpr int ColumnEnabled = 0;
constexpr int ColumnType = 1;
constexpr int ColumnCharacteristic = 2;
constexpr int ColumnOrder = 3;
constexpr int ColumnFrequency = 4;
constexpr int ColumnExtra = 5;
constexpr int ColumnCount = 6;

QTableWidgetItem *readOnlyItem(const QString& text = QString())
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QDoubleSpinBox *frequencySpinBox(QWidget *parent)
{
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setRange(1.0, 200000.0);
    spinBox->setDecimals(1);
    spinBox->setSingleStep(100.0);
    spinBox->setSuffix(QObject::tr(" Hz"));
    spinBox->setKeyboardTracking(false);
    return spinBox;
}

bool isCrossoverType(ActiveFilterType type)
{
    return type == ActiveFilterType::LowPass ||
           type == ActiveFilterType::HighPass ||
           type == ActiveFilterType::BandPass;
}
}

ActiveFilterParametersDialog::ActiveFilterParametersDialog(ActiveFilterChains& chains,
                                                           QWidget *parent,
                                                           int initialDriverIndex)
    : QDialog(parent),
      m_chains(chains),
      m_committedChains(chains),
      m_workingChains(chains)
{
    setWindowTitle(tr("Active Filter Parameters"));
    resize(920, 700);

    auto *mainLayout = new QVBoxLayout(this);

    auto *prototypeNotice = new QLabel(
        tr("Active-filter changes are previewed live in the diagnostic plot and, for supported "
           "Butterworth low-pass/high-pass/band-pass and second-order Notch sections, in the driver simulation. "
           "Apply/OK commits the edited project state; Cancel restores the last applied state. "
           "Active-filter metadata is saved in .kfp projects."),
        this);
    prototypeNotice->setWordWrap(true);
    prototypeNotice->setObjectName(QStringLiteral("activeFilterPrototypeNotice"));
    mainLayout->addWidget(prototypeNotice);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("activeFilterDriverTabs"));
    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        m_tabs->addTab(createDriverPage(index), tr("Driver %1").arg(index + 1));
    }

    loadFromWorkingModel();

    const int safeInitialDriverIndex =
        std::clamp(initialDriverIndex, 0, KFilterProjectIo::DriverCount - 1);
    m_tabs->setCurrentIndex(safeInitialDriverIndex);
    mainLayout->addWidget(m_tabs, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Apply |
                                           QDialogButtonBox::Cancel,
                                           this);
    buttonBox->setObjectName(QStringLiteral("activeFilterDialogButtons"));
    buttonBox->button(QDialogButtonBox::Apply)->setToolTip(
        tr("Commit the current live-preview state to the project. Supported active filters affect the simulation and their metadata is persisted with the project."));
    buttonBox->button(QDialogButtonBox::Ok)->setToolTip(
        tr("Keep the current live-preview state and close the dialog."));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ActiveFilterParametersDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &ActiveFilterParametersDialog::applyClicked);
    mainLayout->addWidget(buttonBox);
}

int ActiveFilterParametersDialog::currentDriverIndex() const
{
    return m_tabs ? m_tabs->currentIndex() : 0;
}

void ActiveFilterParametersDialog::accept()
{
    applyToModel();
    QDialog::accept();
}

void ActiveFilterParametersDialog::applyClicked()
{
    applyToModel();
}

void ActiveFilterParametersDialog::reject()
{
    m_chains = m_committedChains;
    emit parametersPreviewChanged();
    QDialog::reject();
}

QWidget *ActiveFilterParametersDialog::createDriverPage(int driverIndex)
{
    DriverPage& page = m_pages.at(driverIndex);
    page.page = new QWidget(this);

    auto *layout = new QVBoxLayout(page.page);

    page.activeProcessing = new QCheckBox(tr("Enable active processing for this driver"), page.page);
    page.activeProcessing->setObjectName(
        QStringLiteral("activeFilterEnableDriver%1").arg(driverIndex + 1));
    page.activeProcessing->setToolTip(
        tr("Master bypass for the complete active-filter chain of this driver."));
    layout->addWidget(page.activeProcessing);

    auto *chainGroup = new QGroupBox(tr("Active Filter Chain"), page.page);
    auto *chainLayout = new QVBoxLayout(chainGroup);

    page.sections = new QTableWidget(0, ColumnCount, chainGroup);
    page.sections->setObjectName(
        QStringLiteral("activeFilterSectionTable%1").arg(driverIndex + 1));
    page.sections->setHorizontalHeaderLabels({tr("On"),
                                              tr("Type"),
                                              tr("Characteristic"),
                                              tr("Order"),
                                              tr("Frequency"),
                                              tr("Additional parameter")});
    page.sections->horizontalHeader()->setSectionResizeMode(ColumnEnabled, QHeaderView::ResizeToContents);
    page.sections->horizontalHeader()->setSectionResizeMode(ColumnType, QHeaderView::ResizeToContents);
    page.sections->horizontalHeader()->setSectionResizeMode(ColumnCharacteristic, QHeaderView::Stretch);
    page.sections->horizontalHeader()->setSectionResizeMode(ColumnOrder, QHeaderView::ResizeToContents);
    page.sections->horizontalHeader()->setSectionResizeMode(ColumnFrequency, QHeaderView::ResizeToContents);
    page.sections->horizontalHeader()->setSectionResizeMode(ColumnExtra, QHeaderView::Stretch);
    page.sections->verticalHeader()->setVisible(false);
    page.sections->setSelectionBehavior(QAbstractItemView::SelectRows);
    page.sections->setSelectionMode(QAbstractItemView::SingleSelection);
    page.sections->setEditTriggers(QAbstractItemView::NoEditTriggers);
    page.sections->setAlternatingRowColors(true);
    chainLayout->addWidget(page.sections, 1);

    auto *chainButtonLayout = new QHBoxLayout;
    page.addButton = new QPushButton(tr("Add Section"), chainGroup);
    page.removeButton = new QPushButton(tr("Remove"), chainGroup);
    page.moveUpButton = new QPushButton(tr("Move Up"), chainGroup);
    page.moveDownButton = new QPushButton(tr("Move Down"), chainGroup);

    page.addButton->setObjectName(
        QStringLiteral("activeFilterAddButton%1").arg(driverIndex + 1));
    page.removeButton->setObjectName(
        QStringLiteral("activeFilterRemoveButton%1").arg(driverIndex + 1));
    page.moveUpButton->setObjectName(
        QStringLiteral("activeFilterMoveUpButton%1").arg(driverIndex + 1));
    page.moveDownButton->setObjectName(
        QStringLiteral("activeFilterMoveDownButton%1").arg(driverIndex + 1));

    chainButtonLayout->addWidget(page.addButton);
    chainButtonLayout->addWidget(page.removeButton);
    chainButtonLayout->addWidget(page.moveUpButton);
    chainButtonLayout->addWidget(page.moveDownButton);
    chainButtonLayout->addStretch(1);
    chainLayout->addLayout(chainButtonLayout);
    layout->addWidget(chainGroup, 1);

    page.editorGroup = new QGroupBox(tr("Selected Filter Section"), page.page);
    auto *editorLayout = new QGridLayout(page.editorGroup);

    page.sectionEnabled = new QCheckBox(tr("Section enabled"), page.editorGroup);
    page.type = new QComboBox(page.editorGroup);
    page.characteristic = new QComboBox(page.editorGroup);
    page.order = new QSpinBox(page.editorGroup);
    page.frequency1 = frequencySpinBox(page.editorGroup);
    page.frequency2 = frequencySpinBox(page.editorGroup);
    page.q = new QDoubleSpinBox(page.editorGroup);
    page.gain = new QDoubleSpinBox(page.editorGroup);
    page.delay = new QDoubleSpinBox(page.editorGroup);
    page.polarity = new QComboBox(page.editorGroup);

    page.sectionEnabled->setObjectName(
        QStringLiteral("activeFilterSectionEnabled%1").arg(driverIndex + 1));
    page.type->setObjectName(
        QStringLiteral("activeFilterTypeCombo%1").arg(driverIndex + 1));
    page.characteristic->setObjectName(
        QStringLiteral("activeFilterCharacteristicCombo%1").arg(driverIndex + 1));
    page.order->setObjectName(
        QStringLiteral("activeFilterOrderSpin%1").arg(driverIndex + 1));
    page.frequency1->setObjectName(
        QStringLiteral("activeFilterFrequency1Spin%1").arg(driverIndex + 1));
    page.frequency2->setObjectName(
        QStringLiteral("activeFilterFrequency2Spin%1").arg(driverIndex + 1));
    page.q->setObjectName(
        QStringLiteral("activeFilterQSpin%1").arg(driverIndex + 1));
    page.gain->setObjectName(
        QStringLiteral("activeFilterGainSpin%1").arg(driverIndex + 1));
    page.delay->setObjectName(
        QStringLiteral("activeFilterDelaySpin%1").arg(driverIndex + 1));
    page.polarity->setObjectName(
        QStringLiteral("activeFilterPolarityCombo%1").arg(driverIndex + 1));

    page.type->addItem(tr("Low-pass"), static_cast<int>(ActiveFilterType::LowPass));
    page.type->addItem(tr("High-pass"), static_cast<int>(ActiveFilterType::HighPass));
    page.type->addItem(tr("Band-pass"), static_cast<int>(ActiveFilterType::BandPass));
    page.type->addItem(tr("Notch"), static_cast<int>(ActiveFilterType::Notch));
    page.type->addItem(tr("All-pass"), static_cast<int>(ActiveFilterType::AllPass));
    page.type->addItem(tr("Gain"), static_cast<int>(ActiveFilterType::Gain));
    page.type->addItem(tr("Delay"), static_cast<int>(ActiveFilterType::Delay));
    page.type->addItem(tr("Polarity"), static_cast<int>(ActiveFilterType::Polarity));

    page.characteristic->addItem(tr("Butterworth"), static_cast<int>(ActiveFilterCharacteristic::Butterworth));
    page.characteristic->addItem(tr("Bessel"), static_cast<int>(ActiveFilterCharacteristic::Bessel));
    page.characteristic->addItem(tr("Linkwitz-Riley"), static_cast<int>(ActiveFilterCharacteristic::LinkwitzRiley));
    page.characteristic->addItem(tr("Generic / Q-based"), static_cast<int>(ActiveFilterCharacteristic::GenericQ));

    page.order->setRange(1, 8);
    page.order->setValue(2);

    page.frequency1->setValue(2000.0);
    page.frequency1->setToolTip(
        tr("Primary frequency. Depending on filter type this is cutoff, center, or the lower band-pass cutoff."));
    page.frequency2->setValue(4000.0);
    page.frequency2->setToolTip(
        tr("Upper cutoff frequency for Band-pass sections."));

    page.q->setRange(0.01, 100.0);
    page.q->setDecimals(3);
    page.q->setSingleStep(0.05);
    page.q->setValue(0.707);
    page.q->setKeyboardTracking(false);

    page.gain->setRange(-60.0, 24.0);
    page.gain->setDecimals(2);
    page.gain->setSingleStep(0.5);
    page.gain->setSuffix(tr(" dB"));
    page.gain->setValue(0.0);
    page.gain->setKeyboardTracking(false);

    page.delay->setRange(0.0, 1000.0);
    page.delay->setDecimals(3);
    page.delay->setSingleStep(0.1);
    page.delay->setSuffix(tr(" ms"));
    page.delay->setValue(0.0);
    page.delay->setKeyboardTracking(false);

    page.polarity->addItem(tr("Normal"), false);
    page.polarity->addItem(tr("Inverted"), true);

    editorLayout->addWidget(page.sectionEnabled, 0, 0, 1, 4);

    editorLayout->addWidget(new QLabel(tr("Type:"), page.editorGroup), 1, 0);
    editorLayout->addWidget(page.type, 1, 1);
    editorLayout->addWidget(new QLabel(tr("Q / bandwidth factor:"), page.editorGroup), 1, 2);
    editorLayout->addWidget(page.q, 1, 3);

    editorLayout->addWidget(new QLabel(tr("Characteristic:"), page.editorGroup), 2, 0);
    editorLayout->addWidget(page.characteristic, 2, 1);
    editorLayout->addWidget(new QLabel(tr("Gain:"), page.editorGroup), 2, 2);
    editorLayout->addWidget(page.gain, 2, 3);

    editorLayout->addWidget(new QLabel(tr("Order:"), page.editorGroup), 3, 0);
    editorLayout->addWidget(page.order, 3, 1);
    editorLayout->addWidget(new QLabel(tr("Delay:"), page.editorGroup), 3, 2);
    editorLayout->addWidget(page.delay, 3, 3);

    editorLayout->addWidget(new QLabel(tr("Frequency 1:"), page.editorGroup), 4, 0);
    editorLayout->addWidget(page.frequency1, 4, 1);
    editorLayout->addWidget(new QLabel(tr("Polarity:"), page.editorGroup), 4, 2);
    editorLayout->addWidget(page.polarity, 4, 3);

    editorLayout->addWidget(new QLabel(tr("Frequency 2:"), page.editorGroup), 5, 0);
    editorLayout->addWidget(page.frequency2, 5, 1);

    editorLayout->setColumnStretch(1, 1);
    editorLayout->setColumnStretch(3, 1);
    layout->addWidget(page.editorGroup);

    auto *visualizationGroup = new QGroupBox(tr("Visualization"), page.page);
    auto *visualizationLayout = new QVBoxLayout(visualizationGroup);
    page.showResponse = new QCheckBox(tr("Show active-filter transfer function in plot"), visualizationGroup);
    page.showResponse->setObjectName(
        QStringLiteral("activeFilterShowResponse%1").arg(driverIndex + 1));
    page.showResponse->setToolTip(
        tr("Show the combined transfer magnitude of all enabled supported sections, including "
           "Butterworth low-pass/high-pass and second-order Notch. If any enabled section is not "
           "implemented yet, no active-filter overlay is drawn for this driver."));
    visualizationLayout->addWidget(page.showResponse);

    page.responseStatus = new QLabel(visualizationGroup);
    page.responseStatus->setObjectName(
        QStringLiteral("activeFilterResponseStatus%1").arg(driverIndex + 1));
    page.responseStatus->setWordWrap(true);
    visualizationLayout->addWidget(page.responseStatus);
    layout->addWidget(visualizationGroup);

    connect(page.addButton, &QPushButton::clicked, this, [this, driverIndex]() {
        addSection(driverIndex);
    });
    connect(page.removeButton, &QPushButton::clicked, this, [this, driverIndex]() {
        removeSelectedSection(driverIndex);
    });
    connect(page.moveUpButton, &QPushButton::clicked, this, [this, driverIndex]() {
        moveSelectedSection(driverIndex, -1);
    });
    connect(page.moveDownButton, &QPushButton::clicked, this, [this, driverIndex]() {
        moveSelectedSection(driverIndex, 1);
    });
    connect(page.sections, &QTableWidget::itemSelectionChanged, this, [this, driverIndex]() {
        selectionChanged(driverIndex);
    });
    connect(page.activeProcessing, &QCheckBox::toggled, this, [this, driverIndex](bool enabled) {
        if (!m_loadingEditor) {
            m_workingChains.at(static_cast<std::size_t>(driverIndex)).setEnabled(enabled);
            previewWorkingModel();
        }
        updatePageState(driverIndex);
    });
    connect(page.showResponse, &QCheckBox::toggled, this, [this, driverIndex](bool show) {
        if (!m_loadingEditor) {
            m_workingChains.at(static_cast<std::size_t>(driverIndex)).setShowResponseInPlot(show);
            previewWorkingModel();
        }
    });

    connectEditorSignals(driverIndex);
    updatePageState(driverIndex);
    return page.page;
}

void ActiveFilterParametersDialog::connectEditorSignals(int driverIndex)
{
    DriverPage& page = m_pages.at(driverIndex);
    const auto changed = [this, driverIndex]() {
        if (m_loadingEditor) {
            return;
        }
        writeEditorToSelectedSection(driverIndex);
        previewWorkingModel();
    };

    connect(page.sectionEnabled, &QCheckBox::toggled, this, changed);
    connect(page.type,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this, driverIndex](int) { selectedTypeChanged(driverIndex); });
    connect(page.characteristic,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this, driverIndex, changed](int) {
                updateEditorControlState(driverIndex);
                changed();
            });
    connect(page.order,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [changed](int) { changed(); });
    connect(page.frequency1,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [changed](double) { changed(); });
    connect(page.frequency2,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [changed](double) { changed(); });
    connect(page.q,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [changed](double) { changed(); });
    connect(page.gain,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [changed](double) { changed(); });
    connect(page.delay,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [changed](double) { changed(); });
    connect(page.polarity,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [changed](int) { changed(); });
}

void ActiveFilterParametersDialog::loadFromWorkingModel()
{
    m_loadingEditor = true;
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        DriverPage& page = m_pages.at(driverIndex);
        const ActiveFilterChain& chain =
            m_workingChains.at(static_cast<std::size_t>(driverIndex));
        page.activeProcessing->setChecked(chain.enabled());
        page.showResponse->setChecked(chain.showResponseInPlot());
        refreshSectionTable(driverIndex, chain.empty() ? -1 : 0);
    }
    m_loadingEditor = false;

    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        selectionChanged(driverIndex);
    }
}

void ActiveFilterParametersDialog::refreshSectionTable(int driverIndex, int preferredRow)
{
    DriverPage& page = m_pages.at(driverIndex);
    const ActiveFilterChain& chain =
        m_workingChains.at(static_cast<std::size_t>(driverIndex));

    page.sections->setRowCount(static_cast<int>(chain.sectionCount()));
    for (int row = 0; row < page.sections->rowCount(); ++row) {
        for (int column = 0; column < ColumnCount; ++column) {
            if (page.sections->item(row, column) == nullptr) {
                page.sections->setItem(row, column, readOnlyItem());
            }
        }
        refreshSectionRow(driverIndex, row);
    }

    if (page.sections->rowCount() > 0 && preferredRow >= 0) {
        const int row = std::clamp(preferredRow, 0, page.sections->rowCount() - 1);
        page.sections->selectRow(row);
        page.sections->setCurrentCell(row, ColumnType);
    } else {
        page.sections->clearSelection();
        page.sections->setCurrentCell(-1, -1);
    }
}

void ActiveFilterParametersDialog::addSection(int driverIndex)
{
    ActiveFilterChain& chain = m_workingChains.at(static_cast<std::size_t>(driverIndex));
    const std::size_t index = chain.addSection();
    refreshSectionTable(driverIndex, static_cast<int>(index));
    selectionChanged(driverIndex);
    previewWorkingModel();
}

void ActiveFilterParametersDialog::removeSelectedSection(int driverIndex)
{
    DriverPage& page = m_pages.at(driverIndex);
    const int row = page.sections->currentRow();
    if (row < 0) {
        return;
    }

    ActiveFilterChain& chain = m_workingChains.at(static_cast<std::size_t>(driverIndex));
    if (!chain.removeSection(static_cast<std::size_t>(row))) {
        return;
    }

    const int preferredRow = chain.empty()
        ? -1
        : std::min(row, static_cast<int>(chain.sectionCount()) - 1);
    refreshSectionTable(driverIndex, preferredRow);
    selectionChanged(driverIndex);
    previewWorkingModel();
}

void ActiveFilterParametersDialog::moveSelectedSection(int driverIndex, int direction)
{
    DriverPage& page = m_pages.at(driverIndex);
    const int row = page.sections->currentRow();
    const int targetRow = row + direction;
    ActiveFilterChain& chain = m_workingChains.at(static_cast<std::size_t>(driverIndex));
    if (row < 0 || targetRow < 0 ||
        targetRow >= static_cast<int>(chain.sectionCount())) {
        return;
    }

    if (!chain.moveSection(static_cast<std::size_t>(row),
                           static_cast<std::size_t>(targetRow))) {
        return;
    }

    refreshSectionTable(driverIndex, targetRow);
    selectionChanged(driverIndex);
    previewWorkingModel();
}

void ActiveFilterParametersDialog::selectionChanged(int driverIndex)
{
    if (m_loadingEditor) {
        return;
    }
    loadEditorFromSelectedSection(driverIndex);
    updatePageState(driverIndex);
}

void ActiveFilterParametersDialog::selectedTypeChanged(int driverIndex)
{
    if (m_loadingEditor) {
        return;
    }

    DriverPage& page = m_pages.at(driverIndex);
    const int row = page.sections->currentRow();
    ActiveFilterChain& chain = m_workingChains.at(static_cast<std::size_t>(driverIndex));
    if (row < 0 || row >= static_cast<int>(chain.sectionCount())) {
        return;
    }

    ActiveFilterSection& section = chain.section(static_cast<std::size_t>(row));
    const ActiveFilterType newType = static_cast<ActiveFilterType>(page.type->currentData().toInt());
    section.setType(newType);
    loadEditorFromSelectedSection(driverIndex);
    refreshSectionRow(driverIndex, row);
    previewWorkingModel();
}

void ActiveFilterParametersDialog::writeEditorToSelectedSection(int driverIndex)
{
    if (m_loadingEditor) {
        return;
    }

    DriverPage& page = m_pages.at(driverIndex);
    const int row = page.sections->currentRow();
    ActiveFilterChain& chain = m_workingChains.at(static_cast<std::size_t>(driverIndex));
    if (row < 0 || row >= static_cast<int>(chain.sectionCount())) {
        return;
    }

    ActiveFilterSection& section = chain.section(static_cast<std::size_t>(row));
    section.setEnabled(page.sectionEnabled->isChecked());

    const ActiveFilterCharacteristic characteristic =
        static_cast<ActiveFilterCharacteristic>(page.characteristic->currentData().toInt());

    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        parameters.characteristic = characteristic;
        parameters.order = page.order->value();
        parameters.frequencyHz = page.frequency1->value();
        parameters.q = page.q->value();
        break;
    }
    case ActiveFilterType::HighPass: {
        auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        parameters.characteristic = characteristic;
        parameters.order = page.order->value();
        parameters.frequencyHz = page.frequency1->value();
        parameters.q = page.q->value();
        break;
    }
    case ActiveFilterType::BandPass: {
        auto& parameters = std::get<ActiveFilterBandPassParameters>(section.parameters());
        parameters.characteristic = characteristic;
        parameters.order = page.order->value();
        parameters.lowerFrequencyHz = page.frequency1->value();
        parameters.upperFrequencyHz = page.frequency2->value();
        parameters.q = page.q->value();
        break;
    }
    case ActiveFilterType::Notch: {
        auto& parameters = std::get<ActiveFilterNotchParameters>(section.parameters());
        parameters.centerFrequencyHz = page.frequency1->value();
        parameters.q = page.q->value();
        parameters.gainDb = page.gain->value();
        break;
    }
    case ActiveFilterType::AllPass: {
        auto& parameters = std::get<ActiveFilterAllPassParameters>(section.parameters());
        parameters.order = page.order->value();
        parameters.frequencyHz = page.frequency1->value();
        parameters.q = page.q->value();
        break;
    }
    case ActiveFilterType::Gain:
        std::get<ActiveFilterGainParameters>(section.parameters()).gainDb = page.gain->value();
        break;
    case ActiveFilterType::Delay:
        std::get<ActiveFilterDelayParameters>(section.parameters()).delayMs = page.delay->value();
        break;
    case ActiveFilterType::Polarity:
        std::get<ActiveFilterPolarityParameters>(section.parameters()).inverted =
            page.polarity->currentData().toBool();
        break;
    }

    refreshSectionRow(driverIndex, row);
}

void ActiveFilterParametersDialog::loadEditorFromSelectedSection(int driverIndex)
{
    DriverPage& page = m_pages.at(driverIndex);
    const int row = page.sections->currentRow();
    const ActiveFilterChain& chain =
        m_workingChains.at(static_cast<std::size_t>(driverIndex));
    if (row < 0 || row >= static_cast<int>(chain.sectionCount())) {
        page.editorGroup->setEnabled(false);
        return;
    }

    const ActiveFilterSection& section = chain.section(static_cast<std::size_t>(row));

    m_loadingEditor = true;
    page.sectionEnabled->setChecked(section.enabled());
    page.type->setCurrentIndex(page.type->findData(static_cast<int>(section.type())));

    page.characteristic->setCurrentIndex(
        page.characteristic->findData(static_cast<int>(ActiveFilterCharacteristic::Butterworth)));
    page.order->setValue(2);
    page.frequency1->setValue(2000.0);
    page.frequency2->setValue(4000.0);
    page.q->setValue(0.707);
    page.gain->setValue(0.0);
    page.delay->setValue(0.0);
    page.polarity->setCurrentIndex(page.polarity->findData(false));

    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        const auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        page.characteristic->setCurrentIndex(page.characteristic->findData(static_cast<int>(parameters.characteristic)));
        page.order->setValue(parameters.order);
        page.frequency1->setValue(parameters.frequencyHz);
        page.q->setValue(parameters.q);
        break;
    }
    case ActiveFilterType::HighPass: {
        const auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        page.characteristic->setCurrentIndex(page.characteristic->findData(static_cast<int>(parameters.characteristic)));
        page.order->setValue(parameters.order);
        page.frequency1->setValue(parameters.frequencyHz);
        page.q->setValue(parameters.q);
        break;
    }
    case ActiveFilterType::BandPass: {
        const auto& parameters = std::get<ActiveFilterBandPassParameters>(section.parameters());
        page.characteristic->setCurrentIndex(page.characteristic->findData(static_cast<int>(parameters.characteristic)));
        page.order->setValue(parameters.order);
        page.frequency1->setValue(parameters.lowerFrequencyHz);
        page.frequency2->setValue(parameters.upperFrequencyHz);
        page.q->setValue(parameters.q);
        break;
    }
    case ActiveFilterType::Notch: {
        const auto& parameters = std::get<ActiveFilterNotchParameters>(section.parameters());
        page.frequency1->setValue(parameters.centerFrequencyHz);
        page.q->setValue(parameters.q);
        page.gain->setValue(parameters.gainDb);
        break;
    }
    case ActiveFilterType::AllPass: {
        const auto& parameters = std::get<ActiveFilterAllPassParameters>(section.parameters());
        page.order->setValue(parameters.order);
        page.frequency1->setValue(parameters.frequencyHz);
        page.q->setValue(parameters.q);
        break;
    }
    case ActiveFilterType::Gain:
        page.gain->setValue(std::get<ActiveFilterGainParameters>(section.parameters()).gainDb);
        break;
    case ActiveFilterType::Delay:
        page.delay->setValue(std::get<ActiveFilterDelayParameters>(section.parameters()).delayMs);
        break;
    case ActiveFilterType::Polarity:
        page.polarity->setCurrentIndex(page.polarity->findData(
            std::get<ActiveFilterPolarityParameters>(section.parameters()).inverted));
        break;
    }

    m_loadingEditor = false;
    page.editorGroup->setEnabled(true);
    updateEditorControlState(driverIndex);
}

void ActiveFilterParametersDialog::updatePageState(int driverIndex)
{
    DriverPage& page = m_pages.at(driverIndex);
    const int row = page.sections->currentRow();
    const int rowCount = page.sections->rowCount();
    const bool haveSelection = row >= 0 && row < rowCount;

    page.removeButton->setEnabled(haveSelection);
    page.moveUpButton->setEnabled(haveSelection && row > 0);
    page.moveDownButton->setEnabled(haveSelection && row + 1 < rowCount);
    page.editorGroup->setEnabled(haveSelection);

    const QString bypassHint = page.activeProcessing->isChecked()
        ? tr("The active-filter chain is enabled. Fully supported Butterworth low-pass/high-pass/band-pass and second-order Notch chains are applied to the driver simulation.")
        : tr("The active-filter chain is bypassed. Sections remain editable while bypassed.");
    page.activeProcessing->setToolTip(bypassHint);
    updateResponseStatus(driverIndex);
}

void ActiveFilterParametersDialog::updateResponseStatus(int driverIndex)
{
    DriverPage& page = m_pages.at(driverIndex);
    if (page.responseStatus == nullptr) {
        return;
    }

    const ActiveFilterChain& chain =
        m_workingChains.at(static_cast<std::size_t>(driverIndex));
    const ActiveFilterResponse response = calculateActiveFilterResponse(chain);

    if (!chain.enabled()) {
        page.responseStatus->setText(tr("Simulation status: bypassed (active processing disabled)."));
        return;
    }

    switch (response.status) {
    case ActiveFilterResponseStatus::Neutral:
        page.responseStatus->setText(
            tr("Simulation status: neutral (no enabled filter section)."));
        break;
    case ActiveFilterResponseStatus::Valid:
        page.responseStatus->setText(
            tr("Simulation status: active-filter transfer is valid and applied."));
        break;
    case ActiveFilterResponseStatus::Unsupported: {
        QString typeText = tr("unknown");
        if (response.problemSectionIndex < chain.sectionCount()) {
            typeText = sectionTypeText(chain.section(response.problemSectionIndex).type());
        }
        page.responseStatus->setText(
            tr("Simulation status: active-filter chain bypassed — section %1 (%2) is not supported yet.")
                .arg(static_cast<qulonglong>(response.problemSectionIndex + 1))
                .arg(typeText));
        break;
    }
    case ActiveFilterResponseStatus::InvalidParameters:
        page.responseStatus->setText(
            tr("Simulation status: active-filter chain bypassed — section %1 has invalid parameters.")
                .arg(static_cast<qulonglong>(response.problemSectionIndex + 1)));
        break;
    }
}

void ActiveFilterParametersDialog::updateEditorControlState(int driverIndex)
{
    DriverPage& page = m_pages.at(driverIndex);
    if (page.sections->currentRow() < 0) {
        return;
    }

    const ActiveFilterType type = static_cast<ActiveFilterType>(page.type->currentData().toInt());
    const bool crossover = isCrossoverType(type);
    const bool frequencyBased = crossover ||
                                type == ActiveFilterType::Notch ||
                                type == ActiveFilterType::AllPass;
    const bool twoFrequencies = type == ActiveFilterType::BandPass;
    const bool qBased = type == ActiveFilterType::Notch ||
                        type == ActiveFilterType::AllPass ||
                        (crossover && page.characteristic->currentData().toInt() ==
                                          static_cast<int>(ActiveFilterCharacteristic::GenericQ));

    page.characteristic->setEnabled(crossover);
    page.order->setEnabled(crossover || type == ActiveFilterType::AllPass);
    page.frequency1->setEnabled(frequencyBased);
    page.frequency2->setEnabled(twoFrequencies);
    page.q->setEnabled(qBased);
    // The canonical Patch-182 notch is always full-depth; gainDb is retained only
    // as persisted forward-compatible metadata and is intentionally not editable here.
    page.gain->setEnabled(type == ActiveFilterType::Gain);
    page.delay->setEnabled(type == ActiveFilterType::Delay);
    page.polarity->setEnabled(type == ActiveFilterType::Polarity);
}

void ActiveFilterParametersDialog::refreshSectionRow(int driverIndex, int row)
{
    DriverPage& page = m_pages.at(driverIndex);
    const ActiveFilterChain& chain =
        m_workingChains.at(static_cast<std::size_t>(driverIndex));
    if (row < 0 || row >= page.sections->rowCount() ||
        row >= static_cast<int>(chain.sectionCount())) {
        return;
    }

    const ActiveFilterSection& section = chain.section(static_cast<std::size_t>(row));
    page.sections->item(row, ColumnEnabled)->setText(section.enabled() ? tr("Yes") : tr("No"));
    page.sections->item(row, ColumnType)->setText(sectionTypeText(section.type()));
    page.sections->item(row, ColumnCharacteristic)->setText(characteristicSummary(section));
    page.sections->item(row, ColumnOrder)->setText(orderSummary(section));
    page.sections->item(row, ColumnFrequency)->setText(frequencySummary(section));
    page.sections->item(row, ColumnExtra)->setText(extraSummary(section));
}

void ActiveFilterParametersDialog::previewWorkingModel()
{
    m_chains = m_workingChains;
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        updateResponseStatus(driverIndex);
    }
    emit parametersPreviewChanged();
}

void ActiveFilterParametersDialog::applyToModel()
{
    m_chains = m_workingChains;
    m_committedChains = m_workingChains;
    emit parametersApplied();
}

QString ActiveFilterParametersDialog::sectionTypeText(ActiveFilterType type) const
{
    switch (type) {
    case ActiveFilterType::LowPass:
        return tr("Low-pass");
    case ActiveFilterType::HighPass:
        return tr("High-pass");
    case ActiveFilterType::BandPass:
        return tr("Band-pass");
    case ActiveFilterType::Notch:
        return tr("Notch");
    case ActiveFilterType::AllPass:
        return tr("All-pass");
    case ActiveFilterType::Gain:
        return tr("Gain");
    case ActiveFilterType::Delay:
        return tr("Delay");
    case ActiveFilterType::Polarity:
        return tr("Polarity");
    }
    return tr("Unknown");
}

QString ActiveFilterParametersDialog::characteristicText(ActiveFilterCharacteristic characteristic) const
{
    switch (characteristic) {
    case ActiveFilterCharacteristic::Butterworth:
        return tr("Butterworth");
    case ActiveFilterCharacteristic::Bessel:
        return tr("Bessel");
    case ActiveFilterCharacteristic::LinkwitzRiley:
        return tr("Linkwitz-Riley");
    case ActiveFilterCharacteristic::GenericQ:
        return tr("Generic / Q-based");
    }
    return tr("Unknown");
}

QString ActiveFilterParametersDialog::characteristicSummary(const ActiveFilterSection& section) const
{
    switch (section.type()) {
    case ActiveFilterType::LowPass:
        return characteristicText(std::get<ActiveFilterLowPassParameters>(section.parameters()).characteristic);
    case ActiveFilterType::HighPass:
        return characteristicText(std::get<ActiveFilterHighPassParameters>(section.parameters()).characteristic);
    case ActiveFilterType::BandPass:
        return characteristicText(std::get<ActiveFilterBandPassParameters>(section.parameters()).characteristic);
    default:
        return tr("-");
    }
}

QString ActiveFilterParametersDialog::orderSummary(const ActiveFilterSection& section) const
{
    switch (section.type()) {
    case ActiveFilterType::LowPass:
        return QString::number(std::get<ActiveFilterLowPassParameters>(section.parameters()).order);
    case ActiveFilterType::HighPass:
        return QString::number(std::get<ActiveFilterHighPassParameters>(section.parameters()).order);
    case ActiveFilterType::BandPass:
        return QString::number(std::get<ActiveFilterBandPassParameters>(section.parameters()).order);
    case ActiveFilterType::AllPass:
        return QString::number(std::get<ActiveFilterAllPassParameters>(section.parameters()).order);
    default:
        return tr("-");
    }
}

QString ActiveFilterParametersDialog::frequencySummary(const ActiveFilterSection& section) const
{
    switch (section.type()) {
    case ActiveFilterType::LowPass:
        return tr("%1 Hz").arg(std::get<ActiveFilterLowPassParameters>(section.parameters()).frequencyHz, 0, 'f', 1);
    case ActiveFilterType::HighPass:
        return tr("%1 Hz").arg(std::get<ActiveFilterHighPassParameters>(section.parameters()).frequencyHz, 0, 'f', 1);
    case ActiveFilterType::BandPass: {
        const auto& parameters = std::get<ActiveFilterBandPassParameters>(section.parameters());
        return tr("%1 - %2 Hz")
            .arg(parameters.lowerFrequencyHz, 0, 'f', 1)
            .arg(parameters.upperFrequencyHz, 0, 'f', 1);
    }
    case ActiveFilterType::Notch:
        return tr("%1 Hz").arg(std::get<ActiveFilterNotchParameters>(section.parameters()).centerFrequencyHz, 0, 'f', 1);
    case ActiveFilterType::AllPass:
        return tr("%1 Hz").arg(std::get<ActiveFilterAllPassParameters>(section.parameters()).frequencyHz, 0, 'f', 1);
    default:
        return tr("-");
    }
}

QString ActiveFilterParametersDialog::extraSummary(const ActiveFilterSection& section) const
{
    switch (section.type()) {
    case ActiveFilterType::LowPass: {
        const auto& parameters = std::get<ActiveFilterLowPassParameters>(section.parameters());
        return parameters.characteristic == ActiveFilterCharacteristic::GenericQ
            ? tr("Q %1").arg(parameters.q, 0, 'f', 3)
            : tr("-");
    }
    case ActiveFilterType::HighPass: {
        const auto& parameters = std::get<ActiveFilterHighPassParameters>(section.parameters());
        return parameters.characteristic == ActiveFilterCharacteristic::GenericQ
            ? tr("Q %1").arg(parameters.q, 0, 'f', 3)
            : tr("-");
    }
    case ActiveFilterType::BandPass: {
        const auto& parameters = std::get<ActiveFilterBandPassParameters>(section.parameters());
        return parameters.characteristic == ActiveFilterCharacteristic::GenericQ
            ? tr("Q %1").arg(parameters.q, 0, 'f', 3)
            : tr("-");
    }
    case ActiveFilterType::Notch: {
        const auto& parameters = std::get<ActiveFilterNotchParameters>(section.parameters());
        return tr("Q %1").arg(parameters.q, 0, 'f', 3);
    }
    case ActiveFilterType::AllPass:
        return tr("Q %1").arg(std::get<ActiveFilterAllPassParameters>(section.parameters()).q, 0, 'f', 3);
    case ActiveFilterType::Gain:
        return tr("%1 dB").arg(std::get<ActiveFilterGainParameters>(section.parameters()).gainDb, 0, 'f', 2);
    case ActiveFilterType::Delay:
        return tr("%1 ms").arg(std::get<ActiveFilterDelayParameters>(section.parameters()).delayMs, 0, 'f', 3);
    case ActiveFilterType::Polarity:
        return std::get<ActiveFilterPolarityParameters>(section.parameters()).inverted
            ? tr("Inverted")
            : tr("Normal");
    }
    return tr("-");
}
