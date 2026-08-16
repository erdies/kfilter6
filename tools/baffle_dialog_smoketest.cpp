/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleparametersdialog.h"
#include "bafflegeometrypreview.h"
#include "networkvalueutils.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QTabWidget>

#include <cmath>
#include <iostream>

namespace
{
bool near(double left, double right, double tolerance = 1.0e-6)
{
    return std::abs(left - right) <= tolerance;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    BaffleParametersDialog::BaffleSettingsPerDriver settings{};
    BaffleParametersDialog::FloorReflectionSettingsPerDriver floorReflectionSettings{};
    settings[0].enabled = true;
    settings[0].widthMm = 231.0;
    settings[0].showResponseInPlot = true;

    BaffleParametersDialog dialog(settings, floorReflectionSettings, nullptr, 2);

    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("baffleDriverTabs"));
    if (tabs == nullptr || tabs->count() != KFilterProjectIo::DriverCount || dialog.currentDriverIndex() != 2) {
        std::cerr << "Baffle driver tabs or initial selection are invalid\n";
        return 1;
    }

    if (dialog.findChild<QLabel *>(QStringLiteral("baffleStageNotice")) != nullptr) {
        std::cerr << "Patch-250 obsolete explanatory notice is still present\n";
        return 1;
    }

    auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("baffleEnableDriver1"));
    auto *model = dialog.findChild<QComboBox *>(QStringLiteral("baffleModelCombo1"));
    auto *boundary = dialog.findChild<QComboBox *>(QStringLiteral("baffleBoundaryConditionCombo1"));
    auto *width = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleWidthSpin1"));
    auto *height = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleHeightSpin1"));
    auto *driverX = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleDriverXSpin1"));
    auto *driverY = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleDriverYSpin1"));
    auto *leftTreatment = dialog.findChild<QComboBox *>(QStringLiteral("baffleLeftEdgeTreatmentCombo1"));
    auto *leftChamfer = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleLeftChamferSpin1"));
    auto *rightTreatment = dialog.findChild<QComboBox *>(QStringLiteral("baffleRightEdgeTreatmentCombo1"));
    auto *rightChamfer = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("baffleRightChamferSpin1"));
    auto *midpoint = dialog.findChild<QLabel *>(QStringLiteral("baffleMidpointLabel1"));
    auto *geometryPreviewWidget = dialog.findChild<QWidget *>(
        QStringLiteral("baffleGeometryPreview1"));
    auto *geometryPreview = dynamic_cast<BaffleGeometryPreview *>(geometryPreviewWidget);
    auto *showResponse = dialog.findChild<QCheckBox *>(QStringLiteral("baffleShowResponseDriver1"));
    auto *status = dialog.findChild<QLabel *>(QStringLiteral("baffleResponseStatus1"));
    if (enabled == nullptr || model == nullptr || boundary == nullptr || width == nullptr || height == nullptr ||
        driverX == nullptr || driverY == nullptr || leftTreatment == nullptr ||
        leftChamfer == nullptr || rightTreatment == nullptr || rightChamfer == nullptr ||
        midpoint == nullptr || geometryPreview == nullptr || showResponse == nullptr || status == nullptr) {
        std::cerr << "Baffle controls are missing\n";
        return 2;
    }

    if (!enabled->isChecked() || model->count() != 2 ||
        model->currentData().toInt() != static_cast<int>(BaffleModel::SimpleBaffleStep) ||
        boundary->currentData().toInt() != static_cast<int>(BaffleBoundaryCondition::FreeField) ||
        boundary->isEnabled() ||
        !near(width->value(), 231.0) || !showResponse->isChecked() ||
        !near(geometryPreview->baffleWidthMm(), 231.0) ||
        !near(geometryPreview->baffleHeightMm(), BaffleSettings{}.heightMm) ||
        !near(geometryPreview->driverXmm(), BaffleSettings{}.driverXmm) ||
        !near(geometryPreview->driverYmm(), BaffleSettings{}.driverYmm) ||
        !near(geometryPreview->leftChamferSetbackMm(), 0.0) ||
        !near(geometryPreview->rightChamferSetbackMm(), 0.0) ||
        leftTreatment->currentData().toInt() != static_cast<int>(BaffleSideEdgeTreatment::Sharp) ||
        rightTreatment->currentData().toInt() != static_cast<int>(BaffleSideEdgeTreatment::Sharp) ||
        !midpoint->text().contains(QStringLiteral("497")) ||
        !status->text().contains(QStringLiteral("valid complex Simple")) ||
        height->isEnabled() || driverX->isEnabled() || driverY->isEnabled() ||
        leftTreatment->isEnabled() || rightTreatment->isEnabled() ||
        leftChamfer->isEnabled() || rightChamfer->isEnabled() ||
        geometryPreview->driverDragEnabled()) {
        std::cerr << "Persisted Stage-1 state was not loaded into the dialog correctly\n";
        return 3;
    }

    // Patch 199: the geometry preview is presentation-only, follows focus and
    // starts with the values already shown in the geometry controls.
    tabs->setCurrentIndex(0);
    dialog.show();
    QApplication::processEvents();
    width->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::BaffleWidth) {
        std::cerr << "Baffle width focus did not highlight the geometry preview\n";
        return 18;
    }
    enabled->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::None) {
        std::cerr << "Geometry preview did not return to neutral state after focus left the geometry fields\n";
        return 19;
    }

    int previewCount = 0;
    int applyCount = 0;
    QObject::connect(&dialog, &BaffleParametersDialog::parametersPreviewChanged,
                     [&previewCount]() { ++previewCount; });
    QObject::connect(&dialog, &BaffleParametersDialog::parametersApplied,
                     [&applyCount]() { ++applyCount; });

    width->setValue(300.0);
    QApplication::processEvents();
    if (!near(settings[0].widthMm, 300.0) || previewCount == 0 ||
        !near(geometryPreview->baffleWidthMm(), 300.0) ||
        !midpoint->text().contains(QStringLiteral("383"))) {
        std::cerr << "Baffle width change was not mirrored into the live preview model\n";
        return 4;
    }

    // Patch 195/197: Baffle geometry entry must match the other numeric
    // dialogs: accept decimal comma as well as decimal point, including the
    // displayed unit suffix. Mixed separators are a parser-level contract;
    // test that directly rather than relying on QDoubleSpinBox correction of
    // programmatically injected invalid QLineEdit text.
    auto *widthEditor = width->findChild<QLineEdit *>();
    if (widthEditor == nullptr) {
        std::cerr << "Baffle width editor is missing\n";
        return 14;
    }

    widthEditor->setText(QStringLiteral("231,5 mm"));
    width->interpretText();
    QApplication::processEvents();
    if (!near(width->value(), 231.5) || !near(settings[0].widthMm, 231.5)) {
        std::cerr << "Decimal-comma Baffle input was not accepted\n";
        return 15;
    }

    widthEditor->setText(QStringLiteral("232.5 mm"));
    width->interpretText();
    QApplication::processEvents();
    if (!near(width->value(), 232.5) || !near(settings[0].widthMm, 232.5)) {
        std::cerr << "Decimal-point Baffle input regressed\n";
        return 16;
    }

    double mixedSeparatorValue = 0.0;
    if (NetworkValueUtils::parseDisplayValue(QStringLiteral("23,1.5"),
                                             mixedSeparatorValue,
                                             0.0,
                                             10000.0)) {
        std::cerr << "Mixed decimal separators were accepted by the shared parser\n";
        return 17;
    }

    // Patch 192: Stage 2 is selectable and exposes only the minimum geometry
    // fields required by the productive rectangular DSP.
    const int rectangularIndex = model->findData(static_cast<int>(BaffleModel::RectangularEdgeDiffraction));
    if (rectangularIndex < 0) {
        std::cerr << "Rectangular Edge Diffraction model is missing\n";
        return 5;
    }
    model->setCurrentIndex(rectangularIndex);
    QApplication::processEvents();
    if (settings[0].model != BaffleModel::RectangularEdgeDiffraction ||
        !boundary->isEnabled() ||
        !height->isEnabled() || !driverX->isEnabled() || !driverY->isEnabled() ||
        !leftTreatment->isEnabled() || !rightTreatment->isEnabled() ||
        leftChamfer->isEnabled() || rightChamfer->isEnabled() ||
        !geometryPreview->driverDragEnabled() ||
        !midpoint->text().contains(QStringLiteral("not applicable")) ||
        !status->text().contains(QStringLiteral("invalid geometry"))) {
        std::cerr << "Stage-2 selection or initial invalid-geometry state is inconsistent\n";
        return 6;
    }

    width->setValue(231.0);
    height->setValue(900.0);
    driverX->setValue(90.0);
    driverY->setValue(310.0);
    QApplication::processEvents();
    if (!near(settings[0].widthMm, 231.0) || !near(settings[0].heightMm, 900.0) ||
        !near(settings[0].driverXmm, 90.0) || !near(settings[0].driverYmm, 310.0) ||
        !near(geometryPreview->baffleWidthMm(), 231.0) ||
        !near(geometryPreview->baffleHeightMm(), 900.0) ||
        !near(geometryPreview->driverXmm(), 90.0) ||
        !near(geometryPreview->driverYmm(), 310.0) ||
        !near(geometryPreview->leftChamferSetbackMm(), 0.0) ||
        !near(geometryPreview->rightChamferSetbackMm(), 0.0) ||
        !status->text().contains(QStringLiteral("sharp side edges"))) {
        std::cerr << "Valid Rectangular Edge Diffraction geometry was not previewed\n";
        return 7;
    }

    // Patch 220: Rigid floor contact is a Rectangular-only boundary condition.
    // It is productive only for Sharp side edges in this first stage.
    const int rigidFloorIndex = boundary->findData(
        static_cast<int>(BaffleBoundaryCondition::RigidFloorContactDiffractionOnly));
    if (rigidFloorIndex < 0) {
        std::cerr << "Rigid floor boundary condition is missing\n";
        return 34;
    }
    boundary->setCurrentIndex(rigidFloorIndex);
    QApplication::processEvents();
    const int chamferIndex = leftTreatment->findData(static_cast<int>(BaffleSideEdgeTreatment::Chamfer45));
    const int rightChamferIndex = rightTreatment->findData(static_cast<int>(BaffleSideEdgeTreatment::Chamfer45));
    const bool leftChamferItemEnabled =
        chamferIndex >= 0 && (leftTreatment->model()->flags(
            leftTreatment->model()->index(chamferIndex, 0)) & Qt::ItemIsEnabled);
    const bool rightChamferItemEnabled =
        rightChamferIndex >= 0 && (rightTreatment->model()->flags(
            rightTreatment->model()->index(rightChamferIndex, 0)) & Qt::ItemIsEnabled);
    if (settings[0].boundaryCondition !=
            BaffleBoundaryCondition::RigidFloorContactDiffractionOnly ||
        !status->text().contains(QStringLiteral("rigid floor contact"), Qt::CaseInsensitive) ||
        leftChamferItemEnabled || rightChamferItemEnabled) {
        std::cerr << "Rigid floor mode did not activate the Sharp-only productive state\n";
        return 35;
    }

    const int freeFieldIndex = boundary->findData(
        static_cast<int>(BaffleBoundaryCondition::FreeField));
    boundary->setCurrentIndex(freeFieldIndex);
    QApplication::processEvents();

    const int chamferIndexForTest = leftTreatment->findData(static_cast<int>(BaffleSideEdgeTreatment::Chamfer45));
    if (chamferIndexForTest < 0 || rightTreatment->findData(static_cast<int>(BaffleSideEdgeTreatment::Chamfer45)) < 0) {
        std::cerr << "Chamfer 45 edge treatment is missing\n";
        return 24;
    }
    leftTreatment->setCurrentIndex(chamferIndexForTest);
    rightTreatment->setCurrentIndex(chamferIndexForTest);
    leftChamfer->setValue(25.0);
    rightChamfer->setValue(30.0);
    QApplication::processEvents();
    if (settings[0].leftEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45 ||
        settings[0].rightEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45 ||
        !near(settings[0].leftChamferSetbackMm, 25.0) ||
        !near(settings[0].rightChamferSetbackMm, 30.0) ||
        !leftChamfer->isEnabled() || !rightChamfer->isEnabled() ||
        !near(geometryPreview->leftChamferSetbackMm(), 25.0) ||
        !near(geometryPreview->rightChamferSetbackMm(), 30.0) ||
        !status->text().contains(QStringLiteral("45-degree side chamfer"))) {
        std::cerr << "Chamfer edge controls were not mirrored into the live preview model\n";
        return 25;
    }
    const bool rigidFloorItemEnabledWithChamfer =
        boundary->model()->flags(boundary->model()->index(rigidFloorIndex, 0)) & Qt::ItemIsEnabled;
    if (rigidFloorItemEnabledWithChamfer) {
        std::cerr << "Rigid floor option remained selectable with active chamfer geometry\n";
        return 36;
    }

    auto *leftChamferEditor = leftChamfer->findChild<QLineEdit *>();
    if (leftChamferEditor == nullptr) {
        std::cerr << "Left chamfer editor is missing\n";
        return 28;
    }
    leftChamferEditor->setText(QStringLiteral("25,5 mm"));
    leftChamfer->interpretText();
    QApplication::processEvents();
    if (!near(leftChamfer->value(), 25.5) || !near(settings[0].leftChamferSetbackMm, 25.5) ||
        !near(geometryPreview->leftChamferSetbackMm(), 25.5)) {
        std::cerr << "Decimal-comma chamfer input was not accepted\n";
        return 29;
    }
    leftChamfer->setValue(25.0);
    QApplication::processEvents();

    // A driver centre that falls on/inside the chamfered strip is invalid.
    leftChamfer->setValue(90.0);
    QApplication::processEvents();
    if (!status->text().contains(QStringLiteral("invalid geometry"))) {
        std::cerr << "Chamfer consuming the driver centre was not rejected in the dialog\n";
        return 30;
    }
    leftChamfer->setValue(25.0);
    QApplication::processEvents();

    // Patch 215: direct driver dragging is UI-only until mouse release. X/Y
    // controls and the symbol move immediately, but the working/project view
    // and therefore the expensive Baffle response refresh are triggered once
    // at the end of the drag. Active chamfer strips are hard movement bounds.
    driverX->setValue(115.5);
    driverY->setValue(450.0);
    QApplication::processEvents();
    const double dragStartX = settings[0].driverXmm;
    const double dragStartY = settings[0].driverYmm;
    const int dragPreviewCountBefore = previewCount;

    // At a 50/50 driver position the symbol centre is the baffle-area centre.
    // The preview reserves 34/4 px horizontally and 18/4 px vertically for
    // dimensions, shifting that centre by +15/+7 px from the widget centre.
    const QPointF driverCentre(geometryPreview->width() / 2.0 + 15.0,
                               geometryPreview->height() / 2.0 + 7.0);
    QMouseEvent pressEvent(QEvent::MouseButtonPress,
                           driverCentre,
                           QPointF(geometryPreview->mapToGlobal(driverCentre.toPoint())),
                           Qt::LeftButton,
                           Qt::LeftButton,
                           Qt::NoModifier);
    QApplication::sendEvent(geometryPreview, &pressEvent);

    const QPointF farLeftDrag(-200.0, driverCentre.y() + 20.0);
    QMouseEvent moveEvent(QEvent::MouseMove,
                          farLeftDrag,
                          QPointF(geometryPreview->mapToGlobal(farLeftDrag.toPoint())),
                          Qt::NoButton,
                          Qt::LeftButton,
                          Qt::NoModifier);
    QApplication::sendEvent(geometryPreview, &moveEvent);
    QApplication::processEvents();
    if (!near(driverX->value(), 25.1) ||
        near(driverY->value(), dragStartY) ||
        !near(geometryPreview->driverXmm(), driverX->value()) ||
        !near(geometryPreview->driverYmm(), driverY->value()) ||
        !near(settings[0].driverXmm, dragStartX) ||
        !near(settings[0].driverYmm, dragStartY) ||
        previewCount != dragPreviewCountBefore) {
        std::cerr << "Driver drag updated the model before mouse release or ignored chamfer bounds\n";
        return 31;
    }

    QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                             farLeftDrag,
                             QPointF(geometryPreview->mapToGlobal(farLeftDrag.toPoint())),
                             Qt::LeftButton,
                             Qt::NoButton,
                             Qt::NoModifier);
    QApplication::sendEvent(geometryPreview, &releaseEvent);
    QApplication::processEvents();
    if (!near(settings[0].driverXmm, driverX->value()) ||
        !near(settings[0].driverYmm, driverY->value()) ||
        !near(settings[0].driverXmm, 25.1) ||
        previewCount != dragPreviewCountBefore + 1) {
        std::cerr << "Driver drag was not committed exactly once on mouse release\n";
        return 32;
    }

    // Restore the established Stage-3B values for the remaining Apply/Cancel
    // regression checks.
    driverX->setValue(90.0);
    driverY->setValue(310.0);
    QApplication::processEvents();

    leftChamfer->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::LeftChamfer) {
        std::cerr << "Left chamfer focus did not highlight the geometry preview\n";
        return 26;
    }
    rightChamfer->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::RightChamfer) {
        std::cerr << "Right chamfer focus did not highlight the geometry preview\n";
        return 27;
    }

    height->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::BaffleHeight) {
        std::cerr << "Baffle height focus did not highlight the geometry preview\n";
        return 20;
    }
    driverX->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::DriverX) {
        std::cerr << "Driver X focus did not highlight the geometry preview\n";
        return 21;
    }
    driverY->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::DriverY) {
        std::cerr << "Driver Y focus did not highlight the geometry preview\n";
        return 22;
    }

    showResponse->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if (geometryPreview->currentHighlight() != BaffleGeometryPreview::Highlight::None) {
        std::cerr << "Geometry preview did not clear after leaving the geometry fields\n";
        return 23;
    }

    showResponse->setChecked(false);
    QApplication::processEvents();
    if (settings[0].showResponseInPlot || previewCount < 6) {
        std::cerr << "Diagnostic visibility did not update the live preview model\n";
        return 8;
    }

    enabled->setChecked(false);
    QApplication::processEvents();
    if (settings[0].enabled || width->isEnabled() || height->isEnabled() ||
        driverX->isEnabled() || driverY->isEnabled() || model->isEnabled() ||
        leftTreatment->isEnabled() || rightTreatment->isEnabled() ||
        leftChamfer->isEnabled() || rightChamfer->isEnabled() ||
        geometryPreview->driverDragEnabled() ||
        !status->text().contains(QStringLiteral("bypassed"))) {
        std::cerr << "Baffle master bypass state is inconsistent\n";
        return 9;
    }

    enabled->setChecked(true);
    showResponse->setChecked(true);
    QApplication::processEvents();

    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("baffleDialogButtons"));
    if (buttons == nullptr || buttons->button(QDialogButtonBox::Apply) == nullptr) {
        std::cerr << "Baffle Apply button is missing\n";
        return 10;
    }
    buttons->button(QDialogButtonBox::Apply)->click();
    if (applyCount != 1 || !settings[0].enabled ||
        settings[0].model != BaffleModel::RectangularEdgeDiffraction ||
        !near(settings[0].widthMm, 231.0) || !near(settings[0].heightMm, 900.0) ||
        !near(settings[0].driverXmm, 90.0) || !near(settings[0].driverYmm, 310.0) ||
        settings[0].leftEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45 ||
        settings[0].rightEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45 ||
        !near(settings[0].leftChamferSetbackMm, 25.0) ||
        !near(settings[0].rightChamferSetbackMm, 30.0) ||
        !settings[0].showResponseInPlot) {
        std::cerr << "Apply did not commit the Stage-2 Baffle working copy\n";
        return 11;
    }

    height->setValue(700.0);
    driverY->setValue(250.0);
    rightChamfer->setValue(40.0);
    showResponse->setChecked(false);
    QApplication::processEvents();
    if (!near(settings[0].heightMm, 700.0) || !near(settings[0].driverYmm, 250.0) ||
        !near(settings[0].rightChamferSetbackMm, 40.0) || settings[0].showResponseInPlot) {
        std::cerr << "Post-Apply Stage-2 changes were not previewed\n";
        return 12;
    }

    dialog.reject();
    if (!settings[0].enabled ||
        settings[0].model != BaffleModel::RectangularEdgeDiffraction ||
        !near(settings[0].widthMm, 231.0) || !near(settings[0].heightMm, 900.0) ||
        !near(settings[0].driverXmm, 90.0) || !near(settings[0].driverYmm, 310.0) ||
        settings[0].leftEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45 ||
        settings[0].rightEdgeTreatment != BaffleSideEdgeTreatment::Chamfer45 ||
        !near(settings[0].leftChamferSetbackMm, 25.0) ||
        !near(settings[0].rightChamferSetbackMm, 30.0) ||
        !settings[0].showResponseInPlot) {
        std::cerr << "Cancel did not restore the last applied Stage-3B Baffle state\n";
        return 13;
    }

    // Patch 227: Floor Reflection lives in the same per-driver dialog but is
    // independent of whether Baffle / Diffraction processing itself is enabled.
    BaffleParametersDialog::BaffleSettingsPerDriver floorBaffleSettings{};
    BaffleParametersDialog::FloorReflectionSettingsPerDriver floorSettings{};
    BaffleParametersDialog floorDialog(floorBaffleSettings, floorSettings, nullptr, 0);

    auto *floorGroup = floorDialog.findChild<QGroupBox *>(
        QStringLiteral("floorReflectionGroup1"));
    auto *floorLayout = floorDialog.findChild<QGridLayout *>(
        QStringLiteral("floorReflectionLayout1"));
    auto *floorEnable = floorDialog.findChild<QCheckBox *>(
        QStringLiteral("floorReflectionEnableDriver1"));
    auto *floorHeight = floorDialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("baffleHeightSpin1"));
    auto *floorDriverY = floorDialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("baffleDriverYSpin1"));
    auto *floorWidth = floorDialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("baffleWidthSpin1"));
    auto *floorDriverX = floorDialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("baffleDriverXSpin1"));
    auto *floorModel = floorDialog.findChild<QComboBox *>(
        QStringLiteral("baffleModelCombo1"));
    auto *cabinetBottom = floorDialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("floorReflectionCabinetBottomSpin1"));
    auto *listenerHeight = floorDialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("floorReflectionListenerHeightSpin1"));
    auto *listeningDistance = floorDialog.findChild<QDoubleSpinBox *>(
        QStringLiteral("floorReflectionDistanceSpin1"));
    auto *surface = floorDialog.findChild<QComboBox *>(
        QStringLiteral("floorReflectionSurfaceCombo1"));
    auto *floorStatus = floorDialog.findChild<QLabel *>(
        QStringLiteral("floorReflectionStatus1"));
    auto *floorButtons = floorDialog.findChild<QDialogButtonBox *>(
        QStringLiteral("baffleDialogButtons"));

    if (floorGroup == nullptr || floorLayout == nullptr || floorEnable == nullptr ||
        floorHeight == nullptr || floorDriverY == nullptr || floorWidth == nullptr ||
        floorDriverX == nullptr || floorModel == nullptr || cabinetBottom == nullptr ||
        listenerHeight == nullptr || listeningDistance == nullptr || surface == nullptr ||
        floorStatus == nullptr || floorButtons == nullptr) {
        std::cerr << "Patch-227 Floor Reflection controls are missing\n";
        return 40;
    }

    // Patch 249: keep the two vertical geometry fields in the left form column,
    // place distance/surface in the right column, and let the status information
    // span the remaining full width below both columns.
    auto checkFloorGridPosition = [floorLayout](QWidget *widget,
                                                int expectedRow,
                                                int expectedColumn,
                                                int expectedRowSpan = 1,
                                                int expectedColumnSpan = 1) {
        const int index = floorLayout->indexOf(widget);
        if (index < 0) {
            return false;
        }
        int row = -1;
        int column = -1;
        int rowSpan = -1;
        int columnSpan = -1;
        floorLayout->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
        return row == expectedRow && column == expectedColumn &&
               rowSpan == expectedRowSpan && columnSpan == expectedColumnSpan;
    };
    if (!checkFloorGridPosition(floorEnable, 0, 0, 1, 4) ||
        !checkFloorGridPosition(cabinetBottom, 1, 1) ||
        !checkFloorGridPosition(listenerHeight, 2, 1) ||
        !checkFloorGridPosition(listeningDistance, 1, 3) ||
        !checkFloorGridPosition(surface, 2, 3) ||
        !checkFloorGridPosition(floorStatus, 3, 1, 1, 3)) {
        std::cerr << "Patch-249 compact Floor Reflection grid layout is inconsistent\n";
        return 51;
    }

    if (!floorGroup->title().contains(QStringLiteral("Experimental"), Qt::CaseInsensitive)) {
        std::cerr << "Patch-233 Floor Reflection group is not marked Experimental\n";
        return 50;
    }

    if (floorEnable->isChecked() || cabinetBottom->isEnabled() || listenerHeight->isEnabled() ||
        listeningDistance->isEnabled() || surface->isEnabled() ||
        !near(cabinetBottom->value(), 0.0) || !near(listenerHeight->value(), 1050.0) ||
        !near(listeningDistance->value(), 2500.0) || surface->count() != 2 ||
        surface->itemData(0).toInt() != static_cast<int>(FloorSurfacePreset::HardRigid) ||
        surface->itemData(1).toInt() != static_cast<int>(FloorSurfacePreset::MikiReference10mm100k) ||
        surface->currentData().toInt() != static_cast<int>(FloorSurfacePreset::HardRigid) ||
        !floorStatus->text().contains(QStringLiteral("bypassed"), Qt::CaseInsensitive)) {
        std::cerr << "Patch-227 Floor Reflection defaults are inconsistent\n";
        return 41;
    }

    int floorPreviewCount = 0;
    int floorApplyCount = 0;
    QObject::connect(&floorDialog, &BaffleParametersDialog::parametersPreviewChanged,
                     [&floorPreviewCount]() { ++floorPreviewCount; });
    QObject::connect(&floorDialog, &BaffleParametersDialog::parametersApplied,
                     [&floorApplyCount]() { ++floorApplyCount; });

    floorEnable->setChecked(true);
    QApplication::processEvents();
    if (!floorSettings[0].enabled || !floorHeight->isEnabled() || !floorDriverY->isEnabled() ||
        floorWidth->isEnabled() || floorDriverX->isEnabled() || floorModel->isEnabled() ||
        !cabinetBottom->isEnabled() || !listenerHeight->isEnabled() ||
        !listeningDistance->isEnabled() || !surface->isEnabled() || floorPreviewCount == 0) {
        std::cerr << "Floor Reflection was not independent from the Baffle processing bypass\n";
        return 42;
    }

    floorHeight->setValue(965.0);
    floorDriverY->setValue(245.0);
    cabinetBottom->setValue(0.0);
    listenerHeight->setValue(1050.0);
    listeningDistance->setValue(2500.0);
    QApplication::processEvents();
    if (!near(floorBaffleSettings[0].heightMm, 965.0) ||
        !near(floorBaffleSettings[0].driverYmm, 245.0) ||
        !near(floorSettings[0].cabinetBottomAboveFloorMm, 0.0) ||
        !near(floorSettings[0].listenerHeightAboveFloorMm, 1050.0) ||
        !near(floorSettings[0].horizontalDistanceMm, 2500.0) ||
        floorSettings[0].surfacePreset != FloorSurfacePreset::HardRigid ||
        !floorStatus->text().contains(QStringLiteral("720.0 mm"))) {
        std::cerr << "Floor Reflection live preview or derived source height is inconsistent\n";
        return 43;
    }

    surface->setCurrentIndex(1);
    QApplication::processEvents();
    if (floorSettings[0].surfacePreset != FloorSurfacePreset::MikiReference10mm100k ||
        !floorStatus->text().contains(QStringLiteral("Miki reference"), Qt::CaseInsensitive) ||
        !floorStatus->text().contains(QStringLiteral("experimental"), Qt::CaseInsensitive)) {
        std::cerr << "Patch-229 Miki reference preset was not previewed in the dialog\n";
        return 49;
    }

    auto *listenerEditor = listenerHeight->findChild<QLineEdit *>();
    if (listenerEditor == nullptr) {
        std::cerr << "Floor Reflection listener-height editor is missing\n";
        return 44;
    }
    listenerEditor->setText(QStringLiteral("1075,5 mm"));
    listenerHeight->interpretText();
    QApplication::processEvents();
    if (!near(listenerHeight->value(), 1075.5) ||
        !near(floorSettings[0].listenerHeightAboveFloorMm, 1075.5)) {
        std::cerr << "Decimal-comma Floor Reflection input was not accepted\n";
        return 45;
    }

    floorButtons->button(QDialogButtonBox::Apply)->click();
    if (floorApplyCount != 1 || !floorSettings[0].enabled ||
        !near(floorSettings[0].listenerHeightAboveFloorMm, 1075.5) ||
        !near(floorSettings[0].horizontalDistanceMm, 2500.0) ||
        floorSettings[0].surfacePreset != FloorSurfacePreset::MikiReference10mm100k) {
        std::cerr << "Apply did not commit Floor Reflection settings\n";
        return 46;
    }

    listeningDistance->setValue(3100.0);
    floorEnable->setChecked(false);
    QApplication::processEvents();
    if (floorSettings[0].enabled || !near(floorSettings[0].horizontalDistanceMm, 3100.0)) {
        std::cerr << "Post-Apply Floor Reflection changes were not previewed\n";
        return 47;
    }

    floorDialog.reject();
    if (!floorSettings[0].enabled ||
        !near(floorSettings[0].listenerHeightAboveFloorMm, 1075.5) ||
        !near(floorSettings[0].horizontalDistanceMm, 2500.0) ||
        floorSettings[0].surfacePreset != FloorSurfacePreset::MikiReference10mm100k) {
        std::cerr << "Cancel did not restore the last applied Floor Reflection state\n";
        return 48;
    }

    std::cout << "Baffle / Diffraction + Floor Reflection Patch-250 dialog smoke test passed\n";
    return 0;
}
