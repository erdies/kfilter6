/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef DRIVERPARAMETERSDIALOG_H
#define DRIVERPARAMETERSDIALOG_H

#include "kfilterprojectio.h"

#include <QDialog>

#include <array>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QTabWidget;
class QWidget;
class driver;

/**
 * Temporary Qt6 driver parameter dialog used during the KDE3 -> Qt6/KF6 port.
 *
 * The legacy driverinput dialog still depends on Qt3/KDE3 APIs. This dialog is
 * intentionally small and keeps the application usable while the original UI is
 * ported or replaced step by step.
 */
class DriverParametersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DriverParametersDialog(driver (&drivers)[KFilterProjectIo::DriverCount],
                                    QWidget *parent = nullptr,
                                    int initialDriverIndex = 0);

    int currentDriverIndex() const;

signals:
    void parametersApplied();

public slots:
    void accept() override;

private slots:
    void applyClicked();

private:
    struct DriverPage
    {
        QWidget *page = nullptr;
        QLineEdit *title = nullptr;
        QDoubleSpinBox *rdc = nullptr;
        QDoubleSpinBox *lspMilliHenry = nullptr;
        QDoubleSpinBox *f0 = nullptr;
        QDoubleSpinBox *qts = nullptr;
        QDoubleSpinBox *qes = nullptr;
        QDoubleSpinBox *qms = nullptr;
        QDoubleSpinBox *vas = nullptr;
        QDoubleSpinBox *dm = nullptr;
        QDoubleSpinBox *vb = nullptr;
        QDoubleSpinBox *fb = nullptr;
        QDoubleSpinBox *tubeDiameter = nullptr;
        QLineEdit *tubeLength = nullptr;
        QDoubleSpinBox *v2 = nullptr;
        QComboBox *alignmentProposal = nullptr;
        QDoubleSpinBox *gainDb = nullptr;
        QCheckBox *pressureActive = nullptr;
        QCheckBox *impedanceActive = nullptr;
        QCheckBox *summaryActive = nullptr;
        QCheckBox *scalarSummaryActive = nullptr;
        QCheckBox *impedanceSummaryActive = nullptr;
        QCheckBox *invertPhase = nullptr;
        QCheckBox *fullCircuit = nullptr;
    };

    QWidget *createDriverPage(int index);
    void loadFromDrivers();
    void applyToDrivers();
    void updateQtsForPage(DriverPage& page);
    void updateTubeLengthForPage(DriverPage& page);
    void calculateHogeForPage(DriverPage& page);

    driver (&m_drivers)[KFilterProjectIo::DriverCount];
    std::array<DriverPage, KFilterProjectIo::DriverCount> m_pages;
    QTabWidget *m_tabs = nullptr;
};

#endif // DRIVERPARAMETERSDIALOG_H
