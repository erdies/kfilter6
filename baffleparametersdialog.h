/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLEPARAMETERSDIALOG_H
#define BAFFLEPARAMETERSDIALOG_H

#include "bafflemodel.h"
#include "floorreflectionmodel.h"
#include "kfilterprojectio.h"

#include <QDialog>

#include <array>

class BaffleGeometryPreview;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QTabWidget;
class QWidget;

/**
 * Per-driver editor for Baffle / Diffraction and Floor Reflection settings.
 *
 * The dialog exposes both the complex Simple Baffle Step and the on-axis
 * Rectangular Edge Diffraction model. Patch 220 adds an optional ideal rigid
 * floor-contact boundary for Sharp rectangular geometry; Patch 227 adds the
 * independent receiver-dependent Floor Reflection product controls. Field edits
 * are mirrored temporarily into the current KFilterDoc model for live preview. A
 * driver drag updates only the geometry widget and X/Y controls while moving;
 * the final position enters the live-preview model on mouse release. Apply/OK
 * define the new committed project state; Cancel restores the last committed
 * state.
 */
class BaffleParametersDialog : public QDialog
{
    Q_OBJECT

public:
    using BaffleSettingsPerDriver =
        std::array<BaffleSettings, KFilterProjectIo::DriverCount>;
    using FloorReflectionSettingsPerDriver =
        std::array<FloorReflectionSettings, KFilterProjectIo::DriverCount>;

    explicit BaffleParametersDialog(BaffleSettingsPerDriver& settings,
                                    FloorReflectionSettingsPerDriver& floorReflectionSettings,
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
        QComboBox *boundaryCondition = nullptr;
        QDoubleSpinBox *width = nullptr;
        QDoubleSpinBox *height = nullptr;
        QDoubleSpinBox *driverX = nullptr;
        QDoubleSpinBox *driverY = nullptr;
        QComboBox *leftEdgeTreatment = nullptr;
        QDoubleSpinBox *leftChamferSetback = nullptr;
        QComboBox *rightEdgeTreatment = nullptr;
        QDoubleSpinBox *rightChamferSetback = nullptr;
        QLabel *midpoint = nullptr;
        BaffleGeometryPreview *geometryPreview = nullptr;
        QCheckBox *showResponse = nullptr;
        QLabel *responseStatus = nullptr;

        QCheckBox *floorReflectionEnabled = nullptr;
        QDoubleSpinBox *cabinetBottomAboveFloor = nullptr;
        QDoubleSpinBox *listenerHeightAboveFloor = nullptr;
        QDoubleSpinBox *listeningDistance = nullptr;
        QComboBox *floorSurface = nullptr;
        QLabel *floorReflectionStatus = nullptr;
    };

    QWidget *createDriverPage(int driverIndex);
    void loadFromWorkingModel();
    void loadDriverPage(int driverIndex);
    void writePageToWorkingModel(int driverIndex);
    void updatePageState(int driverIndex);
    void updateMidpointLabel(int driverIndex);
    void updateGeometryPreview(int driverIndex);
    void updateResponseStatus(int driverIndex);
    void updateFloorReflectionStatus(int driverIndex);
    void previewWorkingModel();
    void applyToModel();

    BaffleSettingsPerDriver& m_settings;
    FloorReflectionSettingsPerDriver& m_floorReflectionSettings;
    BaffleSettingsPerDriver m_committedSettings;
    BaffleSettingsPerDriver m_workingSettings;
    FloorReflectionSettingsPerDriver m_committedFloorReflectionSettings;
    FloorReflectionSettingsPerDriver m_workingFloorReflectionSettings;
    std::array<DriverPage, KFilterProjectIo::DriverCount> m_pages;
    QTabWidget *m_tabs = nullptr;
    bool m_loading = false;
};

#endif // BAFFLEPARAMETERSDIALOG_H
