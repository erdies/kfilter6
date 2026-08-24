/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef DRIVERPARAMETERSDIALOG_H
#define DRIVERPARAMETERSDIALOG_H

#include "driver.h"
#include "kfilterprojectio.h"

#include <QDialog>

#include <array>

class KFilterDoc;
class QTimer;

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QWidget;

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
    explicit DriverParametersDialog(KFilterDoc& document,
                                    QWidget *parent = nullptr,
                                    int initialDriverIndex = 0);

    int currentDriverIndex() const;

signals:
    void parametersApplied();
    void parametersPreviewed();
    void parametersRestored();

public slots:
    void accept() override;
    void reject() override;

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
        QDoubleSpinBox *ql = nullptr;
        QDoubleSpinBox *fb = nullptr;
        QPushButton *hogeButton = nullptr;
        QDoubleSpinBox *tubeDiameter = nullptr;
        QDoubleSpinBox *tubeLength = nullptr;
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
    enum class ApplyMode
    {
        Preview,
        Commit
    };

    void loadFromDrivers();
    void loadPageFromDriver(int index, bool useTubeDiameterOverride = false, double tubeDiameterCm = 0.0);
    bool applyToDrivers(ApplyMode mode, QString *errorMessage = nullptr);
    void rememberCommittedState();
    void restoreCommittedState();
    void schedulePreview();
    void emitPreview();
    void connectPreviewSignals(DriverPage& page, int index);
    bool readSpinBoxValue(const QDoubleSpinBox *spinBox,
                          const QString& label,
                          int driverIndex,
                          double& value,
                          QString *errorMessage = nullptr) const;
    void importDriver(int index);
    void exportDriver(int index);
    void updateQtsForPage(DriverPage& page);
    void updateEnclosureFieldStates(DriverPage& page);
    void updateTubeLengthForPage(DriverPage& page);
    void updateTubeDiameterForPage(DriverPage& page);
    void calculateHogeForPage(DriverPage& page);

    KFilterDoc& m_document;
    driver (&m_drivers)[KFilterProjectIo::DriverCount];
    std::array<DriverPage, KFilterProjectIo::DriverCount> m_pages;
    std::array<driver, KFilterProjectIo::DriverCount> m_committedDrivers;
    KFilterProjectIo::MeasurementCurves m_committedMeasurementCurves;
    KFilterProjectIo::MeasurementHiddenStates m_committedMeasurementHiddenStates{};
    KFilterProjectIo::ActiveFilterChains m_committedActiveFilterChains;
    KFilterProjectIo::BaffleSettingsPerDriver m_committedBaffleSettings;
    KFilterProjectIo::FloorReflectionSettingsPerDriver m_committedFloorReflectionSettings;
    bool m_committedMeasurementMergeEnabled = false;
    QTabWidget *m_tabs = nullptr;
    QTimer *m_previewTimer = nullptr;
    bool m_loadingFromDrivers = false;
    bool m_restoringDrivers = false;
};

#endif // DRIVERPARAMETERSDIALOG_H
