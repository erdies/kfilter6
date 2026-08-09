/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef ACTIVEFILTERPARAMETERSDIALOG_H
#define ACTIVEFILTERPARAMETERSDIALOG_H

#include "activefiltermodel.h"
#include "kfilterprojectio.h"

#include <QDialog>

#include <array>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QWidget;

/**
 * Editor for the project active-filter data model.
 *
 * Changes are mirrored temporarily into the current KFilterDoc model so the
 * diagnostic transfer plot and supported driver simulation can update live.
 * Apply/OK define the new committed project state; Cancel restores the last
 * committed state. Active-filter metadata is persisted by project format v5.
 */
class ActiveFilterParametersDialog : public QDialog
{
    Q_OBJECT

public:
    using ActiveFilterChains =
        std::array<ActiveFilterChain, KFilterProjectIo::DriverCount>;

    explicit ActiveFilterParametersDialog(ActiveFilterChains& chains,
                                          QWidget *parent = nullptr,
                                          int initialDriverIndex = 0);

    int currentDriverIndex() const;

signals:
    void parametersPreviewChanged();
    void parametersApplied();

public slots:
    void accept() override;
    void reject() override;

private slots:
    void applyClicked();

private:
    struct DriverPage
    {
        QWidget *page = nullptr;
        QCheckBox *activeProcessing = nullptr;
        QTableWidget *sections = nullptr;
        QPushButton *addButton = nullptr;
        QPushButton *removeButton = nullptr;
        QPushButton *moveUpButton = nullptr;
        QPushButton *moveDownButton = nullptr;
        QGroupBox *editorGroup = nullptr;
        QCheckBox *sectionEnabled = nullptr;
        QComboBox *type = nullptr;
        QComboBox *characteristic = nullptr;
        QSpinBox *order = nullptr;
        QDoubleSpinBox *frequency1 = nullptr;
        QDoubleSpinBox *frequency2 = nullptr;
        QDoubleSpinBox *q = nullptr;
        QDoubleSpinBox *gain = nullptr;
        QDoubleSpinBox *delay = nullptr;
        QComboBox *polarity = nullptr;
        QCheckBox *showResponse = nullptr;
        QLabel *responseStatus = nullptr;
    };

    QWidget *createDriverPage(int driverIndex);
    void connectEditorSignals(int driverIndex);
    void loadFromWorkingModel();
    void refreshSectionTable(int driverIndex, int preferredRow = -1);
    void addSection(int driverIndex);
    void removeSelectedSection(int driverIndex);
    void moveSelectedSection(int driverIndex, int direction);
    void selectionChanged(int driverIndex);
    void selectedTypeChanged(int driverIndex);
    void writeEditorToSelectedSection(int driverIndex);
    void loadEditorFromSelectedSection(int driverIndex);
    void updatePageState(int driverIndex);
    void updateResponseStatus(int driverIndex);
    void updateEditorControlState(int driverIndex);
    void refreshSectionRow(int driverIndex, int row);
    void previewWorkingModel();
    void applyToModel();

    QString sectionTypeText(ActiveFilterType type) const;
    QString characteristicText(ActiveFilterCharacteristic characteristic) const;
    QString characteristicSummary(const ActiveFilterSection& section) const;
    QString orderSummary(const ActiveFilterSection& section) const;
    QString frequencySummary(const ActiveFilterSection& section) const;
    QString extraSummary(const ActiveFilterSection& section) const;

    ActiveFilterChains& m_chains;
    ActiveFilterChains m_committedChains;
    ActiveFilterChains m_workingChains;
    std::array<DriverPage, KFilterProjectIo::DriverCount> m_pages;
    QTabWidget *m_tabs = nullptr;
    bool m_loadingEditor = false;
};

#endif // ACTIVEFILTERPARAMETERSDIALOG_H
