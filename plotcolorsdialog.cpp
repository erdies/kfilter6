/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "plotcolorsdialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace
{
QColor readableTextColorFor(const QColor& color)
{
    return color.lightness() < 128 ? QColor(Qt::white) : QColor(Qt::black);
}
}

PlotColorsDialog::PlotColorsDialog(const KFilterView::PlotColorSettings& initialSettings,
                                   QWidget *parent)
    : QDialog(parent),
      m_settings(initialSettings)
{
    setWindowTitle(tr("Grid and Plot Colors"));

    auto *mainLayout = new QVBoxLayout(this);

    auto *noteLabel = new QLabel(tr("These colors are application settings. They are not stored in .kfp project files."), this);
    noteLabel->setWordWrap(true);
    mainLayout->addWidget(noteLabel);

    auto *grid = new QGridLayout();
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 0);

    int row = 0;
    addColorRow(grid, row++, tr("Plot background:"), m_settings.background);
    addColorRow(grid, row++, tr("Grid:"), m_settings.grid);
    addColorRow(grid, row++, tr("Threshold lines:"), m_settings.thresholdGrid);
    addColorRow(grid, row++, tr("Driver 1 SPL:"), m_settings.pressureCurves[0]);
    addColorRow(grid, row++, tr("Driver 2 SPL:"), m_settings.pressureCurves[1]);
    addColorRow(grid, row++, tr("Driver 3 SPL:"), m_settings.pressureCurves[2]);
    addColorRow(grid, row++, tr("Driver 4 SPL:"), m_settings.pressureCurves[3]);
    addColorRow(grid, row++, tr("Driver 1 impedance:"), m_settings.impedanceCurves[0]);
    addColorRow(grid, row++, tr("Driver 2 impedance:"), m_settings.impedanceCurves[1]);
    addColorRow(grid, row++, tr("Driver 3 impedance:"), m_settings.impedanceCurves[2]);
    addColorRow(grid, row++, tr("Driver 4 impedance:"), m_settings.impedanceCurves[3]);
    addColorRow(grid, row++, tr("Vector SPL sum:"), m_settings.pressureSummary);
    addColorRow(grid, row++, tr("Energetic SPL sum:"), m_settings.scalarPressureSummary);
    addColorRow(grid, row++, tr("Total impedance:"), m_settings.impedanceSummary);

    mainLayout->addLayout(grid);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Cancel |
                                           QDialogButtonBox::RestoreDefaults,
                                           this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PlotColorsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PlotColorsDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &PlotColorsDialog::resetToDefaults);
    mainLayout->addWidget(buttonBox);

    updateColorButtons();
}

KFilterView::PlotColorSettings PlotColorsDialog::settings() const
{
    return m_settings;
}

void PlotColorsDialog::addColorRow(QGridLayout *grid,
                                   int row,
                                   const QString& label,
                                   QColor& color)
{
    auto *labelWidget = new QLabel(label, this);
    auto *button = new QPushButton(this);
    button->setMinimumWidth(110);
    button->setToolTip(tr("Choose %1").arg(label.left(label.size() - 1)));

    ColorRow colorRow;
    colorRow.button = button;
    colorRow.color = &color;
    colorRow.title = label.left(label.endsWith(QLatin1Char(':')) ? label.size() - 1 : label.size());
    m_colorRows.append(colorRow);

    const int rowIndex = m_colorRows.size() - 1;
    connect(button, &QPushButton::clicked, this, [this, rowIndex]() {
        ColorRow& row = m_colorRows[rowIndex];
        chooseColor(row);
    });

    grid->addWidget(labelWidget, row, 0);
    grid->addWidget(button, row, 1);
}

void PlotColorsDialog::chooseColor(ColorRow& row)
{
    if (row.color == nullptr) {
        return;
    }

    const QColor selectedColor = QColorDialog::getColor(*row.color,
                                                        this,
                                                        tr("Choose %1").arg(row.title));
    if (!selectedColor.isValid()) {
        return;
    }

    *row.color = selectedColor;
    updateColorButtons();
}

void PlotColorsDialog::resetToDefaults()
{
    m_settings = KFilterView::defaultPlotColorSettings();
    updateColorButtons();
}

void PlotColorsDialog::updateColorButtons()
{
    for (const ColorRow& row : std::as_const(m_colorRows)) {
        if (row.button != nullptr && row.color != nullptr) {
            setColorButtonAppearance(row.button, *row.color);
        }
    }
}

void PlotColorsDialog::setColorButtonAppearance(QPushButton *button, const QColor& color)
{
    if (button == nullptr) {
        return;
    }

    const QString colorName = color.name(QColor::HexRgb).toUpper();
    const QString textColorName = readableTextColorFor(color).name(QColor::HexRgb);
    button->setText(colorName);
    button->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; }")
                              .arg(colorName, textColorName));
}
