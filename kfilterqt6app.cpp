/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterqt6app.h"

#include "circuitout.h"
#include "driver.h"
#include "driverparametersdialog.h"
#include "networkparametersdialog.h"
#include "networksectioneditdialog.h"
#include "plotcolorsdialog.h"
#include "kfilterdoc.h"
#include "kfilterprojectio.h"
#include "kfilterview.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QGroupBox>
#include <QSizePolicy>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QFrame>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#ifndef KFILTER_VERSION_STRING
#define KFILTER_VERSION_STRING "unknown"
#endif

#ifndef KFILTER_PATCH_LEVEL_STRING
#define KFILTER_PATCH_LEVEL_STRING "unknown"
#endif

#ifndef KFILTER_LICENSE_STRING
#define KFILTER_LICENSE_STRING "unknown"
#endif

#include <algorithm>
#include <cmath>
#include <memory>


namespace
{
QString ensureProjectSuffix(QString filePath)
{
    if (!filePath.endsWith(QStringLiteral(".kfp"), Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".kfp");
    }
    return filePath;
}

bool nearlyEqual(double lhs, double rhs)
{
    const double scale = std::max(1.0, std::max(std::abs(lhs), std::abs(rhs)));
    return std::abs(lhs - rhs) <= 1e-12 * scale;
}

QColor readColorSetting(const QSettings& settings, const QString& key, const QColor& defaultColor)
{
    const QVariant value = settings.value(key);
    if (!value.isValid()) {
        return defaultColor;
    }

    const QColor color = value.value<QColor>();
    return color.isValid() ? color : defaultColor;
}

void writeColorSetting(QSettings& settings,
                       const QString& key,
                       const QColor& color,
                       const QColor& defaultColor)
{
    if (!color.isValid() || color == defaultColor) {
        settings.remove(key);
        return;
    }

    settings.setValue(key, color);
}

QString plotPressureColorKey(int index)
{
    return QStringLiteral("PlotWindow/pressureColor%1").arg(index);
}

QString plotImpedanceColorKey(int index)
{
    return QStringLiteral("PlotWindow/impedanceColor%1").arg(index);
}

KFilterView::PlotColorSettings readPlotColorSettings(const QSettings& settings)
{
    KFilterView::PlotColorSettings colors = KFilterView::defaultPlotColorSettings();
    const KFilterView::PlotColorSettings defaults = KFilterView::defaultPlotColorSettings();

    colors.background = readColorSetting(settings, QStringLiteral("PlotWindow/backgroundColor"), defaults.background);
    colors.grid = readColorSetting(settings, QStringLiteral("PlotWindow/gridColor"), defaults.grid);
    colors.thresholdGrid = readColorSetting(settings, QStringLiteral("PlotWindow/thresholdGridColor"), defaults.thresholdGrid);
    for (int index = 0; index < static_cast<int>(colors.pressureCurves.size()); ++index) {
        colors.pressureCurves[index] = readColorSetting(settings,
                                                        plotPressureColorKey(index),
                                                        defaults.pressureCurves[index]);
        colors.impedanceCurves[index] = readColorSetting(settings,
                                                         plotImpedanceColorKey(index),
                                                         defaults.impedanceCurves[index]);
    }
    colors.pressureSummary = readColorSetting(settings,
                                              QStringLiteral("PlotWindow/pressureSummaryColor"),
                                              defaults.pressureSummary);
    colors.scalarPressureSummary = readColorSetting(settings,
                                                    QStringLiteral("PlotWindow/scalarPressureSummaryColor"),
                                                    defaults.scalarPressureSummary);
    colors.impedanceSummary = readColorSetting(settings,
                                               QStringLiteral("PlotWindow/impedanceSummaryColor"),
                                               defaults.impedanceSummary);
    return colors;
}

void writePlotColorSettings(QSettings& settings, const KFilterView::PlotColorSettings& colors)
{
    const KFilterView::PlotColorSettings defaults = KFilterView::defaultPlotColorSettings();

    writeColorSetting(settings, QStringLiteral("PlotWindow/backgroundColor"), colors.background, defaults.background);
    writeColorSetting(settings, QStringLiteral("PlotWindow/gridColor"), colors.grid, defaults.grid);
    writeColorSetting(settings, QStringLiteral("PlotWindow/thresholdGridColor"), colors.thresholdGrid, defaults.thresholdGrid);
    for (int index = 0; index < static_cast<int>(colors.pressureCurves.size()); ++index) {
        writeColorSetting(settings, plotPressureColorKey(index), colors.pressureCurves[index], defaults.pressureCurves[index]);
        writeColorSetting(settings, plotImpedanceColorKey(index), colors.impedanceCurves[index], defaults.impedanceCurves[index]);
    }
    writeColorSetting(settings, QStringLiteral("PlotWindow/pressureSummaryColor"),
                      colors.pressureSummary, defaults.pressureSummary);
    writeColorSetting(settings, QStringLiteral("PlotWindow/scalarPressureSummaryColor"),
                      colors.scalarPressureSummary, defaults.scalarPressureSummary);
    writeColorSetting(settings, QStringLiteral("PlotWindow/impedanceSummaryColor"),
                      colors.impedanceSummary, defaults.impedanceSummary);
}

}

KFilterQt6App::KFilterQt6App(QWidget *parent)
    : QMainWindow(parent),
      m_doc(new KFilterDoc(this)),
      m_lastDirectory(QDir::homePath())
{
    setAcceptDrops(true);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    m_stateLabel = new QLabel(central);
    m_stateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_plotView = new KFilterView(m_doc, central);
    m_plotView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *plotBox = new QGroupBox(tr("Frequency Response / Impedance"), central);
    auto *plotLayout = new QVBoxLayout(plotBox);
    plotLayout->setContentsMargins(6, 8, 6, 6);
    plotLayout->addWidget(m_plotView, 1);

    m_circuitPreview = new CircuitOut(central);
    m_circuitPreview->setMinimumSize(900, 330);
    m_circuitPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_circuitPreview, &CircuitOut::networkSectionClicked,
            this, [this](int driverIndex, int sectionIndex, CircuitOut::NetworkHitGroup group) {
        editNetworkSectionFromPreview(driverIndex, sectionIndex, static_cast<int>(group));
    });
    connect(m_circuitPreview, &CircuitOut::networkSectionContextMenuRequested,
            this, [this](int driverIndex, int sectionIndex, CircuitOut::NetworkHitGroup group, const QPoint& globalPosition) {
        showNetworkSectionContextMenuFromPreview(driverIndex, sectionIndex, static_cast<int>(group), globalPosition);
    });
    connect(m_circuitPreview, &CircuitOut::networkSectionHovered,
            this, [this](int driverIndex, int sectionIndex, CircuitOut::NetworkHitGroup group) {
        showNetworkSectionHoverFromPreview(driverIndex, sectionIndex, static_cast<int>(group));
    });
    connect(m_circuitPreview, &CircuitOut::networkSectionHoverLeft,
            this, &KFilterQt6App::clearNetworkSectionHoverFromPreview);
    connect(m_circuitPreview, &CircuitOut::driverClicked,
            this, &KFilterQt6App::editDriverParametersFromPreview);
    connect(m_circuitPreview, &CircuitOut::driverHovered,
            this, &KFilterQt6App::showDriverHoverFromPreview);
    connect(m_circuitPreview, &CircuitOut::driverActivityLampClicked,
            this, &KFilterQt6App::toggleDriverPlotVisibilityFromPreview);

    auto *circuitScrollArea = new QScrollArea(central);
    circuitScrollArea->setWidgetResizable(true);
    circuitScrollArea->setFrameShape(QFrame::NoFrame);
    circuitScrollArea->setWidget(m_circuitPreview);

    auto *circuitBox = new QGroupBox(tr("Network / Filter Schematic"), central);
    auto *circuitLayout = new QVBoxLayout(circuitBox);
    circuitLayout->setContentsMargins(6, 8, 6, 6);
    circuitLayout->addWidget(circuitScrollArea, 1);

    m_mainSplitter = new QSplitter(Qt::Vertical, central);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(8);
    m_mainSplitter->addWidget(plotBox);
    m_mainSplitter->addWidget(circuitBox);
    m_mainSplitter->setStretchFactor(0, 5);
    m_mainSplitter->setStretchFactor(1, 4);

    layout->addWidget(m_stateLabel);
    layout->addWidget(m_mainSplitter, 1);
    setCentralWidget(central);

    enableProjectDropTarget(central);
    enableProjectDropTarget(m_stateLabel);
    enableProjectDropTarget(m_plotView);
    enableProjectDropTarget(plotBox);
    enableProjectDropTarget(circuitScrollArea);
    enableProjectDropTarget(circuitScrollArea->viewport());
    enableProjectDropTarget(m_circuitPreview);
    enableProjectDropTarget(circuitBox);
    enableProjectDropTarget(m_mainSplitter);

    createActions();
    createMenusAndToolBar();

    connect(m_doc, &KFilterDoc::forceviewrefresh, this, &KFilterQt6App::refreshOverview);
    connect(m_doc, &KFilterDoc::forceviewrefresh, m_plotView, [this]() {
        m_plotView->update();
    });
    connect(m_doc, &KFilterDoc::forceviewrefresh, this, &KFilterQt6App::refreshCircuitPreview);

    m_doc->newDocument();
    loadSettings();
    statusBar()->showMessage(tr("Ready."));
    refreshOverview();
}

KFilterQt6App::~KFilterQt6App() = default;

void KFilterQt6App::createActions()
{
    m_newAction = new QAction(tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &KFilterQt6App::newFile);

    m_openAction = new QAction(tr("&Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &KFilterQt6App::openFile);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &KFilterQt6App::saveFile);

    m_saveAsAction = new QAction(tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &KFilterQt6App::saveFileAs);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_driverParametersAction = new QAction(tr("Driver &Parameters..."), this);
    connect(m_driverParametersAction, &QAction::triggered, this, &KFilterQt6App::editDriverParameters);

    m_networkParametersAction = new QAction(tr("&Network / Filter Parameters..."), this);
    connect(m_networkParametersAction, &QAction::triggered, this, &KFilterQt6App::editNetworkParameters);

    m_showFileToolBarAction = new QAction(tr("Show &File Toolbar"), this);
    m_showFileToolBarAction->setCheckable(true);
    m_showFileToolBarAction->setChecked(true);
    connect(m_showFileToolBarAction, &QAction::toggled, this, &KFilterQt6App::setFileToolBarVisible);

    m_showEditToolBarAction = new QAction(tr("Show &Edit Toolbar"), this);
    m_showEditToolBarAction->setCheckable(true);
    m_showEditToolBarAction->setChecked(true);
    connect(m_showEditToolBarAction, &QAction::toggled, this, &KFilterQt6App::setEditToolBarVisible);

    m_showStatusBarAction = new QAction(tr("Show &Status Bar"), this);
    m_showStatusBarAction->setCheckable(true);
    m_showStatusBarAction->setChecked(true);
    connect(m_showStatusBarAction, &QAction::toggled, this, &KFilterQt6App::setStatusBarVisible);

    m_resetLayoutAction = new QAction(tr("Reset &Window Layout"), this);
    connect(m_resetLayoutAction, &QAction::triggered, this, &KFilterQt6App::resetWindowLayout);

    m_configurePlotColorsAction = new QAction(tr("&Grid and Plot Colors..."), this);
    connect(m_configurePlotColorsAction, &QAction::triggered, this, &KFilterQt6App::configurePlotColors);

    m_resetPlotColorsAction = new QAction(tr("&Reset Grid and Plot Colors"), this);
    connect(m_resetPlotColorsAction, &QAction::triggered, this, &KFilterQt6App::resetPlotColors);

    m_circuitPreviewDriverActionGroup = new QActionGroup(this);
    m_circuitPreviewDriverActionGroup->setExclusive(true);

    m_circuitPreviewAllDriversAction = new QAction(tr("&All Drivers"), this);
    m_circuitPreviewAllDriversAction->setCheckable(true);
    m_circuitPreviewAllDriversAction->setChecked(true);
    m_circuitPreviewDriverActionGroup->addAction(m_circuitPreviewAllDriversAction);
    connect(m_circuitPreviewAllDriversAction, &QAction::triggered, this, [this]() {
        setCircuitPreviewDriverIndex(KFilterProjectIo::DriverCount, true);
    });

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        m_circuitPreviewDriverActions[index] = new QAction(circuitPreviewDriverMenuText(index), this);
        m_circuitPreviewDriverActions[index]->setCheckable(true);
        m_circuitPreviewDriverActions[index]->setEnabled(circuitPreviewDriverSlotAvailable(index));
        m_circuitPreviewDriverActionGroup->addAction(m_circuitPreviewDriverActions[index]);
        connect(m_circuitPreviewDriverActions[index], &QAction::triggered, this, [this, index]() {
            setCircuitPreviewDriverIndex(index, true);
        });
    }

    m_circuitPreviewBackgroundColorAction = new QAction(tr("Background &Color..."), this);
    connect(m_circuitPreviewBackgroundColorAction, &QAction::triggered,
            this, &KFilterQt6App::chooseCircuitPreviewBackgroundColor);

    m_resetCircuitPreviewBackgroundColorAction = new QAction(tr("&Reset Background Color"), this);
    connect(m_resetCircuitPreviewBackgroundColorAction, &QAction::triggered,
            this, &KFilterQt6App::resetCircuitPreviewBackgroundColor);

    m_aboutAction = new QAction(tr("&About KFilter6"), this);
    connect(m_aboutAction, &QAction::triggered, this, &KFilterQt6App::showAboutDialog);

    updateCircuitPreviewDriverActions();
}

void KFilterQt6App::createMenusAndToolBar()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_driverParametersAction);
    editMenu->addAction(m_networkParametersAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_showFileToolBarAction);
    viewMenu->addAction(m_showEditToolBarAction);
    viewMenu->addAction(m_showStatusBarAction);
    viewMenu->addSeparator();

    QMenu *plotWindowMenu = viewMenu->addMenu(tr("&Plot Window"));
    plotWindowMenu->addAction(m_configurePlotColorsAction);
    plotWindowMenu->addAction(m_resetPlotColorsAction);

    QMenu *networkPreviewMenu = viewMenu->addMenu(tr("&Network Preview"));
    QMenu *driverViewMenu = networkPreviewMenu->addMenu(tr("&Driver View"));
    driverViewMenu->addAction(m_circuitPreviewAllDriversAction);
    driverViewMenu->addSeparator();
    for (QAction *driverAction : m_circuitPreviewDriverActions) {
        if (driverAction != nullptr) {
            driverViewMenu->addAction(driverAction);
        }
    }
    networkPreviewMenu->addSeparator();
    networkPreviewMenu->addAction(m_circuitPreviewBackgroundColorAction);
    networkPreviewMenu->addAction(m_resetCircuitPreviewBackgroundColorAction);

    viewMenu->addAction(m_resetLayoutAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_aboutAction);

    m_fileToolBar = addToolBar(tr("File"));
    m_fileToolBar->setObjectName(QStringLiteral("fileToolBar"));
    m_fileToolBar->addAction(m_newAction);
    m_fileToolBar->addAction(m_openAction);
    m_fileToolBar->addAction(m_saveAction);
    m_fileToolBar->addAction(m_saveAsAction);

    m_editToolBar = addToolBar(tr("Edit"));
    m_editToolBar->setObjectName(QStringLiteral("editToolBar"));
    m_editToolBar->addAction(m_driverParametersAction);
    m_editToolBar->addAction(m_networkParametersAction);
}


void KFilterQt6App::setFileToolBarVisible(bool visible)
{
    if (m_fileToolBar != nullptr) {
        m_fileToolBar->setVisible(visible);
    }

    if (m_showFileToolBarAction != nullptr && m_showFileToolBarAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_showFileToolBarAction);
        m_showFileToolBarAction->setChecked(visible);
    }

    statusBar()->showMessage(visible ? tr("File toolbar shown.") : tr("File toolbar hidden."), 3000);
}

void KFilterQt6App::setEditToolBarVisible(bool visible)
{
    if (m_editToolBar != nullptr) {
        m_editToolBar->setVisible(visible);
    }

    if (m_showEditToolBarAction != nullptr && m_showEditToolBarAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_showEditToolBarAction);
        m_showEditToolBarAction->setChecked(visible);
    }

    statusBar()->showMessage(visible ? tr("Edit toolbar shown.") : tr("Edit toolbar hidden."), 3000);
}

void KFilterQt6App::setStatusBarVisible(bool visible)
{
    statusBar()->setVisible(visible);

    if (m_showStatusBarAction != nullptr && m_showStatusBarAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_showStatusBarAction);
        m_showStatusBarAction->setChecked(visible);
    }

    if (visible) {
        statusBar()->showMessage(tr("Status bar shown."), 3000);
    }
}

void KFilterQt6App::resetWindowLayout()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (m_mainSplitter == nullptr) {
        return;
    }

    // Keep the plot slightly larger than the schematic, while leaving enough
    // room for the complete 8-section network preview and value table.
    m_mainSplitter->setSizes({520, 360});
}

void KFilterQt6App::configurePlotColors()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (m_plotView == nullptr) {
        return;
    }

    PlotColorsDialog dialog(m_plotView->plotColorSettings(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const KFilterView::PlotColorSettings colors = dialog.settings();
    m_plotView->setPlotColorSettings(colors);

    QSettings settings;
    writePlotColorSettings(settings, colors);

    statusBar()->showMessage(tr("Plot colors changed."), 3000);
}

void KFilterQt6App::resetPlotColors()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (m_plotView == nullptr) {
        return;
    }

    m_plotView->resetPlotColorSettings();

    QSettings settings;
    writePlotColorSettings(settings, m_plotView->plotColorSettings());

    statusBar()->showMessage(tr("Plot colors reset."), 3000);
}

void KFilterQt6App::chooseCircuitPreviewBackgroundColor()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (m_circuitPreview == nullptr) {
        return;
    }

    const QColor selectedColor = QColorDialog::getColor(
        m_circuitPreview->backgroundColor(),
        this,
        tr("Network Preview Background Color"));

    if (!selectedColor.isValid()) {
        return;
    }

    m_circuitPreview->setBackgroundColor(selectedColor);

    QSettings settings;
    if (selectedColor == CircuitOut::defaultBackgroundColor()) {
        settings.remove(QStringLiteral("CircuitPreview/backgroundColor"));
    } else {
        settings.setValue(QStringLiteral("CircuitPreview/backgroundColor"), selectedColor);
    }

    statusBar()->showMessage(tr("Network preview background color changed."), 3000);
}

void KFilterQt6App::resetCircuitPreviewBackgroundColor()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (m_circuitPreview == nullptr) {
        return;
    }

    m_circuitPreview->setBackgroundColor(CircuitOut::defaultBackgroundColor());

    QSettings settings;
    settings.remove(QStringLiteral("CircuitPreview/backgroundColor"));

    statusBar()->showMessage(tr("Network preview background color reset."), 3000);
}

void KFilterQt6App::showNetworkSectionHoverFromPreview(int driverIndex, int sectionIndex, int groupValue)
{
    if (networkSectionEditInProgress()) {
        return;
    }

    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount ||
        sectionIndex < 0 || sectionIndex >= 8) {
        return;
    }

    const int seriesGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::SeriesSection);
    const int shuntGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::ShuntSection);
    if (groupValue != seriesGroupValue && groupValue != shuntGroupValue) {
        return;
    }

    const QString groupName = groupValue == seriesGroupValue ? tr("Series") : tr("Shunt");
    m_lastCircuitPreviewHoverStatus = tr("Click to edit Driver %1, Section %2, %3 R/C/L; right-click for options.")
                                         .arg(driverIndex + 1)
                                         .arg(sectionIndex + 1)
                                         .arg(groupName);
    statusBar()->showMessage(m_lastCircuitPreviewHoverStatus);
}

void KFilterQt6App::showDriverHoverFromPreview(int driverIndex)
{
    if (networkSectionEditInProgress()) {
        return;
    }

    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        return;
    }

    m_lastCircuitPreviewHoverStatus = tr("Click to edit Driver %1 parameters.")
                                         .arg(driverIndex + 1);
    statusBar()->showMessage(m_lastCircuitPreviewHoverStatus);
}

void KFilterQt6App::clearNetworkSectionHoverFromPreview()
{
    if (!m_lastCircuitPreviewHoverStatus.isEmpty() &&
        statusBar()->currentMessage() == m_lastCircuitPreviewHoverStatus) {
        statusBar()->clearMessage();
    }
    m_lastCircuitPreviewHoverStatus.clear();
}

void KFilterQt6App::showNetworkSectionContextMenuFromPreview(int driverIndex, int sectionIndex, int groupValue, const QPoint& globalPosition)
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount ||
        sectionIndex < 0 || sectionIndex >= 8) {
        return;
    }

    const int seriesGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::SeriesSection);
    const int shuntGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::ShuntSection);
    if (groupValue != seriesGroupValue && groupValue != shuntGroupValue) {
        return;
    }

    const QString groupName = groupValue == seriesGroupValue ? tr("Series") : tr("Shunt");

    QMenu menu(this);
    QAction *editAction = menu.addAction(tr("Edit Driver %1, Section %2, %3 R/C/L...")
                                             .arg(driverIndex + 1)
                                             .arg(sectionIndex + 1)
                                             .arg(groupName));
    QAction *clearAction = menu.addAction(tr("Clear Driver %1, Section %2, %3 R/C/L")
                                              .arg(driverIndex + 1)
                                              .arg(sectionIndex + 1)
                                              .arg(groupName));

    QAction *selectedAction = menu.exec(globalPosition);
    if (selectedAction == editAction) {
        editNetworkSectionFromPreview(driverIndex, sectionIndex, groupValue);
        return;
    }

    if (selectedAction == clearAction) {
        clearNetworkSectionFromPreview(driverIndex, sectionIndex, groupValue);
    }
}

void KFilterQt6App::clearNetworkSectionFromPreview(int driverIndex, int sectionIndex, int groupValue)
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount ||
        sectionIndex < 0 || sectionIndex >= 8) {
        return;
    }

    const int seriesGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::SeriesSection);
    const int shuntGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::ShuntSection);
    if (groupValue != seriesGroupValue && groupValue != shuntGroupValue) {
        return;
    }

    const bool clearSeriesGroup = groupValue == seriesGroupValue;
    const int firstRow = clearSeriesGroup ? 0 : 3;
    const QString groupName = clearSeriesGroup ? tr("Series") : tr("Shunt");

    auto unitIndex = [](int row, int section) {
        return section * 6 + row + 1;
    };

    driver& drv = m_doc->m_driverDriver[driverIndex];
    const int resistanceUnit = unitIndex(firstRow, sectionIndex);
    const int capacitanceUnit = unitIndex(firstRow + 1, sectionIndex);
    const int inductanceUnit = unitIndex(firstRow + 2, sectionIndex);

    if (nearlyEqual(drv.getUnit(resistanceUnit), 0.0) &&
        nearlyEqual(drv.getUnit(capacitanceUnit), 0.0) &&
        nearlyEqual(drv.getUnit(inductanceUnit), 0.0)) {
        statusBar()->showMessage(tr("Driver %1, Section %2, %3 R/C/L already clear.")
                                     .arg(driverIndex + 1)
                                     .arg(sectionIndex + 1)
                                     .arg(groupName),
                                 3000);
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Clear Network Section"),
        tr("Clear Driver %1, Section %2, %3 R/C/L?\n\nThis will set R, C and L to 0.")
            .arg(driverIndex + 1)
            .arg(sectionIndex + 1)
            .arg(groupName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        statusBar()->showMessage(tr("Network section clear cancelled."), 3000);
        return;
    }

    drv.setUnit(resistanceUnit, 0.0);
    drv.setUnit(capacitanceUnit, 0.0);
    drv.setUnit(inductanceUnit, 0.0);
    drv.Berechneparameter();

    m_lastNetworkParametersDriverIndex = driverIndex;
    m_doc->setModified(true);
    m_doc->viewrefresh();
    statusBar()->showMessage(tr("Driver %1, Section %2, %3 R/C/L cleared.")
                                 .arg(driverIndex + 1)
                                 .arg(sectionIndex + 1)
                                 .arg(groupName),
                             3000);
}

void KFilterQt6App::editNetworkSectionFromPreview(int driverIndex, int sectionIndex, int groupValue)
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount ||
        sectionIndex < 0 || sectionIndex >= 8) {
        return;
    }

    const int seriesGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::SeriesSection);
    const int shuntGroupValue = static_cast<int>(CircuitOut::NetworkHitGroup::ShuntSection);
    if (groupValue != seriesGroupValue && groupValue != shuntGroupValue) {
        return;
    }

    const bool editSeriesGroup = groupValue == seriesGroupValue;
    const int firstRow = editSeriesGroup ? 0 : 3;
    const QString groupName = editSeriesGroup ? tr("Series") : tr("Shunt");

    auto unitIndex = [](int row, int section) {
        return section * 6 + row + 1;
    };

    auto displayFromInternal = [](int row, double value) {
        if (row == 1 || row == 4) {
            return value * 1000000.0; // F -> uF
        }
        if (row == 2 || row == 5) {
            return value * 1000.0; // H -> mH
        }
        return value;
    };

    auto internalFromDisplay = [](int row, double value) {
        if (row == 1 || row == 4) {
            return value / 1000000.0; // uF -> F
        }
        if (row == 2 || row == 5) {
            return value / 1000.0; // mH -> H
        }
        return value;
    };

    const int resistanceUnit = unitIndex(firstRow, sectionIndex);
    const int capacitanceUnit = unitIndex(firstRow + 1, sectionIndex);
    const int inductanceUnit = unitIndex(firstRow + 2, sectionIndex);

    struct SectionInternalValues
    {
        double resistance = 0.0;
        double capacitance = 0.0;
        double inductance = 0.0;
    };

    driver& drv = m_doc->m_driverDriver[driverIndex];
    auto readInternalValues = [&drv, resistanceUnit, capacitanceUnit, inductanceUnit]() {
        SectionInternalValues values;
        values.resistance = drv.getUnit(resistanceUnit);
        values.capacitance = drv.getUnit(capacitanceUnit);
        values.inductance = drv.getUnit(inductanceUnit);
        return values;
    };
    auto displayValuesFromInternal = [displayFromInternal, firstRow](const SectionInternalValues& values) {
        NetworkSectionEditDialog::Values displayValues;
        displayValues.resistanceOhm = displayFromInternal(firstRow, values.resistance);
        displayValues.capacitanceMicroFarad = displayFromInternal(firstRow + 1, values.capacitance);
        displayValues.inductanceMilliHenry = displayFromInternal(firstRow + 2, values.inductance);
        return displayValues;
    };
    auto internalValuesFromDisplay = [internalFromDisplay, firstRow](const NetworkSectionEditDialog::Values& values) {
        SectionInternalValues internalValues;
        internalValues.resistance = internalFromDisplay(firstRow, values.resistanceOhm);
        internalValues.capacitance = internalFromDisplay(firstRow + 1, values.capacitanceMicroFarad);
        internalValues.inductance = internalFromDisplay(firstRow + 2, values.inductanceMilliHenry);
        return internalValues;
    };
    auto applyInternalValues = [this, driverIndex, resistanceUnit, capacitanceUnit, inductanceUnit](const SectionInternalValues& values) {
        driver& targetDriver = m_doc->m_driverDriver[driverIndex];
        targetDriver.setUnit(resistanceUnit, values.resistance);
        targetDriver.setUnit(capacitanceUnit, values.capacitance);
        targetDriver.setUnit(inductanceUnit, values.inductance);
        targetDriver.Berechneparameter();
    };
    auto sameInternalValues = [](const SectionInternalValues& lhs, const SectionInternalValues& rhs) {
        return nearlyEqual(lhs.resistance, rhs.resistance) &&
               nearlyEqual(lhs.capacitance, rhs.capacitance) &&
               nearlyEqual(lhs.inductance, rhs.inductance);
    };

    struct SectionEditState
    {
        SectionInternalValues originalInternalValues;
        SectionInternalValues previewInternalValues;
    };

    auto state = std::make_shared<SectionEditState>();
    state->originalInternalValues = readInternalValues();
    state->previewInternalValues = state->originalInternalValues;
    const NetworkSectionEditDialog::Values initialValues = displayValuesFromInternal(state->originalInternalValues);

    auto *dialog = new NetworkSectionEditDialog(driverIndex, sectionIndex, groupName, initialValues, this);
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    m_activeNetworkSectionEditDialog = dialog;
    updateActionState();

    connect(dialog, &NetworkSectionEditDialog::previewValuesChanged, this,
            [this, state, internalValuesFromDisplay, applyInternalValues, sameInternalValues](const NetworkSectionEditDialog::Values& previewValues) {
        const SectionInternalValues nextPreviewValues = internalValuesFromDisplay(previewValues);
        if (sameInternalValues(state->previewInternalValues, nextPreviewValues)) {
            return;
        }

        applyInternalValues(nextPreviewValues);
        state->previewInternalValues = nextPreviewValues;
        m_doc->viewrefresh();
    });

    connect(dialog, &QDialog::finished, this,
            [this,
             dialog,
             state,
             internalValuesFromDisplay,
             applyInternalValues,
             sameInternalValues,
             driverIndex,
             sectionIndex,
             groupName](int result) {
        auto clearActiveDialog = [this, dialog]() {
            if (m_activeNetworkSectionEditDialog == dialog) {
                m_activeNetworkSectionEditDialog = nullptr;
                updateActionState();
            }
        };

        if (result != QDialog::Accepted) {
            if (!sameInternalValues(state->previewInternalValues, state->originalInternalValues)) {
                applyInternalValues(state->originalInternalValues);
                m_doc->viewrefresh();
            }
            statusBar()->showMessage(tr("Network section edit cancelled."), 3000);
            clearActiveDialog();
            dialog->deleteLater();
            return;
        }

        const SectionInternalValues newInternalValues = internalValuesFromDisplay(dialog->values());
        const bool previewAlreadyMatchesFinal = sameInternalValues(state->previewInternalValues, newInternalValues);
        if (!previewAlreadyMatchesFinal) {
            applyInternalValues(newInternalValues);
        }

        if (sameInternalValues(state->originalInternalValues, newInternalValues)) {
            if (!previewAlreadyMatchesFinal) {
                m_doc->viewrefresh();
            }
            statusBar()->showMessage(tr("Driver %1, Section %2, %3 R/C/L unchanged.")
                                         .arg(driverIndex + 1)
                                         .arg(sectionIndex + 1)
                                         .arg(groupName),
                                     3000);
            clearActiveDialog();
            dialog->deleteLater();
            return;
        }

        m_lastNetworkParametersDriverIndex = driverIndex;
        m_doc->setModified(true);
        m_doc->viewrefresh();
        statusBar()->showMessage(tr("Driver %1, Section %2, %3 R/C/L updated.")
                                     .arg(driverIndex + 1)
                                     .arg(sectionIndex + 1)
                                     .arg(groupName),
                                 3000);
        clearActiveDialog();
        dialog->deleteLater();
    });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void KFilterQt6App::loadSettings()
{
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("MainWindow/geometry")).toByteArray();
    if (geometry.isEmpty() || !restoreGeometry(geometry)) {
        resize(1180, 860);
    }

    const QByteArray splitterState = settings.value(QStringLiteral("MainWindow/splitterState")).toByteArray();
    if (splitterState.isEmpty() || m_mainSplitter == nullptr || !m_mainSplitter->restoreState(splitterState)) {
        resetWindowLayout();
    }

    const bool showFileToolBar = settings.value(QStringLiteral("MainWindow/showFileToolBar"), true).toBool();
    const bool showEditToolBar = settings.value(QStringLiteral("MainWindow/showEditToolBar"), true).toBool();
    const bool showStatusBar = settings.value(QStringLiteral("MainWindow/showStatusBar"), true).toBool();

    if (m_showFileToolBarAction != nullptr) {
        m_showFileToolBarAction->setChecked(showFileToolBar);
    }
    setFileToolBarVisible(showFileToolBar);

    if (m_showEditToolBarAction != nullptr) {
        m_showEditToolBarAction->setChecked(showEditToolBar);
    }
    setEditToolBarVisible(showEditToolBar);

    if (m_showStatusBarAction != nullptr) {
        m_showStatusBarAction->setChecked(showStatusBar);
    }
    setStatusBarVisible(showStatusBar);

    const QString lastDirectory = settings.value(QStringLiteral("Files/lastDirectory"), QDir::homePath()).toString();
    m_lastDirectory = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;

    const QColor previewBackgroundColor = settings.value(QStringLiteral("CircuitPreview/backgroundColor"),
                                                          CircuitOut::defaultBackgroundColor()).value<QColor>();
    if (m_circuitPreview != nullptr && previewBackgroundColor.isValid()) {
        m_circuitPreview->setBackgroundColor(previewBackgroundColor);
    }

    if (m_plotView != nullptr) {
        m_plotView->setPlotColorSettings(readPlotColorSettings(settings));
    }

    settings.remove(QStringLiteral("CircuitPreview/driverIndex"));
    setCircuitPreviewDriverIndex(KFilterProjectIo::DriverCount, false);
}

void KFilterQt6App::saveSettings() const
{
    QSettings settings;

    settings.setValue(QStringLiteral("MainWindow/geometry"), saveGeometry());
    if (m_mainSplitter != nullptr) {
        settings.setValue(QStringLiteral("MainWindow/splitterState"), m_mainSplitter->saveState());
    }

    settings.setValue(QStringLiteral("MainWindow/showFileToolBar"),
                      m_showFileToolBarAction == nullptr || m_showFileToolBarAction->isChecked());
    settings.setValue(QStringLiteral("MainWindow/showEditToolBar"),
                      m_showEditToolBarAction == nullptr || m_showEditToolBarAction->isChecked());
    settings.setValue(QStringLiteral("MainWindow/showStatusBar"),
                      m_showStatusBarAction == nullptr || m_showStatusBarAction->isChecked());

    settings.setValue(QStringLiteral("Files/lastDirectory"),
                      m_lastDirectory.isEmpty() ? QDir::homePath() : m_lastDirectory);

    settings.remove(QStringLiteral("CircuitPreview/driverIndex"));

    if (m_circuitPreview != nullptr) {
        const QColor previewBackgroundColor = m_circuitPreview->backgroundColor();
        if (previewBackgroundColor == CircuitOut::defaultBackgroundColor()) {
            settings.remove(QStringLiteral("CircuitPreview/backgroundColor"));
        } else {
            settings.setValue(QStringLiteral("CircuitPreview/backgroundColor"), previewBackgroundColor);
        }
    }

    if (m_plotView != nullptr) {
        writePlotColorSettings(settings, m_plotView->plotColorSettings());
    }
}

void KFilterQt6App::closeEvent(QCloseEvent *event)
{
    if (raiseActiveNetworkSectionEditor()) {
        event->ignore();
        return;
    }

    if (maybeSave()) {
        saveSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

bool KFilterQt6App::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    switch (event->type()) {
    case QEvent::DragEnter:
        return acceptProjectDragEvent(static_cast<QDragEnterEvent *>(event));
    case QEvent::DragMove:
        return acceptProjectDragEvent(static_cast<QDragMoveEvent *>(event));
    case QEvent::Drop:
        return openProjectFromDropEvent(static_cast<QDropEvent *>(event));
    default:
        break;
    }

    return QMainWindow::eventFilter(watched, event);
}


void KFilterQt6App::dragEnterEvent(QDragEnterEvent *event)
{
    acceptProjectDragEvent(event);
}

void KFilterQt6App::dragMoveEvent(QDragMoveEvent *event)
{
    acceptProjectDragEvent(event);
}

void KFilterQt6App::dropEvent(QDropEvent *event)
{
    openProjectFromDropEvent(event);
}

void KFilterQt6App::newFile()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (!maybeSave()) {
        return;
    }

    m_doc->newDocument();
    clearDriverPlotVisibilityMemory();
    statusBar()->showMessage(tr("New document created."), 3000);
    refreshOverview();
}

void KFilterQt6App::openFile()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (!maybeSave()) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open KFilter6 Project"),
        dialogStartDirectory(),
        tr("KFilter6 project files (*.kfp);;All files (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    openDocumentFile(QUrl::fromLocalFile(filePath));
}

void KFilterQt6App::clearDriverPlotVisibilityMemory()
{
    for (DriverPlotVisibilityMemory& rememberedVisibility : m_driverPlotVisibilityMemory) {
        rememberedVisibility = DriverPlotVisibilityMemory{};
    }
}

bool KFilterQt6App::openDocumentFile(const QUrl &url)
{
    if (raiseActiveNetworkSectionEditor()) {
        return false;
    }

    if (!url.isLocalFile()) {
        QMessageBox::warning(this, tr("Open KFilter6 Project"),
                             tr("Only local project files are currently supported."));
        return false;
    }

    if (!m_doc->openDocument(url)) {
        QMessageBox::warning(this, tr("Open KFilter6 Project"),
                             tr("The project file could not be opened:\n%1").arg(url.toLocalFile()));
        return false;
    }

    clearDriverPlotVisibilityMemory();
    rememberDirectoryForPath(url.toLocalFile());
    statusBar()->showMessage(tr("Opened %1").arg(url.toLocalFile()), 3000);
    refreshOverview();
    return true;
}

bool KFilterQt6App::saveFile()
{
    if (raiseActiveNetworkSectionEditor()) {
        return false;
    }

    const QString path = currentLocalPath();
    if (path.isEmpty()) {
        return saveFileAs();
    }

    return saveToUrl(QUrl::fromLocalFile(path));
}

bool KFilterQt6App::saveFileAs()
{
    if (raiseActiveNetworkSectionEditor()) {
        return false;
    }

    QString proposedPath = currentLocalPath();
    if (proposedPath.isEmpty()) {
        proposedPath = QDir(dialogStartDirectory()).filePath(QStringLiteral("Untitled.kfp"));
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save KFilter6 Project"),
        proposedPath,
        tr("KFilter6 project files (*.kfp);;All files (*)"));

    if (filePath.isEmpty()) {
        return false;
    }

    filePath = ensureProjectSuffix(filePath);
    return saveToUrl(QUrl::fromLocalFile(filePath));
}

bool KFilterQt6App::saveToUrl(const QUrl &url)
{
    if (!m_doc->saveDocument(url)) {
        QMessageBox::warning(this, tr("Save KFilter6 Project"),
                             tr("The project file could not be saved:\n%1").arg(url.toLocalFile()));
        return false;
    }

    rememberDirectoryForPath(url.toLocalFile());
    statusBar()->showMessage(tr("Saved %1").arg(url.toLocalFile()), 3000);
    refreshOverview();
    return true;
}

void KFilterQt6App::enableProjectDropTarget(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    widget->setAcceptDrops(true);
    widget->installEventFilter(this);
}

bool KFilterQt6App::acceptProjectDragEvent(QDropEvent *event) const
{
    if (event == nullptr) {
        return false;
    }

    QUrl url;
    if (!projectUrlFromDropMimeData(event->mimeData(), url)) {
        event->ignore();
        return false;
    }

    event->acceptProposedAction();
    return true;
}

bool KFilterQt6App::openProjectFromDropEvent(QDropEvent *event)
{
    if (event == nullptr) {
        return false;
    }

    QUrl url;
    if (!projectUrlFromDropMimeData(event->mimeData(), url)) {
        event->ignore();
        return false;
    }

    if (raiseActiveNetworkSectionEditor()) {
        event->ignore();
        return true;
    }

    if (!maybeSave()) {
        event->ignore();
        statusBar()->showMessage(tr("Open cancelled."), 3000);
        return true;
    }

    if (!openDocumentFile(url)) {
        event->ignore();
        return true;
    }

    event->acceptProposedAction();
    return true;
}

bool KFilterQt6App::maybeSave()
{
    if (!m_doc->isModified()) {
        return true;
    }

    const QMessageBox::StandardButton ret = QMessageBox::question(
        this,
        tr("KFilter6"),
        tr("The document '%1' has unsaved changes.\nDo you want to save them?").arg(currentDisplayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (ret == QMessageBox::Save) {
        return saveFile();
    }

    if (ret == QMessageBox::Cancel) {
        return false;
    }

    return true;
}

void KFilterQt6App::rememberDirectoryForPath(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString directory = info.absolutePath();
    if (!directory.isEmpty()) {
        m_lastDirectory = directory;
    }
}

QString KFilterQt6App::dialogStartDirectory() const
{
    const QString path = currentLocalPath();
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        if (!info.absolutePath().isEmpty()) {
            return info.absolutePath();
        }
    }

    return m_lastDirectory.isEmpty() ? QDir::homePath() : m_lastDirectory;
}

QString KFilterQt6App::currentDisplayName() const
{
    const QUrl url = m_doc->URL();
    if (url.isLocalFile()) {
        return QFileInfo(url.toLocalFile()).fileName();
    }

    const QString value = url.toString();
    return value.isEmpty() ? tr("Untitled") : value;
}

QString KFilterQt6App::currentLocalPath() const
{
    const QUrl url = m_doc->URL();
    if (!url.isLocalFile()) {
        return QString();
    }
    return url.toLocalFile();
}

void KFilterQt6App::refreshOverview()
{
    m_stateLabel->setText(tr("Document: %1%2")
                              .arg(currentDisplayName(), m_doc->isModified() ? tr(" [modified]") : QString()));
    refreshCircuitPreview();
    updateWindowTitle();
    updateActionState();
}

void KFilterQt6App::refreshCircuitPreview()
{
    if (m_circuitPreview == nullptr) {
        return;
    }

    if (m_circuitPreviewDriverIndex == KFilterProjectIo::DriverCount) {
        m_circuitPreview->setDrivers(m_doc->m_driverDriver, KFilterProjectIo::DriverCount);
        return;
    }

    int driverIndex = m_circuitPreviewDriverIndex;
    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        driverIndex = KFilterProjectIo::DriverCount;
        m_circuitPreviewDriverIndex = KFilterProjectIo::DriverCount;
    }

    if (driverIndex == KFilterProjectIo::DriverCount) {
        m_circuitPreview->setDrivers(m_doc->m_driverDriver, KFilterProjectIo::DriverCount);
        return;
    }

    m_circuitPreview->setDriver(m_doc->m_driverDriver[driverIndex], driverIndex + 1);
}

void KFilterQt6App::setCircuitPreviewDriverIndex(int driverIndex, bool showStatusMessage)
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (driverIndex < 0 || driverIndex > KFilterProjectIo::DriverCount ||
        (driverIndex < KFilterProjectIo::DriverCount && !circuitPreviewDriverSlotAvailable(driverIndex))) {
        driverIndex = KFilterProjectIo::DriverCount;
    }

    m_circuitPreviewDriverIndex = driverIndex;
    updateCircuitPreviewDriverActions();
    refreshCircuitPreview();

    if (!showStatusMessage) {
        return;
    }

    if (driverIndex == KFilterProjectIo::DriverCount) {
        statusBar()->showMessage(tr("Network preview shows all drivers."), 3000);
    } else {
        statusBar()->showMessage(tr("Network preview shows Driver %1.").arg(driverIndex + 1), 3000);
    }
}

void KFilterQt6App::updateCircuitPreviewDriverActions()
{
    const bool locked = networkSectionEditInProgress();

    if (m_circuitPreviewAllDriversAction != nullptr) {
        m_circuitPreviewAllDriversAction->setEnabled(!locked);
        m_circuitPreviewAllDriversAction->setChecked(m_circuitPreviewDriverIndex == KFilterProjectIo::DriverCount);
    }

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        QAction *driverAction = m_circuitPreviewDriverActions[index];
        if (driverAction == nullptr) {
            continue;
        }

        const bool available = circuitPreviewDriverSlotAvailable(index);
        driverAction->setText(circuitPreviewDriverMenuText(index));
        driverAction->setEnabled(!locked && available);
        driverAction->setChecked(available && m_circuitPreviewDriverIndex == index);
    }
}

bool KFilterQt6App::circuitPreviewDriverSlotAvailable(int driverIndex) const
{
    return m_doc != nullptr && driverIndex >= 0 && driverIndex < KFilterProjectIo::DriverCount;
}

QString KFilterQt6App::circuitPreviewDriverMenuText(int driverIndex) const
{
    const QString fallback = tr("Driver %1").arg(driverIndex + 1);
    if (m_doc == nullptr || driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        return fallback;
    }

    QString title = m_doc->m_driverDriver[driverIndex].GetTitle().trimmed();
    if (title.isEmpty() || title == QStringLiteral("This is a default driver")) {
        return fallback;
    }

    constexpr qsizetype MaximumMenuTitleLength = 60;
    if (title.size() > MaximumMenuTitleLength) {
        title = title.left(MaximumMenuTitleLength - 1) + QStringLiteral("…");
    }

    return tr("Driver %1: %2").arg(driverIndex + 1).arg(title);
}

void KFilterQt6App::updateWindowTitle()
{
    const QString path = currentLocalPath();
    setWindowFilePath(path.isEmpty() ? currentDisplayName() : path);
    setWindowModified(m_doc->isModified());
    setWindowTitle(QStringLiteral("%1[*] - KFilter6").arg(currentDisplayName()));
}

void KFilterQt6App::updateActionState()
{
    const bool locked = networkSectionEditInProgress();

    if (m_newAction != nullptr) {
        m_newAction->setEnabled(!locked);
    }
    if (m_openAction != nullptr) {
        m_openAction->setEnabled(!locked);
    }
    // Keep Save available normally: for untitled documents it behaves like Save As.
    if (m_saveAction != nullptr) {
        m_saveAction->setEnabled(!locked);
    }
    if (m_saveAsAction != nullptr) {
        m_saveAsAction->setEnabled(!locked);
    }
    if (m_driverParametersAction != nullptr) {
        m_driverParametersAction->setEnabled(!locked);
    }
    if (m_networkParametersAction != nullptr) {
        m_networkParametersAction->setEnabled(!locked);
    }
    if (m_quitAction != nullptr) {
        m_quitAction->setEnabled(!locked);
    }
    if (m_showFileToolBarAction != nullptr) {
        m_showFileToolBarAction->setEnabled(!locked);
    }
    if (m_showEditToolBarAction != nullptr) {
        m_showEditToolBarAction->setEnabled(!locked);
    }
    if (m_showStatusBarAction != nullptr) {
        m_showStatusBarAction->setEnabled(!locked);
    }
    if (m_resetLayoutAction != nullptr) {
        m_resetLayoutAction->setEnabled(!locked);
    }
    if (m_configurePlotColorsAction != nullptr) {
        m_configurePlotColorsAction->setEnabled(!locked);
    }
    if (m_resetPlotColorsAction != nullptr) {
        m_resetPlotColorsAction->setEnabled(!locked);
    }
    if (m_circuitPreviewBackgroundColorAction != nullptr) {
        m_circuitPreviewBackgroundColorAction->setEnabled(!locked);
    }
    if (m_resetCircuitPreviewBackgroundColorAction != nullptr) {
        m_resetCircuitPreviewBackgroundColorAction->setEnabled(!locked);
    }

    updateCircuitPreviewDriverActions();
}

bool KFilterQt6App::networkSectionEditInProgress() const
{
    return m_activeNetworkSectionEditDialog != nullptr;
}

bool KFilterQt6App::raiseActiveNetworkSectionEditor()
{
    if (m_activeNetworkSectionEditDialog == nullptr) {
        return false;
    }

    m_activeNetworkSectionEditDialog->show();
    m_activeNetworkSectionEditDialog->raise();
    m_activeNetworkSectionEditDialog->activateWindow();
    statusBar()->showMessage(tr("Finish or cancel the open network section editor first."), 3000);
    return true;
}


bool KFilterQt6App::projectUrlFromDropMimeData(const QMimeData *mimeData, QUrl &url) const
{
    if (mimeData == nullptr || !mimeData->hasUrls()) {
        return false;
    }

    const QList<QUrl> urls = mimeData->urls();
    if (urls.size() != 1) {
        return false;
    }

    const QUrl candidateUrl = urls.constFirst();
    if (!candidateUrl.isLocalFile()) {
        return false;
    }

    const QString filePath = candidateUrl.toLocalFile();
    if (!filePath.endsWith(QStringLiteral(".kfp"), Qt::CaseInsensitive)) {
        return false;
    }

    url = candidateUrl;
    return true;
}

void KFilterQt6App::showAboutDialog()
{
    QMessageBox::about(
        this,
        tr("About KFilter6"),
        tr("<b>KFilter6</b><br>"
           "Version %1<br>"
           "Patch %2<br>"
           "License %3<br>"
           "Copyright &copy; 2002-2026 Martin Erdtmann<br><br>"
           "KFilter6 is a Qt6-based loudspeaker design and crossover modelling tool.<br><br>"
           "It visualizes driver response, impedance, enclosure behaviour, "
           "crossover networks, vector SPL summation, energetic SPL summation, "
           "and total impedance while preserving the legacy KFilter project model.")
            .arg(QStringLiteral(KFILTER_VERSION_STRING))
            .arg(QStringLiteral(KFILTER_PATCH_LEVEL_STRING))
            .arg(QStringLiteral(KFILTER_LICENSE_STRING)));
}

void KFilterQt6App::editDriverParameters()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    openDriverParametersDialog(m_lastDriverParametersDriverIndex);
}

void KFilterQt6App::editDriverParametersFromPreview(int driverIndex)
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        return;
    }

    openDriverParametersDialog(driverIndex);
}

void KFilterQt6App::toggleDriverPlotVisibilityFromPreview(int driverIndex)
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    if (m_doc == nullptr || driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        return;
    }

    driver& selectedDriver = m_doc->m_driverDriver[driverIndex];
    const bool hasActivePlotFlag = selectedDriver.PressureisActive ||
                                   selectedDriver.ImpedanzisActive ||
                                   selectedDriver.SummaryisActive ||
                                   selectedDriver.ScalarSummaryisActive ||
                                   selectedDriver.ImpedanzSummaryisActive;

    DriverPlotVisibilityMemory& rememberedVisibility = m_driverPlotVisibilityMemory[driverIndex];

    if (hasActivePlotFlag) {
        rememberedVisibility.valid = true;
        rememberedVisibility.pressure = selectedDriver.PressureisActive;
        rememberedVisibility.impedance = selectedDriver.ImpedanzisActive;
        rememberedVisibility.vectorSum = selectedDriver.SummaryisActive;
        rememberedVisibility.scalarSum = selectedDriver.ScalarSummaryisActive;
        rememberedVisibility.impedanceSum = selectedDriver.ImpedanzSummaryisActive;

        selectedDriver.PressureisActive = false;
        selectedDriver.ImpedanzisActive = false;
        selectedDriver.SummaryisActive = false;
        selectedDriver.ScalarSummaryisActive = false;
        selectedDriver.ImpedanzSummaryisActive = false;

        statusBar()->showMessage(tr("All plot flags disabled for Driver %1.").arg(driverIndex + 1), 3000);
    } else if (rememberedVisibility.valid) {
        selectedDriver.PressureisActive = rememberedVisibility.pressure;
        selectedDriver.ImpedanzisActive = rememberedVisibility.impedance;
        selectedDriver.SummaryisActive = rememberedVisibility.vectorSum;
        selectedDriver.ScalarSummaryisActive = rememberedVisibility.scalarSum;
        selectedDriver.ImpedanzSummaryisActive = rememberedVisibility.impedanceSum;
        rememberedVisibility = DriverPlotVisibilityMemory{};

        statusBar()->showMessage(tr("Previous plot flags restored for Driver %1.").arg(driverIndex + 1), 3000);
    } else {
        selectedDriver.PressureisActive = true;
        selectedDriver.ImpedanzisActive = false;
        selectedDriver.SummaryisActive = false;
        selectedDriver.ScalarSummaryisActive = false;
        selectedDriver.ImpedanzSummaryisActive = false;

        statusBar()->showMessage(tr("SPL curve enabled for Driver %1.").arg(driverIndex + 1), 3000);
    }

    m_doc->setModified(true);
    m_doc->viewrefresh();
}

void KFilterQt6App::openDriverParametersDialog(int initialDriverIndex)
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    const int safeInitialDriverIndex =
        std::clamp(initialDriverIndex, 0, KFilterProjectIo::DriverCount - 1);

    DriverParametersDialog dialog(m_doc->m_driverDriver, this, safeInitialDriverIndex);
    connect(&dialog, &DriverParametersDialog::parametersApplied, this, [this]() {
        m_doc->setModified(true);
        m_doc->viewrefresh();
        statusBar()->showMessage(tr("Driver parameters applied."), 3000);
    });

    dialog.exec();
    m_lastDriverParametersDriverIndex = dialog.currentDriverIndex();
}

void KFilterQt6App::editNetworkParameters()
{
    if (raiseActiveNetworkSectionEditor()) {
        return;
    }

    NetworkParametersDialog dialog(m_doc->m_driverDriver, this, m_lastNetworkParametersDriverIndex);
    connect(&dialog, &NetworkParametersDialog::parametersApplied, this, [this]() {
        m_doc->setModified(true);
        m_doc->viewrefresh();
        statusBar()->showMessage(tr("Network / filter parameters applied."), 3000);
    });

    dialog.exec();
    m_lastNetworkParametersDriverIndex = dialog.currentDriverIndex();
}
