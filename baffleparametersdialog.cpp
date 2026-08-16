/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleparametersdialog.h"

#include "bafflegeometrypreview.h"
#include "baffleresponse.h"
#include "networkvalueutils.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QValidator>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace
{
void setComboItemEnabled(QComboBox *combo, int index, bool enabled)
{
    if (combo == nullptr || index < 0) {
        return;
    }
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    if (model == nullptr) {
        return;
    }
    QStandardItem *item = model->item(index);
    if (item != nullptr) {
        item->setEnabled(enabled);
    }
}

QString stripSpinBoxAffixes(const QDoubleSpinBox *spinBox, const QString& text)
{
    QString stripped = text.trimmed();

    const QString prefix = spinBox->prefix();
    if (!prefix.isEmpty() && stripped.startsWith(prefix)) {
        stripped.remove(0, prefix.size());
        stripped = stripped.trimmed();
    }

    const QString suffix = spinBox->suffix();
    if (!suffix.isEmpty() && stripped.endsWith(suffix)) {
        stripped.chop(suffix.size());
        stripped = stripped.trimmed();
    } else {
        const QString trimmedSuffix = suffix.trimmed();
        if (!trimmedSuffix.isEmpty() && stripped.endsWith(trimmedSuffix)) {
            stripped.chop(trimmedSuffix.size());
            stripped = stripped.trimmed();
        }
    }

    return stripped;
}

bool parseSpinBoxText(const QDoubleSpinBox *spinBox,
                      const QString& text,
                      double& value,
                      double minimum,
                      double maximum)
{
    return NetworkValueUtils::parseDisplayValue(stripSpinBoxAffixes(spinBox, text),
                                                value,
                                                minimum,
                                                maximum);
}

bool isIncompleteNumber(const QString& text, double minimum)
{
    if (text.isEmpty()) {
        return true;
    }

    if (text == QLatin1String("+") || text == QLatin1String(".") || text == QLatin1String(",")) {
        return true;
    }

    if (text == QLatin1String("-")) {
        return minimum < 0.0;
    }

    if (text == QLatin1String("+.") || text == QLatin1String("+,")) {
        return true;
    }

    if (text == QLatin1String("-.") || text == QLatin1String("-,")) {
        return minimum < 0.0;
    }

    if (text.endsWith(QLatin1Char('.')) || text.endsWith(QLatin1Char(','))) {
        double parsedPrefix = 0.0;
        return NetworkValueUtils::parseDisplayValue(text.left(text.size() - 1),
                                                    parsedPrefix,
                                                    -std::numeric_limits<double>::max(),
                                                    std::numeric_limits<double>::max());
    }

    return false;
}

class BaffleValueSpinBox : public QDoubleSpinBox
{
public:
    using FocusCallback = std::function<void(bool)>;

    explicit BaffleValueSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setCorrectionMode(QAbstractSpinBox::CorrectToPreviousValue);
    }

    void setFocusCallback(FocusCallback callback)
    {
        m_focusCallback = std::move(callback);
    }

protected:
    void focusInEvent(QFocusEvent *event) override
    {
        QDoubleSpinBox::focusInEvent(event);
        if (m_focusCallback) {
            m_focusCallback(true);
        }
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        QDoubleSpinBox::focusOutEvent(event);
        if (m_focusCallback) {
            m_focusCallback(false);
        }
    }

    double valueFromText(const QString& text) const override
    {
        double parsed = 0.0;
        if (parseSpinBoxText(this, text, parsed, minimum(), maximum())) {
            return parsed;
        }
        return value();
    }

    QValidator::State validate(QString& input, int& pos) const override
    {
        Q_UNUSED(pos);

        const QString stripped = stripSpinBoxAffixes(this, input);
        if (isIncompleteNumber(stripped, minimum())) {
            return QValidator::Intermediate;
        }

        double parsed = 0.0;
        if (parseSpinBoxText(this, input, parsed, minimum(), maximum())) {
            return QValidator::Acceptable;
        }

        if (parseSpinBoxText(this,
                             input,
                             parsed,
                             -std::numeric_limits<double>::max(),
                             std::numeric_limits<double>::max())) {
            return QValidator::Intermediate;
        }

        return QValidator::Invalid;
    }

private:
    FocusCallback m_focusCallback;
};

BaffleValueSpinBox *createGeometrySpinBox(QWidget *parent, const QString& objectName)
{
    auto *spin = new BaffleValueSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(0.0, 10000.0);
    spin->setDecimals(1);
    spin->setSingleStep(1.0);
    spin->setSuffix(QObject::tr(" mm"));
    spin->setKeyboardTracking(false);
    return spin;
}
}

BaffleParametersDialog::BaffleParametersDialog(
    BaffleSettingsPerDriver& settings,
    FloorReflectionSettingsPerDriver& floorReflectionSettings,
    QWidget *parent,
    int initialDriverIndex)
    : QDialog(parent),
      m_settings(settings),
      m_floorReflectionSettings(floorReflectionSettings),
      m_committedSettings(settings),
      m_workingSettings(settings),
      m_committedFloorReflectionSettings(floorReflectionSettings),
      m_workingFloorReflectionSettings(floorReflectionSettings)
{
    setWindowTitle(tr("Baffle / Diffraction / Floor Reflection"));
    resize(980, 760);

    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("baffleDriverTabs"));
    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        m_tabs->addTab(createDriverPage(index), tr("Driver %1").arg(index + 1));
    }

    loadFromWorkingModel();
    m_tabs->setCurrentIndex(std::clamp(initialDriverIndex, 0, KFilterProjectIo::DriverCount - 1));
    mainLayout->addWidget(m_tabs, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Apply |
                                           QDialogButtonBox::Cancel,
                                           this);
    buttonBox->setObjectName(QStringLiteral("baffleDialogButtons"));
    buttonBox->button(QDialogButtonBox::Apply)->setToolTip(
        tr("Commit the current live-preview Baffle / Diffraction settings to the project."));
    buttonBox->button(QDialogButtonBox::Ok)->setToolTip(
        tr("Keep the current live-preview settings and close the dialog."));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &BaffleParametersDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &BaffleParametersDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &BaffleParametersDialog::applyClicked);
    mainLayout->addWidget(buttonBox);
}

int BaffleParametersDialog::currentDriverIndex() const
{
    return m_tabs ? m_tabs->currentIndex() : 0;
}

void BaffleParametersDialog::accept()
{
    applyToModel();
    QDialog::accept();
}

void BaffleParametersDialog::applyClicked()
{
    applyToModel();
}

void BaffleParametersDialog::reject()
{
    m_settings = m_committedSettings;
    m_floorReflectionSettings = m_committedFloorReflectionSettings;
    emit parametersPreviewChanged();
    QDialog::reject();
}

QWidget *BaffleParametersDialog::createDriverPage(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    page.page = new QWidget(this);
    auto *layout = new QVBoxLayout(page.page);

    page.enabled = new QCheckBox(tr("Enable baffle / diffraction for this driver"), page.page);
    page.enabled->setObjectName(QStringLiteral("baffleEnableDriver%1").arg(driverIndex + 1));
    page.enabled->setToolTip(tr("Master bypass for the Baffle / Diffraction processing stage of this driver."));
    layout->addWidget(page.enabled);

    auto *parametersGroup = new QGroupBox(tr("Baffle / Diffraction Model"), page.page);
    auto *parametersLayout = new QHBoxLayout(parametersGroup);

    auto *formContainer = new QWidget(parametersGroup);
    auto *form = new QFormLayout(formContainer);
    parametersLayout->addWidget(formContainer, 0);

    page.model = new QComboBox(formContainer);
    page.model->setObjectName(QStringLiteral("baffleModelCombo%1").arg(driverIndex + 1));
    page.model->addItem(tr("Simple Baffle Step"), static_cast<int>(BaffleModel::SimpleBaffleStep));
    page.model->addItem(tr("Rectangular Edge Diffraction"),
                        static_cast<int>(BaffleModel::RectangularEdgeDiffraction));
    page.model->setToolTip(
        tr("Simple Baffle Step is a smooth width-only shelf. Rectangular Edge Diffraction "
           "uses the physical baffle geometry, with Sharp or optional left/right 45-degree side chamfers."));
    form->addRow(tr("Model:"), page.model);

    page.boundaryCondition = new QComboBox(formContainer);
    page.boundaryCondition->setObjectName(
        QStringLiteral("baffleBoundaryConditionCombo%1").arg(driverIndex + 1));
    page.boundaryCondition->addItem(
        tr("Free field"), static_cast<int>(BaffleBoundaryCondition::FreeField));
    page.boundaryCondition->addItem(
        tr("Rigid floor contact (diffraction only)"),
        static_cast<int>(BaffleBoundaryCondition::RigidFloorContactDiffractionOnly));
    page.boundaryCondition->setToolTip(
        tr("Free field treats every cabinet edge as exposed. Rigid floor contact unfolds "
           "the lower edge onto an ideal infinite rigid plane and changes diffraction shape "
           "only; it does not model listening-position floor bounce or boundary gain."));
    form->addRow(tr("Boundary condition:"), page.boundaryCondition);

    page.width = createGeometrySpinBox(formContainer,
                                       QStringLiteral("baffleWidthSpin%1").arg(driverIndex + 1));
    page.width->setRange(1.0, 10000.0);
    page.width->setToolTip(
        tr("Baffle width. Simple Baffle Step uses f0 = 115 / W[m]; Rectangular Edge Diffraction uses the physical geometry."));
    form->addRow(tr("Baffle width:"), page.width);

    page.height = createGeometrySpinBox(formContainer,
                                        QStringLiteral("baffleHeightSpin%1").arg(driverIndex + 1));
    page.height->setToolTip(tr("Rectangular baffle height."));
    form->addRow(tr("Baffle height:"), page.height);

    page.driverX = createGeometrySpinBox(formContainer,
                                         QStringLiteral("baffleDriverXSpin%1").arg(driverIndex + 1));
    page.driverX->setToolTip(tr("Driver centre measured from the original outer left baffle edge (cabinet silhouette), not from the chamfer start."));
    form->addRow(tr("Driver X from left:"), page.driverX);

    page.driverY = createGeometrySpinBox(formContainer,
                                         QStringLiteral("baffleDriverYSpin%1").arg(driverIndex + 1));
    page.driverY->setToolTip(tr("Driver centre measured from the top baffle edge."));
    form->addRow(tr("Driver Y from top:"), page.driverY);

    page.leftEdgeTreatment = new QComboBox(formContainer);
    page.leftEdgeTreatment->setObjectName(
        QStringLiteral("baffleLeftEdgeTreatmentCombo%1").arg(driverIndex + 1));
    page.leftEdgeTreatment->addItem(tr("Sharp"),
                                    static_cast<int>(BaffleSideEdgeTreatment::Sharp));
    page.leftEdgeTreatment->addItem(tr("Chamfer 45°"),
                                    static_cast<int>(BaffleSideEdgeTreatment::Chamfer45));
    page.leftEdgeTreatment->setToolTip(
        tr("Treatment of the left vertical cabinet edge. Chamfer width is the setback measured on the front-baffle plane."));
    form->addRow(tr("Left edge:"), page.leftEdgeTreatment);

    page.leftChamferSetback = createGeometrySpinBox(
        formContainer, QStringLiteral("baffleLeftChamferSpin%1").arg(driverIndex + 1));
    page.leftChamferSetback->setRange(5.0, 10000.0);
    page.leftChamferSetback->setToolTip(
        tr("Left 45-degree chamfer setback on the front-baffle plane. Version 1 supports 5 mm or larger."));
    form->addRow(tr("Left chamfer width:"), page.leftChamferSetback);

    page.rightEdgeTreatment = new QComboBox(formContainer);
    page.rightEdgeTreatment->setObjectName(
        QStringLiteral("baffleRightEdgeTreatmentCombo%1").arg(driverIndex + 1));
    page.rightEdgeTreatment->addItem(tr("Sharp"),
                                     static_cast<int>(BaffleSideEdgeTreatment::Sharp));
    page.rightEdgeTreatment->addItem(tr("Chamfer 45°"),
                                     static_cast<int>(BaffleSideEdgeTreatment::Chamfer45));
    page.rightEdgeTreatment->setToolTip(
        tr("Treatment of the right vertical cabinet edge. Chamfer width is the setback measured on the front-baffle plane."));
    form->addRow(tr("Right edge:"), page.rightEdgeTreatment);

    page.rightChamferSetback = createGeometrySpinBox(
        formContainer, QStringLiteral("baffleRightChamferSpin%1").arg(driverIndex + 1));
    page.rightChamferSetback->setRange(5.0, 10000.0);
    page.rightChamferSetback->setToolTip(
        tr("Right 45-degree chamfer setback on the front-baffle plane. Version 1 supports 5 mm or larger."));
    form->addRow(tr("Right chamfer width:"), page.rightChamferSetback);

    page.midpoint = new QLabel(formContainer);
    page.midpoint->setObjectName(QStringLiteral("baffleMidpointLabel%1").arg(driverIndex + 1));
    form->addRow(tr("Calculated midpoint:"), page.midpoint);

    auto *previewGroup = new QGroupBox(tr("Geometry preview"), parametersGroup);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setContentsMargins(4, 0, 4, 4);
    previewLayout->setSpacing(0);
    page.geometryPreview = new BaffleGeometryPreview(previewGroup);
    page.geometryPreview->setObjectName(
        QStringLiteral("baffleGeometryPreview%1").arg(driverIndex + 1));
    page.geometryPreview->setDriverPositionCallbacks(
        [this, driverIndex](double xMm, double yMm) {
            DriverPage& dragPage = m_pages.at(static_cast<std::size_t>(driverIndex));
            const QSignalBlocker xBlocker(dragPage.driverX);
            const QSignalBlocker yBlocker(dragPage.driverY);
            dragPage.driverX->setValue(xMm);
            dragPage.driverY->setValue(yMm);
        },
        [this, driverIndex](double xMm, double yMm) {
            if (m_loading) {
                return;
            }

            DriverPage& dragPage = m_pages.at(static_cast<std::size_t>(driverIndex));
            {
                const QSignalBlocker xBlocker(dragPage.driverX);
                const QSignalBlocker yBlocker(dragPage.driverY);
                dragPage.driverX->setValue(xMm);
                dragPage.driverY->setValue(yMm);
            }

            // Mouse movement itself is UI-only. Commit the final X/Y pair to
            // the existing live-preview model exactly once on button release.
            writePageToWorkingModel(driverIndex);
            updatePageState(driverIndex);
            previewWorkingModel();
        });
    previewLayout->addWidget(page.geometryPreview, 1);
    parametersLayout->addWidget(previewGroup, 1);

    auto bindPreviewFocus = [preview = page.geometryPreview](BaffleValueSpinBox *spin,
                                                            BaffleGeometryPreview::Highlight highlight) {
        spin->setFocusCallback([preview, highlight](bool focused) {
            if (focused) {
                preview->setHighlight(highlight);
            } else if (preview->currentHighlight() == highlight) {
                preview->setHighlight(BaffleGeometryPreview::Highlight::None);
            }
        });
    };
    bindPreviewFocus(static_cast<BaffleValueSpinBox *>(page.width),
                     BaffleGeometryPreview::Highlight::BaffleWidth);
    bindPreviewFocus(static_cast<BaffleValueSpinBox *>(page.height),
                     BaffleGeometryPreview::Highlight::BaffleHeight);
    bindPreviewFocus(static_cast<BaffleValueSpinBox *>(page.driverX),
                     BaffleGeometryPreview::Highlight::DriverX);
    bindPreviewFocus(static_cast<BaffleValueSpinBox *>(page.driverY),
                     BaffleGeometryPreview::Highlight::DriverY);
    bindPreviewFocus(static_cast<BaffleValueSpinBox *>(page.leftChamferSetback),
                     BaffleGeometryPreview::Highlight::LeftChamfer);
    bindPreviewFocus(static_cast<BaffleValueSpinBox *>(page.rightChamferSetback),
                     BaffleGeometryPreview::Highlight::RightChamfer);

    layout->addWidget(parametersGroup);


    page.showResponse = new QCheckBox(tr("Show baffle transfer function in plot"), page.page);
    page.showResponse->setObjectName(QStringLiteral("baffleShowResponseDriver%1").arg(driverIndex + 1));
    page.showResponse->setToolTip(
        tr("Shows 20 log10(|H_baffle|) as a diagnostic curve. This controls visibility only; it does not enable or disable processing."));
    layout->addWidget(page.showResponse);

    page.responseStatus = new QLabel(page.page);
    page.responseStatus->setObjectName(QStringLiteral("baffleResponseStatus%1").arg(driverIndex + 1));
    page.responseStatus->setWordWrap(true);
    layout->addWidget(page.responseStatus);

    auto *floorGroup = new QGroupBox(tr("Floor Reflection (Experimental)"), page.page);
    floorGroup->setObjectName(QStringLiteral("floorReflectionGroup%1").arg(driverIndex + 1));
    auto *floorLayout = new QGridLayout(floorGroup);
    floorLayout->setObjectName(QStringLiteral("floorReflectionLayout%1").arg(driverIndex + 1));
    floorLayout->setColumnStretch(1, 1);
    floorLayout->setColumnStretch(3, 1);

    page.floorReflectionEnabled = new QCheckBox(
        tr("Enable floor reflection for this driver"), floorGroup);
    page.floorReflectionEnabled->setObjectName(
        QStringLiteral("floorReflectionEnableDriver%1").arg(driverIndex + 1));
    page.floorReflectionEnabled->setToolTip(
        tr("Enables the receiver-dependent first specular floor reflection. This is independent "
           "of the Baffle / Diffraction processing stage and its rigid-floor-contact boundary option."));
    floorLayout->addWidget(page.floorReflectionEnabled, 0, 0, 1, 4);

    page.cabinetBottomAboveFloor = createGeometrySpinBox(
        floorGroup, QStringLiteral("floorReflectionCabinetBottomSpin%1").arg(driverIndex + 1));
    page.cabinetBottomAboveFloor->setRange(0.0, 10000.0);
    page.cabinetBottomAboveFloor->setToolTip(
        tr("Vertical distance between the cabinet bottom and the floor. Source height is derived from "
           "this value plus Baffle height minus Driver Y from top."));
    floorLayout->addWidget(new QLabel(tr("Cabinet bottom above floor:"), floorGroup), 1, 0);
    floorLayout->addWidget(page.cabinetBottomAboveFloor, 1, 1);

    page.listenerHeightAboveFloor = createGeometrySpinBox(
        floorGroup, QStringLiteral("floorReflectionListenerHeightSpin%1").arg(driverIndex + 1));
    page.listenerHeightAboveFloor->setRange(0.0, 10000.0);
    page.listenerHeightAboveFloor->setToolTip(
        tr("Height of the listening position above the floor."));
    floorLayout->addWidget(new QLabel(tr("Listener height above floor:"), floorGroup), 2, 0);
    floorLayout->addWidget(page.listenerHeightAboveFloor, 2, 1);

    page.listeningDistance = createGeometrySpinBox(
        floorGroup, QStringLiteral("floorReflectionDistanceSpin%1").arg(driverIndex + 1));
    page.listeningDistance->setRange(0.0, 100000.0);
    page.listeningDistance->setToolTip(
        tr("Horizontal distance between the loudspeaker source plane and the listening position."));
    floorLayout->addWidget(new QLabel(tr("Listening distance:"), floorGroup), 1, 2);
    floorLayout->addWidget(page.listeningDistance, 1, 3);

    page.floorSurface = new QComboBox(floorGroup);
    page.floorSurface->setObjectName(
        QStringLiteral("floorReflectionSurfaceCombo%1").arg(driverIndex + 1));
    page.floorSurface->addItem(tr("Hard / rigid floor"),
                               static_cast<int>(FloorSurfacePreset::HardRigid));
    page.floorSurface->addItem(tr("Porous floor - Miki reference"),
                               static_cast<int>(FloorSurfacePreset::MikiReference10mm100k));
    page.floorSurface->setToolTip(
        tr("Hard / rigid uses Gamma = +1. The Miki reference is an experimental 10 mm porous layer "
           "with flow resistivity 100000 Pa*s/m^2 on a rigid backing. It is a documented engineering "
           "reference, not a claim to represent one specific carpet."));
    floorLayout->addWidget(new QLabel(tr("Surface:"), floorGroup), 2, 2);
    floorLayout->addWidget(page.floorSurface, 2, 3);

    page.floorReflectionStatus = new QLabel(floorGroup);
    page.floorReflectionStatus->setObjectName(
        QStringLiteral("floorReflectionStatus%1").arg(driverIndex + 1));
    page.floorReflectionStatus->setWordWrap(true);
    floorLayout->addWidget(new QLabel(tr("Status:"), floorGroup), 3, 0);
    floorLayout->addWidget(page.floorReflectionStatus, 3, 1, 1, 3);

    layout->addWidget(floorGroup);
    layout->addStretch(1);

    auto changed = [this, driverIndex]() {
        if (m_loading) {
            return;
        }
        writePageToWorkingModel(driverIndex);
        updatePageState(driverIndex);
        previewWorkingModel();
    };

    connect(page.enabled, &QCheckBox::toggled, this, [changed](bool) { changed(); });
    connect(page.model, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [changed](int) { changed(); });
    connect(page.boundaryCondition, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [changed](int) { changed(); });
    connect(page.width, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.height, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.driverX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.driverY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.leftEdgeTreatment, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [changed](int) { changed(); });
    connect(page.leftChamferSetback, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.rightEdgeTreatment, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [changed](int) { changed(); });
    connect(page.rightChamferSetback, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.showResponse, &QCheckBox::toggled, this, [changed](bool) { changed(); });
    connect(page.floorReflectionEnabled, &QCheckBox::toggled,
            this, [changed](bool) { changed(); });
    connect(page.cabinetBottomAboveFloor, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.listenerHeightAboveFloor, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.listeningDistance, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.floorSurface, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [changed](int) { changed(); });

    return page.page;
}

void BaffleParametersDialog::loadFromWorkingModel()
{
    m_loading = true;
    for (int driverIndex = 0; driverIndex < KFilterProjectIo::DriverCount; ++driverIndex) {
        loadDriverPage(driverIndex);
    }
    m_loading = false;
}

void BaffleParametersDialog::loadDriverPage(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    const BaffleSettings& settings = m_workingSettings.at(static_cast<std::size_t>(driverIndex));
    const FloorReflectionSettings& floorSettings =
        m_workingFloorReflectionSettings.at(static_cast<std::size_t>(driverIndex));

    const QSignalBlocker enabledBlocker(page.enabled);
    const QSignalBlocker modelBlocker(page.model);
    const QSignalBlocker boundaryBlocker(page.boundaryCondition);
    const QSignalBlocker widthBlocker(page.width);
    const QSignalBlocker heightBlocker(page.height);
    const QSignalBlocker driverXBlocker(page.driverX);
    const QSignalBlocker driverYBlocker(page.driverY);
    const QSignalBlocker leftTreatmentBlocker(page.leftEdgeTreatment);
    const QSignalBlocker leftChamferBlocker(page.leftChamferSetback);
    const QSignalBlocker rightTreatmentBlocker(page.rightEdgeTreatment);
    const QSignalBlocker rightChamferBlocker(page.rightChamferSetback);
    const QSignalBlocker responseBlocker(page.showResponse);
    const QSignalBlocker floorEnabledBlocker(page.floorReflectionEnabled);
    const QSignalBlocker cabinetBottomBlocker(page.cabinetBottomAboveFloor);
    const QSignalBlocker listenerHeightBlocker(page.listenerHeightAboveFloor);
    const QSignalBlocker listeningDistanceBlocker(page.listeningDistance);
    const QSignalBlocker floorSurfaceBlocker(page.floorSurface);

    page.enabled->setChecked(settings.enabled);
    const int modelIndex = page.model->findData(static_cast<int>(settings.model));
    page.model->setCurrentIndex(modelIndex >= 0 ? modelIndex : 0);
    const int boundaryIndex =
        page.boundaryCondition->findData(static_cast<int>(settings.boundaryCondition));
    page.boundaryCondition->setCurrentIndex(boundaryIndex >= 0 ? boundaryIndex : 0);
    page.width->setValue(std::isfinite(settings.widthMm) && settings.widthMm > 0.0
                             ? settings.widthMm
                             : BaffleSettings{}.widthMm);
    page.height->setValue(std::isfinite(settings.heightMm) && settings.heightMm >= 0.0
                              ? settings.heightMm
                              : BaffleSettings{}.heightMm);
    page.driverX->setValue(std::isfinite(settings.driverXmm) && settings.driverXmm >= 0.0
                               ? settings.driverXmm
                               : BaffleSettings{}.driverXmm);
    page.driverY->setValue(std::isfinite(settings.driverYmm) && settings.driverYmm >= 0.0
                               ? settings.driverYmm
                               : BaffleSettings{}.driverYmm);
    const int leftTreatmentIndex =
        page.leftEdgeTreatment->findData(static_cast<int>(settings.leftEdgeTreatment));
    page.leftEdgeTreatment->setCurrentIndex(leftTreatmentIndex >= 0 ? leftTreatmentIndex : 0);
    page.leftChamferSetback->setValue(
        std::isfinite(settings.leftChamferSetbackMm) && settings.leftChamferSetbackMm >= 5.0
            ? settings.leftChamferSetbackMm
            : BaffleSettings{}.leftChamferSetbackMm);
    const int rightTreatmentIndex =
        page.rightEdgeTreatment->findData(static_cast<int>(settings.rightEdgeTreatment));
    page.rightEdgeTreatment->setCurrentIndex(rightTreatmentIndex >= 0 ? rightTreatmentIndex : 0);
    page.rightChamferSetback->setValue(
        std::isfinite(settings.rightChamferSetbackMm) && settings.rightChamferSetbackMm >= 5.0
            ? settings.rightChamferSetbackMm
            : BaffleSettings{}.rightChamferSetbackMm);
    page.showResponse->setChecked(settings.showResponseInPlot);

    page.floorReflectionEnabled->setChecked(floorSettings.enabled);
    page.cabinetBottomAboveFloor->setValue(
        std::isfinite(floorSettings.cabinetBottomAboveFloorMm) &&
                floorSettings.cabinetBottomAboveFloorMm >= 0.0
            ? floorSettings.cabinetBottomAboveFloorMm
            : FloorReflectionSettings{}.cabinetBottomAboveFloorMm);
    page.listenerHeightAboveFloor->setValue(
        std::isfinite(floorSettings.listenerHeightAboveFloorMm) &&
                floorSettings.listenerHeightAboveFloorMm >= 0.0
            ? floorSettings.listenerHeightAboveFloorMm
            : FloorReflectionSettings{}.listenerHeightAboveFloorMm);
    page.listeningDistance->setValue(
        std::isfinite(floorSettings.horizontalDistanceMm) &&
                floorSettings.horizontalDistanceMm >= 0.0
            ? floorSettings.horizontalDistanceMm
            : FloorReflectionSettings{}.horizontalDistanceMm);
    const int floorSurfaceIndex =
        page.floorSurface->findData(static_cast<int>(floorSettings.surfacePreset));
    page.floorSurface->setCurrentIndex(floorSurfaceIndex >= 0 ? floorSurfaceIndex : 0);

    updatePageState(driverIndex);
}

void BaffleParametersDialog::writePageToWorkingModel(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    BaffleSettings& settings = m_workingSettings.at(static_cast<std::size_t>(driverIndex));

    settings.enabled = page.enabled->isChecked();
    settings.model = static_cast<BaffleModel>(page.model->currentData().toInt());
    settings.boundaryCondition = static_cast<BaffleBoundaryCondition>(
        page.boundaryCondition->currentData().toInt());
    settings.widthMm = page.width->value();
    settings.heightMm = page.height->value();
    settings.driverXmm = page.driverX->value();
    settings.driverYmm = page.driverY->value();
    settings.leftEdgeTreatment = static_cast<BaffleSideEdgeTreatment>(
        page.leftEdgeTreatment->currentData().toInt());
    settings.leftChamferSetbackMm = page.leftChamferSetback->value();
    settings.rightEdgeTreatment = static_cast<BaffleSideEdgeTreatment>(
        page.rightEdgeTreatment->currentData().toInt());
    settings.rightChamferSetbackMm = page.rightChamferSetback->value();
    settings.showResponseInPlot = page.showResponse->isChecked();

    FloorReflectionSettings& floorSettings =
        m_workingFloorReflectionSettings.at(static_cast<std::size_t>(driverIndex));
    floorSettings.enabled = page.floorReflectionEnabled->isChecked();
    floorSettings.cabinetBottomAboveFloorMm = page.cabinetBottomAboveFloor->value();
    floorSettings.listenerHeightAboveFloorMm = page.listenerHeightAboveFloor->value();
    floorSettings.horizontalDistanceMm = page.listeningDistance->value();
    floorSettings.surfacePreset = static_cast<FloorSurfacePreset>(
        page.floorSurface->currentData().toInt());
}

void BaffleParametersDialog::updatePageState(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    const bool enabled = page.enabled->isChecked();
    const BaffleModel model = static_cast<BaffleModel>(page.model->currentData().toInt());
    const bool rectangular = model == BaffleModel::RectangularEdgeDiffraction;
    const BaffleBoundaryCondition boundaryCondition =
        static_cast<BaffleBoundaryCondition>(page.boundaryCondition->currentData().toInt());
    const bool rigidFloor = rectangular &&
        boundaryCondition == BaffleBoundaryCondition::RigidFloorContactDiffractionOnly;
    const bool leftChamfer = rectangular &&
        static_cast<BaffleSideEdgeTreatment>(page.leftEdgeTreatment->currentData().toInt()) ==
            BaffleSideEdgeTreatment::Chamfer45;
    const bool rightChamfer = rectangular &&
        static_cast<BaffleSideEdgeTreatment>(page.rightEdgeTreatment->currentData().toInt()) ==
            BaffleSideEdgeTreatment::Chamfer45;
    const bool floorReflectionEnabled = page.floorReflectionEnabled->isChecked();

    page.model->setEnabled(enabled);
    page.boundaryCondition->setEnabled(enabled && rectangular);
    page.width->setEnabled(enabled);
    // Floor Reflection shares the physical baffle height and Driver-Y geometry
    // even when Baffle / Diffraction processing itself is bypassed or uses the
    // width-only Simple Baffle Step model. Keep those two geometry fields
    // editable whenever either productive stage needs them.
    page.height->setEnabled((enabled && rectangular) || floorReflectionEnabled);
    page.driverX->setEnabled(enabled && rectangular);
    page.driverY->setEnabled((enabled && rectangular) || floorReflectionEnabled);
    page.leftEdgeTreatment->setEnabled(enabled && rectangular);
    page.rightEdgeTreatment->setEnabled(enabled && rectangular);

    const int rigidFloorIndex = page.boundaryCondition->findData(
        static_cast<int>(BaffleBoundaryCondition::RigidFloorContactDiffractionOnly));
    setComboItemEnabled(page.boundaryCondition, rigidFloorIndex, !leftChamfer && !rightChamfer);
    const int leftChamferIndex = page.leftEdgeTreatment->findData(
        static_cast<int>(BaffleSideEdgeTreatment::Chamfer45));
    const int rightChamferIndex = page.rightEdgeTreatment->findData(
        static_cast<int>(BaffleSideEdgeTreatment::Chamfer45));
    setComboItemEnabled(page.leftEdgeTreatment, leftChamferIndex, !rigidFloor);
    setComboItemEnabled(page.rightEdgeTreatment, rightChamferIndex, !rigidFloor);

    page.leftChamferSetback->setEnabled(enabled && leftChamfer && !rigidFloor);
    page.rightChamferSetback->setEnabled(enabled && rightChamfer && !rigidFloor);
    page.geometryPreview->setDriverDragEnabled(enabled && rectangular);
    // Diagnostic visibility remains editable even while the processing stage is
    // bypassed, mirroring the Active Filters dialog semantics.
    page.showResponse->setEnabled(true);

    page.cabinetBottomAboveFloor->setEnabled(floorReflectionEnabled);
    page.listenerHeightAboveFloor->setEnabled(floorReflectionEnabled);
    page.listeningDistance->setEnabled(floorReflectionEnabled);
    page.floorSurface->setEnabled(floorReflectionEnabled);

    updateGeometryPreview(driverIndex);
    updateMidpointLabel(driverIndex);
    updateResponseStatus(driverIndex);
    updateFloorReflectionStatus(driverIndex);
}

void BaffleParametersDialog::updateGeometryPreview(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    if (page.geometryPreview == nullptr) {
        return;
    }

    const bool rectangular =
        static_cast<BaffleModel>(page.model->currentData().toInt()) ==
            BaffleModel::RectangularEdgeDiffraction;
    const bool leftChamfer = rectangular &&
        static_cast<BaffleSideEdgeTreatment>(page.leftEdgeTreatment->currentData().toInt()) ==
            BaffleSideEdgeTreatment::Chamfer45;
    const bool rightChamfer = rectangular &&
        static_cast<BaffleSideEdgeTreatment>(page.rightEdgeTreatment->currentData().toInt()) ==
            BaffleSideEdgeTreatment::Chamfer45;
    page.geometryPreview->setGeometryValues(page.width->value(),
                                            page.height->value(),
                                            page.driverX->value(),
                                            page.driverY->value(),
                                            leftChamfer ? page.leftChamferSetback->value() : 0.0,
                                            rightChamfer ? page.rightChamferSetback->value() : 0.0);
}

void BaffleParametersDialog::updateMidpointLabel(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    const BaffleModel model = static_cast<BaffleModel>(page.model->currentData().toInt());
    if (model != BaffleModel::SimpleBaffleStep) {
        page.midpoint->setText(tr("not applicable (Rectangular Edge Diffraction)"));
        return;
    }

    const double midpointHz = simpleBaffleStepMidpointFrequencyHz(page.width->value());
    if (!std::isfinite(midpointHz) || midpointHz <= 0.0) {
        page.midpoint->setText(tr("invalid"));
        return;
    }

    page.midpoint->setText(midpointHz >= 1000.0
                               ? tr("%1 kHz").arg(midpointHz / 1000.0, 0, 'f', 2)
                               : tr("%1 Hz").arg(midpointHz, 0, 'f', 1));
}

void BaffleParametersDialog::updateResponseStatus(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    const bool enabled = page.enabled->isChecked();
    const BaffleModel model = static_cast<BaffleModel>(page.model->currentData().toInt());
    const BaffleBoundaryCondition boundaryCondition = static_cast<BaffleBoundaryCondition>(
        page.boundaryCondition->currentData().toInt());

    if (!enabled) {
        page.responseStatus->setText(tr("Response: bypassed (unity)."));
        return;
    }

    if (model == BaffleModel::SimpleBaffleStep) {
        const double midpointHz = simpleBaffleStepMidpointFrequencyHz(page.width->value());
        page.responseStatus->setText(
            std::isfinite(midpointHz) && midpointHz > 0.0
                ? tr("Response: valid complex Simple Baffle Step (0 dB → +6.02 dB).")
                : tr("Response: invalid baffle width; Baffle stage is bypassed."));
        return;
    }

    if (model != BaffleModel::RectangularEdgeDiffraction) {
        page.responseStatus->setText(tr("Response: unsupported model; Baffle stage is bypassed."));
        return;
    }

    const BaffleSideEdgeTreatment leftTreatment = static_cast<BaffleSideEdgeTreatment>(
        page.leftEdgeTreatment->currentData().toInt());
    const BaffleSideEdgeTreatment rightTreatment = static_cast<BaffleSideEdgeTreatment>(
        page.rightEdgeTreatment->currentData().toInt());
    const double leftSetback = leftTreatment == BaffleSideEdgeTreatment::Chamfer45
                                   ? page.leftChamferSetback->value()
                                   : 0.0;
    const double rightSetback = rightTreatment == BaffleSideEdgeTreatment::Chamfer45
                                    ? page.rightChamferSetback->value()
                                    : 0.0;
    const double width = page.width->value();
    const double height = page.height->value();
    const double driverX = page.driverX->value();
    const double driverY = page.driverY->value();
    const bool validGeometry =
        std::isfinite(width) && std::isfinite(height) &&
        std::isfinite(driverX) && std::isfinite(driverY) &&
        std::isfinite(leftSetback) && std::isfinite(rightSetback) &&
        width > 0.0 && height > 0.0 &&
        driverY > 0.0 && driverY < height &&
        leftSetback + rightSetback < width &&
        driverX > leftSetback && driverX < width - rightSetback &&
        (leftTreatment != BaffleSideEdgeTreatment::Chamfer45 || leftSetback >= 5.0) &&
        (rightTreatment != BaffleSideEdgeTreatment::Chamfer45 || rightSetback >= 5.0);

    if (!validGeometry) {
        page.responseStatus->setText(
            tr("Response: invalid geometry; width/height must be positive, active chamfers must be at least 5 mm, and the driver centre must remain on the flat front surface. Baffle stage is bypassed."));
        return;
    }

    const bool chamfered = leftTreatment == BaffleSideEdgeTreatment::Chamfer45 ||
                           rightTreatment == BaffleSideEdgeTreatment::Chamfer45;
    if (boundaryCondition == BaffleBoundaryCondition::RigidFloorContactDiffractionOnly) {
        if (chamfered) {
            page.responseStatus->setText(
                tr("Response: Rigid floor contact currently supports Sharp side edges only; Baffle stage is bypassed."));
            return;
        }
        page.responseStatus->setText(
            tr("Response: valid Rectangular Edge Diffraction with ideal rigid floor contact (diffraction geometry only; no floor-bounce/listening-position gain; finite driver source from Dm when it fits)."));
        return;
    }
    if (boundaryCondition != BaffleBoundaryCondition::FreeField) {
        page.responseStatus->setText(
            tr("Response: unsupported boundary condition; Baffle stage is bypassed."));
        return;
    }

    page.responseStatus->setText(
        chamfered
            ? tr("Response: valid complex Rectangular Edge Diffraction with 45-degree side chamfer correction (finite driver source from Dm when it fits; point-source fallback otherwise).")
            : tr("Response: valid complex Rectangular Edge Diffraction with sharp side edges (finite driver source from Dm when it fits; point-source fallback otherwise)."));
}

void BaffleParametersDialog::updateFloorReflectionStatus(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));

    if (!page.floorReflectionEnabled->isChecked()) {
        page.floorReflectionStatus->setText(tr("bypassed (unity)."));
        return;
    }

    const FloorSurfacePreset surfacePreset =
        static_cast<FloorSurfacePreset>(page.floorSurface->currentData().toInt());
    if (surfacePreset != FloorSurfacePreset::HardRigid &&
        surfacePreset != FloorSurfacePreset::MikiReference10mm100k) {
        page.floorReflectionStatus->setText(
            tr("unsupported surface preset; Floor Reflection is bypassed."));
        return;
    }

    const double baffleHeightMm = page.height->value();
    const double driverYmm = page.driverY->value();
    const double cabinetBottomMm = page.cabinetBottomAboveFloor->value();
    const double listenerHeightMm = page.listenerHeightAboveFloor->value();
    const double distanceMm = page.listeningDistance->value();

    if (!std::isfinite(baffleHeightMm) || !std::isfinite(driverYmm) ||
        !std::isfinite(cabinetBottomMm) || !std::isfinite(listenerHeightMm) ||
        !std::isfinite(distanceMm) || baffleHeightMm <= 0.0 || driverYmm < 0.0 ||
        driverYmm > baffleHeightMm || cabinetBottomMm < 0.0 ||
        listenerHeightMm < 0.0 || distanceMm < 0.0) {
        page.floorReflectionStatus->setText(
            tr("invalid geometry; set Baffle height > 0 and keep Driver Y from top within the baffle. "
               "Floor Reflection is bypassed."));
        return;
    }

    const double sourceHeightMm = cabinetBottomMm + baffleHeightMm - driverYmm;
    const double directVerticalMm = listenerHeightMm - sourceHeightMm;
    const double imageVerticalMm = listenerHeightMm + sourceHeightMm;
    const double directDistanceMm = std::hypot(distanceMm, directVerticalMm);
    const double imageDistanceMm = std::hypot(distanceMm, imageVerticalMm);
    if (!std::isfinite(sourceHeightMm) || !std::isfinite(directDistanceMm) ||
        !std::isfinite(imageDistanceMm) || sourceHeightMm < 0.0 ||
        directDistanceMm <= 0.0 || imageDistanceMm <= 0.0) {
        page.floorReflectionStatus->setText(
            tr("degenerate source/listener geometry; Floor Reflection is bypassed."));
        return;
    }

    if (surfacePreset == FloorSurfacePreset::HardRigid) {
        page.floorReflectionStatus->setText(
            tr("active: Hard / rigid floor; derived source height %1 mm.")
                .arg(sourceHeightMm, 0, 'f', 1));
    } else {
        page.floorReflectionStatus->setText(
            tr("active: Porous floor - Miki reference (10 mm, 100000 Pa*s/m^2, rigid backing; "
               "experimental); derived source height %1 mm.")
                .arg(sourceHeightMm, 0, 'f', 1));
    }
}

void BaffleParametersDialog::previewWorkingModel()
{
    m_settings = m_workingSettings;
    m_floorReflectionSettings = m_workingFloorReflectionSettings;
    emit parametersPreviewChanged();
}

void BaffleParametersDialog::applyToModel()
{
    m_settings = m_workingSettings;
    m_floorReflectionSettings = m_workingFloorReflectionSettings;
    m_committedSettings = m_workingSettings;
    m_committedFloorReflectionSettings = m_workingFloorReflectionSettings;
    emit parametersApplied();
}
