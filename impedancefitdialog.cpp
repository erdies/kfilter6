/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "impedancefitdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QString ohmText(double value)
{
    return QObject::tr("%1 Ohm").arg(value, 0, 'f', 3);
}

QString milliHenryText(double valueHenry)
{
    return QObject::tr("%1 mH").arg(valueHenry * 1000.0, 0, 'f', 4);
}
}

ImpedanceFitDialog::ImpedanceFitDialog(const driver& drv,
                                       const KFilterImpedanceReferenceCurve& zma,
                                       const QString& driverLabel,
                                       QWidget *parent)
    : QDialog(parent),
      m_driver(drv),
      m_zma(zma)
{
    setWindowTitle(tr("Fit Voice Coil to ZMA"));
    setMinimumWidth(460);

    auto *intro = new QLabel(
        tr("Fits Lsp, R2 and L2 of %1 to its imported impedance ZMA. Fs, Qes, Qms, Vas and the "
           "enclosure values stay unchanged but are part of the model, so the resonance is "
           "taken into account.")
            .arg(driverLabel),
        this);
    intro->setWordWrap(true);

    double minimumHz = 0.0;
    double maximumHz = 0.0;
    defaultImpedanceFitBand(m_driver, m_zma, minimumHz, maximumHz);
    const double firstHz = m_zma.isEmpty() ? 1.0 : m_zma.constFirst().frequencyHz;
    const double lastHz = m_zma.isEmpty() ? 1.0 : m_zma.constLast().frequencyHz;

    auto createFrequencySpin = [this, firstHz, lastHz](double value) {
        auto *spin = new QDoubleSpinBox(this);
        spin->setRange(firstHz, lastHz);
        spin->setDecimals(1);
        spin->setSingleStep(10.0);
        spin->setSuffix(tr(" Hz"));
        spin->setValue(value);
        connect(spin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this, [this](double) { invalidateFit(); });
        return spin;
    };
    m_minimumFrequency = createFrequencySpin(minimumHz);
    m_maximumFrequency = createFrequencySpin(maximumHz);
    m_minimumFrequency->setToolTip(tr("Lower end of the fit band. The default 2 x Fs keeps the "
                                      "resonance, where the T/S values dominate, out of the fit."));

    m_fitRdc = new QCheckBox(tr("Also fit Rdc"), this);
    m_fitRdc->setToolTip(tr("Rdc is normally measured directly and therefore kept fixed."));
    connect(m_fitRdc, &QCheckBox::toggled, this, [this](bool) { invalidateFit(); });

    auto *settingsForm = new QFormLayout;
    settingsForm->addRow(tr("From"), m_minimumFrequency);
    settingsForm->addRow(tr("To"), m_maximumFrequency);
    settingsForm->addRow(QString(), m_fitRdc);

    auto *fitButton = new QPushButton(tr("Fit"), this);
    connect(fitButton, &QPushButton::clicked, this, &ImpedanceFitDialog::runFit);
    settingsForm->addRow(QString(), fitButton);

    auto *resultGroup = new QGroupBox(tr("Result"), this);
    auto *resultGrid = new QGridLayout(resultGroup);
    resultGrid->addWidget(new QLabel(tr("<b>Current</b>"), resultGroup), 0, 1);
    resultGrid->addWidget(new QLabel(tr("<b>Fitted</b>"), resultGroup), 0, 2);
    const std::array<QString, 4> names{tr("Rdc"), tr("Lsp"), tr("R2"), tr("L2")};
    const std::array<QString, 4> currentValues{ohmText(m_driver.getRdc()),
                                               milliHenryText(m_driver.getLsp()),
                                               ohmText(m_driver.getR2()),
                                               milliHenryText(m_driver.getL2())};
    for (int row = 0; row < 4; ++row) {
        resultGrid->addWidget(new QLabel(names[static_cast<std::size_t>(row)], resultGroup), row + 1, 0);
        auto *current = new QLabel(currentValues[static_cast<std::size_t>(row)], resultGroup);
        current->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        resultGrid->addWidget(current, row + 1, 1);
        auto *fitted = new QLabel(QStringLiteral("-"), resultGroup);
        fitted->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        resultGrid->addWidget(fitted, row + 1, 2);
        m_fittedValueLabels[static_cast<std::size_t>(row)] = fitted;
    }
    m_summary = new QLabel(tr("Press Fit to start."), resultGroup);
    m_summary->setWordWrap(true);
    resultGrid->addWidget(m_summary, 5, 0, 1, 3);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_applyButton = buttonBox->addButton(tr("Apply to Driver"), QDialogButtonBox::AcceptRole);
    m_applyButton->setEnabled(false);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(settingsForm);
    layout->addWidget(resultGroup);
    layout->addWidget(buttonBox);
}

const KFilterImpedanceFitResult& ImpedanceFitDialog::fitResult() const
{
    return m_result;
}

bool ImpedanceFitDialog::rdcFitted() const
{
    return m_resultRdcFitted;
}

void ImpedanceFitDialog::runFit()
{
    KFilterImpedanceFitOptions options;
    options.minimumFrequencyHz = m_minimumFrequency->value();
    options.maximumFrequencyHz = m_maximumFrequency->value();
    options.fitRdc = m_fitRdc->isChecked();

    m_result = fitVoiceCoilToImpedance(m_driver, m_zma, options);
    m_resultRdcFitted = options.fitRdc;
    if (!m_result.success) {
        invalidateFit();
        m_summary->setText(m_result.errorMessage);
        return;
    }

    m_fittedValueLabels[0]->setText(options.fitRdc ? ohmText(m_result.rdcOhm) : tr("(fixed)"));
    m_fittedValueLabels[1]->setText(milliHenryText(m_result.lspH));
    m_fittedValueLabels[2]->setText(ohmText(m_result.r2Ohm));
    m_fittedValueLabels[3]->setText(milliHenryText(m_result.l2H));

    QString summary = tr("Mean deviation from the ZMA: %1 % before, %2 % after "
                         "(%3 points, %4 iterations).")
                          .arg(m_result.initialRmsPercent, 0, 'f', 1)
                          .arg(m_result.finalRmsPercent, 0, 'f', 2)
                          .arg(m_result.pointCount)
                          .arg(m_result.iterations);
    if (m_result.magnitudeOnly) {
        summary += QLatin1Char(' ') + tr("The ZMA has no phase, so only |Z| was compared.");
    }
    m_summary->setText(summary);
    m_applyButton->setEnabled(true);
}

void ImpedanceFitDialog::invalidateFit()
{
    m_applyButton->setEnabled(false);
    for (QLabel *label : m_fittedValueLabels) {
        label->setText(QStringLiteral("-"));
    }
    m_summary->setText(tr("Press Fit to start."));
}
