/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef MEASUREMENTIMPORTDIALOG_H
#define MEASUREMENTIMPORTDIALOG_H

#include "correctioncurveimport.h"
#include "measurementcurveparser.h"

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class MeasurementImportPreview;

class MeasurementImportDialog : public QDialog
{
    Q_OBJECT

public:
    MeasurementImportDialog(const QString& sourceFileName,
                            const QString& driverLabel,
                            const KFilterMeasurementParseResult& parseResult,
                            QWidget *parent = nullptr);

    KFilterMeasurementCurve correctionCurve() const;
    KFilterCorrectionImportSettings importSettings() const;
    KFilterCorrectionImportResult importResult() const;

private:
    void updateImportResult();
    KFilterCorrectionImportSettings settingsFromUi() const;

    KFilterMeasurementParseResult m_parseResult;
    KFilterCorrectionImportResult m_importResult;

    QDoubleSpinBox *m_calibrationMinSpin = nullptr;
    QDoubleSpinBox *m_calibrationMaxSpin = nullptr;
    QDoubleSpinBox *m_manualOffsetSpin = nullptr;
    QDoubleSpinBox *m_correctionMinSpin = nullptr;
    QDoubleSpinBox *m_correctionMaxSpin = nullptr;
    QCheckBox *m_lowerFadeCheck = nullptr;
    QDoubleSpinBox *m_lowerFadeSpin = nullptr;
    QCheckBox *m_upperFadeCheck = nullptr;
    QDoubleSpinBox *m_upperFadeSpin = nullptr;
    QLabel *m_referenceMedianValue = nullptr;
    QLabel *m_automaticOffsetValue = nullptr;
    QLabel *m_effectiveOffsetValue = nullptr;
    QLabel *m_validationLabel = nullptr;
    QLabel *m_warningLabel = nullptr;
    MeasurementImportPreview *m_preview = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // MEASUREMENTIMPORTDIALOG_H
