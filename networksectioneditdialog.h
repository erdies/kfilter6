/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef NETWORKSECTIONEDITDIALOG_H
#define NETWORKSECTIONEDITDIALOG_H

#include <QDialog>
#include <QString>

class QTimer;

class QDialogButtonBox;
class QLabel;
class QDoubleSpinBox;
class QSlider;

/**
 * Small Qt6 editor for one logical network section group.
 *
 * The dialog intentionally edits one R/C/L triple only.  It mirrors the
 * NetworkParametersDialog display units: Ohm for R, uF for C and mH for L.
 */
class NetworkSectionEditDialog : public QDialog
{
    Q_OBJECT

public:
    struct Values
    {
        double resistanceOhm = 0.0;
        double capacitanceMicroFarad = 0.0;
        double inductanceMilliHenry = 0.0;
    };

    explicit NetworkSectionEditDialog(int driverIndex,
                                      int sectionIndex,
                                      const QString& groupName,
                                      const Values& initialValues,
                                      QWidget *parent = nullptr);

    Values values() const;
    Values originalValues() const;
    bool currentValues(Values *values, QString *errorMessage = nullptr) const;

signals:
    void previewValuesChanged(const Values& values);

public slots:
    void accept() override;
    void reject() override;

private slots:
    void handleInputChanged();
    void validateInput();
    void emitPreviewValues();

private:
    void schedulePreview(int intervalMilliseconds, bool restartActiveTimer);
    void scheduleTextPreview();
    void scheduleSliderPreview();
    void handleVariationSliderChanged(QSlider *slider,
                                      QDoubleSpinBox *field,
                                      double baseValue,
                                      int position);
    void syncSlidersFromFields();
    void syncSliderFromField(QSlider *slider,
                             QDoubleSpinBox *field,
                             double& baseValue);
    void setVariationSliderEnabled(QSlider *slider, bool enabled);
    static double valueFromSliderPosition(double baseValue, int position);
    static int sliderPositionFromValue(double baseValue, double value);

    bool readField(QDoubleSpinBox *field,
                   const QString& label,
                   double& value,
                   QString *errorMessage = nullptr) const;
    bool validateFields(QString *errorMessage = nullptr) const;

    QDoubleSpinBox *m_resistanceEdit = nullptr;
    QDoubleSpinBox *m_capacitanceEdit = nullptr;
    QDoubleSpinBox *m_inductanceEdit = nullptr;
    QSlider *m_resistanceSlider = nullptr;
    QSlider *m_capacitanceSlider = nullptr;
    QSlider *m_inductanceSlider = nullptr;
    QLabel *m_validationLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QTimer *m_previewTimer = nullptr;
    Values m_values;
    Values m_originalValues;
    double m_resistanceSliderBase = 0.0;
    double m_capacitanceSliderBase = 0.0;
    double m_inductanceSliderBase = 0.0;
    bool m_updatingVariationControls = false;
};

#endif // NETWORKSECTIONEDITDIALOG_H
