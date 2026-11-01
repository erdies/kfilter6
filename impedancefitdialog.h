/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef IMPEDANCEFITDIALOG_H
#define IMPEDANCEFITDIALOG_H

#include "driver.h"
#include "impedancefit.h"
#include "impedancereferencecurve.h"

#include <QDialog>

#include <array>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

/**
 * Fits the LR2 voice coil (Lsp, R2, L2, optionally Rdc) of one driver to its
 * imported impedance ZMA. The dialog only computes; the caller writes the
 * accepted values into the Driver Parameters fields.
 */
class ImpedanceFitDialog : public QDialog
{
    Q_OBJECT

public:
    ImpedanceFitDialog(const driver& drv,
                       const KFilterImpedanceReferenceCurve& zma,
                       const QString& driverLabel,
                       QWidget *parent = nullptr);

    /** Valid after the dialog was accepted. */
    const KFilterImpedanceFitResult& fitResult() const;
    bool rdcFitted() const;

private:
    void runFit();
    void invalidateFit();

    driver m_driver;
    KFilterImpedanceReferenceCurve m_zma;
    KFilterImpedanceFitResult m_result;
    bool m_resultRdcFitted = false;

    QDoubleSpinBox *m_minimumFrequency = nullptr;
    QDoubleSpinBox *m_maximumFrequency = nullptr;
    QCheckBox *m_fitRdc = nullptr;
    std::array<QLabel *, 4> m_fittedValueLabels{};
    QLabel *m_summary = nullptr;
    QPushButton *m_applyButton = nullptr;
};

#endif // IMPEDANCEFITDIALOG_H
