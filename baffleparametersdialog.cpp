/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleparametersdialog.h"

#include "baffleresponse.h"
#include "networkvalueutils.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QValidator>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
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
    explicit BaffleValueSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setCorrectionMode(QAbstractSpinBox::CorrectToPreviousValue);
    }

protected:
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
};

QDoubleSpinBox *createGeometrySpinBox(QWidget *parent, const QString& objectName)
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

BaffleParametersDialog::BaffleParametersDialog(BaffleSettingsPerDriver& settings,
                                               QWidget *parent,
                                               int initialDriverIndex)
    : QDialog(parent),
      m_settings(settings),
      m_committedSettings(settings),
      m_workingSettings(settings)
{
    setWindowTitle(tr("Baffle / Diffraction Parameters"));
    resize(660, 520);

    auto *mainLayout = new QVBoxLayout(this);

    auto *notice = new QLabel(
        tr("Simple Baffle Step uses only the baffle width. Rectangular Edge Diffraction "
           "uses width, height and the driver position on a sharp-edged rectangular baffle. "
           "Changes are previewed live; Apply/OK commits the edited project state and Cancel "
           "restores the last applied state."),
        this);
    notice->setWordWrap(true);
    notice->setObjectName(QStringLiteral("baffleStageNotice"));
    mainLayout->addWidget(notice);

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
    auto *form = new QFormLayout(parametersGroup);

    page.model = new QComboBox(parametersGroup);
    page.model->setObjectName(QStringLiteral("baffleModelCombo%1").arg(driverIndex + 1));
    page.model->addItem(tr("Simple Baffle Step"), static_cast<int>(BaffleModel::SimpleBaffleStep));
    page.model->addItem(tr("Rectangular Edge Diffraction"),
                        static_cast<int>(BaffleModel::RectangularEdgeDiffraction));
    page.model->setToolTip(
        tr("Simple Baffle Step is a smooth width-only shelf. Rectangular Edge Diffraction "
           "uses a point-source, sharp-edge, on-axis far-field model."));
    form->addRow(tr("Model:"), page.model);

    page.width = createGeometrySpinBox(parametersGroup,
                                       QStringLiteral("baffleWidthSpin%1").arg(driverIndex + 1));
    page.width->setRange(1.0, 10000.0);
    page.width->setToolTip(
        tr("Baffle width. Simple Baffle Step uses f0 = 115 / W[m]; Rectangular Edge Diffraction uses the physical geometry."));
    form->addRow(tr("Baffle width:"), page.width);

    page.height = createGeometrySpinBox(parametersGroup,
                                        QStringLiteral("baffleHeightSpin%1").arg(driverIndex + 1));
    page.height->setToolTip(tr("Rectangular baffle height."));
    form->addRow(tr("Baffle height:"), page.height);

    page.driverX = createGeometrySpinBox(parametersGroup,
                                         QStringLiteral("baffleDriverXSpin%1").arg(driverIndex + 1));
    page.driverX->setToolTip(tr("Driver centre measured from the left baffle edge."));
    form->addRow(tr("Driver X from left:"), page.driverX);

    page.driverY = createGeometrySpinBox(parametersGroup,
                                         QStringLiteral("baffleDriverYSpin%1").arg(driverIndex + 1));
    page.driverY->setToolTip(tr("Driver centre measured from the top baffle edge."));
    form->addRow(tr("Driver Y from top:"), page.driverY);

    page.midpoint = new QLabel(parametersGroup);
    page.midpoint->setObjectName(QStringLiteral("baffleMidpointLabel%1").arg(driverIndex + 1));
    form->addRow(tr("Calculated midpoint:"), page.midpoint);

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
    connect(page.width, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.height, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.driverX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.driverY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [changed](double) { changed(); });
    connect(page.showResponse, &QCheckBox::toggled, this, [changed](bool) { changed(); });

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

    const QSignalBlocker enabledBlocker(page.enabled);
    const QSignalBlocker modelBlocker(page.model);
    const QSignalBlocker widthBlocker(page.width);
    const QSignalBlocker heightBlocker(page.height);
    const QSignalBlocker driverXBlocker(page.driverX);
    const QSignalBlocker driverYBlocker(page.driverY);
    const QSignalBlocker responseBlocker(page.showResponse);

    page.enabled->setChecked(settings.enabled);
    const int modelIndex = page.model->findData(static_cast<int>(settings.model));
    page.model->setCurrentIndex(modelIndex >= 0 ? modelIndex : 0);
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
    page.showResponse->setChecked(settings.showResponseInPlot);
    updatePageState(driverIndex);
}

void BaffleParametersDialog::writePageToWorkingModel(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    BaffleSettings& settings = m_workingSettings.at(static_cast<std::size_t>(driverIndex));

    settings.enabled = page.enabled->isChecked();
    settings.model = static_cast<BaffleModel>(page.model->currentData().toInt());
    settings.widthMm = page.width->value();
    settings.heightMm = page.height->value();
    settings.driverXmm = page.driverX->value();
    settings.driverYmm = page.driverY->value();
    settings.showResponseInPlot = page.showResponse->isChecked();
}

void BaffleParametersDialog::updatePageState(int driverIndex)
{
    DriverPage& page = m_pages.at(static_cast<std::size_t>(driverIndex));
    const bool enabled = page.enabled->isChecked();
    const BaffleModel model = static_cast<BaffleModel>(page.model->currentData().toInt());
    const bool rectangular = model == BaffleModel::RectangularEdgeDiffraction;

    page.model->setEnabled(enabled);
    page.width->setEnabled(enabled);
    page.height->setEnabled(enabled && rectangular);
    page.driverX->setEnabled(enabled && rectangular);
    page.driverY->setEnabled(enabled && rectangular);
    // Diagnostic visibility remains editable even while the processing stage is
    // bypassed, mirroring the Active Filters dialog semantics.
    page.showResponse->setEnabled(true);
    updateMidpointLabel(driverIndex);
    updateResponseStatus(driverIndex);
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
    BaffleSettings settings = m_workingSettings.at(static_cast<std::size_t>(driverIndex));
    settings.enabled = page.enabled->isChecked();
    settings.model = static_cast<BaffleModel>(page.model->currentData().toInt());
    settings.widthMm = page.width->value();
    settings.heightMm = page.height->value();
    settings.driverXmm = page.driverX->value();
    settings.driverYmm = page.driverY->value();
    settings.showResponseInPlot = page.showResponse->isChecked();

    const BaffleResponse response = calculateBaffleResponse(settings);
    switch (response.status) {
    case BaffleResponseStatus::Neutral:
        page.responseStatus->setText(tr("Response: bypassed (unity)."));
        break;
    case BaffleResponseStatus::Valid:
        if (settings.model == BaffleModel::SimpleBaffleStep) {
            page.responseStatus->setText(
                tr("Response: valid complex Simple Baffle Step (0 dB → +6.02 dB)."));
        } else {
            page.responseStatus->setText(
                tr("Response: valid complex Rectangular Edge Diffraction (finite driver source from Dm when it fits; point-source fallback otherwise)."));
        }
        break;
    case BaffleResponseStatus::UnsupportedModel:
        page.responseStatus->setText(tr("Response: unsupported model; Baffle stage is bypassed."));
        break;
    case BaffleResponseStatus::InvalidParameters:
        page.responseStatus->setText(
            tr("Response: invalid geometry; width/height must be positive and the driver centre must lie strictly inside the baffle. Baffle stage is bypassed."));
        break;
    }
}

void BaffleParametersDialog::previewWorkingModel()
{
    m_settings = m_workingSettings;
    emit parametersPreviewChanged();
}

void BaffleParametersDialog::applyToModel()
{
    m_settings = m_workingSettings;
    m_committedSettings = m_workingSettings;
    emit parametersApplied();
}
