/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLEPARAMETERSDIALOG_H
#define BAFFLEPARAMETERSDIALOG_H

#include "bafflemodel.h"
#include "kfilterprojectio.h"

#include <QDialog>

#include <array>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QTabWidget;
class QWidget;

/**
 * Per-driver editor for Baffle / Diffraction project settings.
 *
 * Patch 192 exposes both the complex Simple Baffle Step and the initial
 * on-axis far-field Rectangular Edge Diffraction model. Changes are mirrored
 * temporarily into the current KFilterDoc model for live preview. Apply/OK
 * define the new committed project state; Cancel restores the last committed
 * state.
 */
class BaffleParametersDialog : public QDialog
{
    Q_OBJECT

public:
    using BaffleSettingsPerDriver =
        std::array<BaffleSettings, KFilterProjectIo::DriverCount>;

    explicit BaffleParametersDialog(BaffleSettingsPerDriver& settings,
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
        QCheckBox *enabled = nullptr;
        QComboBox *model = nullptr;
        QDoubleSpinBox *width = nullptr;
        QDoubleSpinBox *height = nullptr;
        QDoubleSpinBox *driverX = nullptr;
        QDoubleSpinBox *driverY = nullptr;
        QLabel *midpoint = nullptr;
        QCheckBox *showResponse = nullptr;
        QLabel *responseStatus = nullptr;
    };

    QWidget *createDriverPage(int driverIndex);
    void loadFromWorkingModel();
    void loadDriverPage(int driverIndex);
    void writePageToWorkingModel(int driverIndex);
    void updatePageState(int driverIndex);
    void updateMidpointLabel(int driverIndex);
    void updateResponseStatus(int driverIndex);
    void previewWorkingModel();
    void applyToModel();

    BaffleSettingsPerDriver& m_settings;
    BaffleSettingsPerDriver m_committedSettings;
    BaffleSettingsPerDriver m_workingSettings;
    std::array<DriverPage, KFilterProjectIo::DriverCount> m_pages;
    QTabWidget *m_tabs = nullptr;
    bool m_loading = false;
};

#endif // BAFFLEPARAMETERSDIALOG_H
