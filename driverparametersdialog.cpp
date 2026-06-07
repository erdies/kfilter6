/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "driverparametersdialog.h"

#include "driver.h"
#include "networkvalueutils.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QTabWidget>
#include <QTimer>
#include <QVariant>
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

class DriverValueSpinBox : public QDoubleSpinBox
{
public:
    explicit DriverValueSpinBox(QWidget *parent = nullptr)
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

QDoubleSpinBox *createSpinBox(double minimum,
                              double maximum,
                              int decimals,
                              double singleStep,
                              const QString& suffix = QString())
{
    auto *spinBox = new DriverValueSpinBox;
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(decimals);
    spinBox->setSingleStep(singleStep);
    spinBox->setKeyboardTracking(false);
    if (!suffix.isEmpty()) {
        spinBox->setSuffix(suffix);
    }
    return spinBox;
}

void addRow(QFormLayout *layout, const QString& label, QWidget *editor)
{
    layout->addRow(label, editor);
}

double linearGainToDb(double gain)
{
    if (gain <= 0.0 || !std::isfinite(gain)) {
        return 0.0;
    }
    return 20.0 * std::log10(gain);
}

double dbToLinearGain(double db)
{
    return std::pow(10.0, db / 20.0);
}

double calculateBassReflexTubeLengthCm(double diameterCm, double fbHz, double vbLiter)
{
    if (diameterCm <= 0.0 || fbHz <= 0.0 || vbLiter <= 0.0 ||
        !std::isfinite(diameterCm) || !std::isfinite(fbHz) || !std::isfinite(vbLiter)) {
        return 0.0;
    }

    constexpr double pi = 3.14159265358979323846;
    const double tubeAreaCm2 = pi * std::pow(diameterCm * 0.5, 2.0);
    const double effectiveLengthCm = 29830.0 * tubeAreaCm2 / (fbHz * fbHz * vbLiter);
    return effectiveLengthCm - 0.825 * std::sqrt(tubeAreaCm2);
}

QString tubeDiameterSettingsKey(int driverIndex)
{
    return QStringLiteral("DriverParameters/tubeDiameter%1").arg(driverIndex + 1);
}
}

DriverParametersDialog::DriverParametersDialog(driver (&drivers)[KFilterProjectIo::DriverCount],
                                               QWidget *parent,
                                               int initialDriverIndex)
    : QDialog(parent),
      m_drivers(drivers)
{
    setWindowTitle(tr("Driver parameters"));
    resize(740, 520);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(150);
    connect(m_previewTimer, &QTimer::timeout, this, &DriverParametersDialog::emitPreview);

    auto *mainLayout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        m_tabs->addTab(createDriverPage(index), tr("Driver %1").arg(index + 1));
    }

    const int safeInitialDriverIndex =
        std::clamp(initialDriverIndex, 0, KFilterProjectIo::DriverCount - 1);
    m_tabs->setCurrentIndex(safeInitialDriverIndex);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Apply |
                                           QDialogButtonBox::Cancel,
                                           this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &DriverParametersDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &DriverParametersDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &DriverParametersDialog::applyClicked);

    mainLayout->addWidget(m_tabs);
    mainLayout->addWidget(buttonBox);

    loadFromDrivers();
    rememberCommittedDrivers();
}

int DriverParametersDialog::currentDriverIndex() const
{
    return m_tabs ? m_tabs->currentIndex() : 0;
}

QWidget *DriverParametersDialog::createDriverPage(int index)
{
    DriverPage& page = m_pages.at(index);
    page.page = new QWidget(this);

    page.title = new QLineEdit(page.page);
    page.rdc = createSpinBox(0.001, 1000.0, 4, 0.1, tr(" Ohm"));
    page.lspMilliHenry = createSpinBox(0.0, 1000.0, 6, 0.01, tr(" mH"));
    page.f0 = createSpinBox(0.001, 100000.0, 3, 1.0, tr(" Hz"));
    page.qts = createSpinBox(0.001, 1000.0, 5, 0.01);
    page.qes = createSpinBox(0.001, 1000.0, 5, 0.01);
    page.qms = createSpinBox(0.001, 1000.0, 5, 0.01);
    page.vas = createSpinBox(0.0, 100000.0, 3, 1.0, tr(" l"));
    page.dm = createSpinBox(0.0, 1000.0, 3, 0.1, tr(" cm"));
    page.vb = createSpinBox(0.0, 100000.0, 3, 1.0, tr(" l"));
    page.ql = createSpinBox(0.001, 1000.0, 5, 0.1);
    page.fb = createSpinBox(0.0, 100000.0, 3, 1.0, tr(" Hz"));
    page.tubeDiameter = createSpinBox(0.0, 1000.0, 3, 0.1, tr(" cm"));
    page.tubeLength = new QLineEdit(page.page);
    page.tubeLength->setReadOnly(true);
    page.tubeLength->setText(tr("n. a."));
    page.v2 = createSpinBox(0.0, 100000.0, 3, 1.0, tr(" l"));
    page.gainDb = createSpinBox(-60.0, 60.0, 2, 0.5, tr(" dB"));

    const int enclosureEditorWidth = page.gainDb->sizeHint().width();
    page.vb->setFixedWidth(enclosureEditorWidth);
    page.ql->setFixedWidth(enclosureEditorWidth);
    page.fb->setFixedWidth(enclosureEditorWidth);
    page.tubeDiameter->setFixedWidth(enclosureEditorWidth);
    page.v2->setFixedWidth(enclosureEditorWidth);

    page.alignmentProposal = new QComboBox(page.page);
    page.alignmentProposal->addItem(tr("Open baffle driver"), 0);
    page.alignmentProposal->addItem(tr("Sealed enclosure"), 1);
    page.alignmentProposal->addItem(tr("Vented enclosure"), 2);
    page.alignmentProposal->addItem(tr("Bandpass enclosure"), 3);

    page.pressureActive = new QCheckBox(tr("Show SPL curve"), page.page);
    page.impedanceActive = new QCheckBox(tr("Show impedance curve"), page.page);
    page.summaryActive = new QCheckBox(tr("Include in vector SPL sum"), page.page);
    page.scalarSummaryActive = new QCheckBox(tr("Include in energetic SPL sum"), page.page);
    page.impedanceSummaryActive = new QCheckBox(tr("Include in total impedance"), page.page);
    page.invertPhase = new QCheckBox(tr("Invert polarity"), page.page);
    page.fullCircuit = new QCheckBox(tr("Use full crossover simulation"), page.page);

    page.pressureActive->setToolTip(tr("Show this driver's sound pressure level curve."));
    page.impedanceActive->setToolTip(tr("Show this driver's impedance curve."));
    page.summaryActive->setToolTip(tr("Include this driver's SPL curve in the phase-aware vector sum."));
    page.scalarSummaryActive->setToolTip(tr("Include this driver's SPL curve in the energetic SPL sum. Unlike the vector SPL sum, phase cancellation and addition are ignored."));
    page.impedanceSummaryActive->setToolTip(tr("Include this driver's impedance curve in the total parallel impedance seen by the amplifier."));
    page.invertPhase->setToolTip(tr("Invert this driver's polarity by 180 degrees for the SPL calculation."));
    page.fullCircuit->setToolTip(tr("Use the full crossover network for this driver instead of the simplified calculation."));
    page.ql->setToolTip(tr("Enclosure loss damping factor used by vented and bandpass calculations. Must be greater than zero."));
    page.tubeDiameter->setToolTip(tr("Inner diameter of one round bass reflex tube. The calculated length uses Vb and Fb."));
    page.tubeLength->setToolTip(tr("Calculated physical bass reflex tube length from Vb, Fb, and tube diameter."));

    auto *leftForm = new QFormLayout;
    addRow(leftForm, tr("Driver name"), page.title);
    addRow(leftForm, tr("Rdc"), page.rdc);
    addRow(leftForm, tr("Lsp"), page.lspMilliHenry);
    addRow(leftForm, tr("Fs"), page.f0);
    addRow(leftForm, tr("Qts"), page.qts);
    addRow(leftForm, tr("Qes"), page.qes);
    addRow(leftForm, tr("Qms"), page.qms);
    addRow(leftForm, tr("Vas"), page.vas);
    addRow(leftForm, tr("Diameter"), page.dm);

    auto *qtsButton = new QPushButton(tr("Calculate Qts from Qes and Qms"), page.page);
    connect(qtsButton, &QPushButton::clicked, this, [this, index]() {
        updateQtsForPage(m_pages.at(index));
    });
    leftForm->addRow(QString(), qtsButton);

    auto *boxForm = new QFormLayout;

    auto *vbLayoutWidget = new QWidget(page.page);
    auto *vbLayout = new QHBoxLayout(vbLayoutWidget);
    vbLayout->setContentsMargins(0, 0, 0, 0);
    vbLayout->addWidget(page.vb);
    auto *qlLabel = new QLabel(tr("Ql"), vbLayoutWidget);
    vbLayout->addWidget(qlLabel);
    vbLayout->addWidget(page.ql);
    vbLayout->addStretch(1);
    addRow(boxForm, tr("Vb"), vbLayoutWidget);

    auto *fbLayoutWidget = new QWidget(page.page);
    auto *fbLayout = new QHBoxLayout(fbLayoutWidget);
    fbLayout->setContentsMargins(0, 0, 0, 0);
    fbLayout->addWidget(page.fb);
    auto *qlLabelSpacer = new QWidget(fbLayoutWidget);
    qlLabelSpacer->setFixedWidth(qlLabel->sizeHint().width());
    fbLayout->addWidget(qlLabelSpacer);
    auto *hogeButton = new QPushButton(tr("Hoge"), fbLayoutWidget);
    hogeButton->setFixedWidth(enclosureEditorWidth);
    hogeButton->setToolTip(tr("Calculate Vb and Fb from Fs, Qts, and Vas using the legacy Hoge formula."));
    fbLayout->addWidget(hogeButton);
    fbLayout->addStretch(1);
    addRow(boxForm, tr("Fb"), fbLayoutWidget);
    connect(hogeButton, &QPushButton::clicked, this, [this, index]() {
        calculateHogeForPage(m_pages.at(index));
    });

    auto *tubeLayoutWidget = new QWidget(page.page);
    auto *tubeLayout = new QHBoxLayout(tubeLayoutWidget);
    tubeLayout->setContentsMargins(0, 0, 0, 0);
    tubeLayout->addWidget(page.tubeDiameter);
    tubeLayout->addWidget(new QLabel(tr("Tube length"), tubeLayoutWidget));
    tubeLayout->addWidget(page.tubeLength);
    addRow(boxForm, tr("Tube diameter"), tubeLayoutWidget);

    addRow(boxForm, tr("V2"), page.v2);
    addRow(boxForm, tr("Gain"), page.gainDb);
    addRow(boxForm, tr("Enclosure type"), page.alignmentProposal);

    auto *optionsLayout = new QVBoxLayout;
    optionsLayout->addWidget(page.pressureActive);
    optionsLayout->addWidget(page.impedanceActive);
    optionsLayout->addWidget(page.summaryActive);
    optionsLayout->addWidget(page.scalarSummaryActive);
    optionsLayout->addWidget(page.impedanceSummaryActive);
    optionsLayout->addWidget(page.invertPhase);
    optionsLayout->addWidget(page.fullCircuit);
    optionsLayout->addStretch(1);

    auto *leftGroup = new QGroupBox(tr("Thiele/Small parameters"), page.page);
    leftGroup->setLayout(leftForm);
    auto *boxGroup = new QGroupBox(tr("Enclosure and gain"), page.page);
    boxGroup->setLayout(boxForm);
    auto *optionsGroup = new QGroupBox(tr("Curves and totals"), page.page);
    optionsGroup->setLayout(optionsLayout);

    auto *grid = new QGridLayout(page.page);
    grid->addWidget(leftGroup, 0, 0, 2, 1);
    grid->addWidget(boxGroup, 0, 1);
    grid->addWidget(optionsGroup, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    connect(page.vb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, [this, index](double) { updateTubeLengthForPage(m_pages.at(index)); });
    connect(page.fb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, [this, index](double) { updateTubeLengthForPage(m_pages.at(index)); });
    connect(page.tubeDiameter, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, [this, index](double) { updateTubeLengthForPage(m_pages.at(index)); });

    connectPreviewSignals(page);

    return page.page;
}

void DriverParametersDialog::connectPreviewSignals(const DriverPage& page)
{
    auto connectSpinBox = [this](QDoubleSpinBox *spinBox) {
        connect(spinBox, &QDoubleSpinBox::textChanged,
                this, &DriverParametersDialog::schedulePreview);
        connect(spinBox,
                static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this,
                [this](double) { schedulePreview(); });
    };

    connect(page.title, &QLineEdit::textEdited,
            this, &DriverParametersDialog::schedulePreview);

    connectSpinBox(page.rdc);
    connectSpinBox(page.lspMilliHenry);
    connectSpinBox(page.f0);
    connectSpinBox(page.qts);
    connectSpinBox(page.qes);
    connectSpinBox(page.qms);
    connectSpinBox(page.vas);
    connectSpinBox(page.dm);
    connectSpinBox(page.vb);
    connectSpinBox(page.ql);
    connectSpinBox(page.fb);
    connectSpinBox(page.tubeDiameter);
    connectSpinBox(page.v2);
    connectSpinBox(page.gainDb);

    connect(page.alignmentProposal,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { schedulePreview(); });

    connect(page.pressureActive, &QCheckBox::toggled,
            this, &DriverParametersDialog::schedulePreview);
    connect(page.impedanceActive, &QCheckBox::toggled,
            this, &DriverParametersDialog::schedulePreview);
    connect(page.summaryActive, &QCheckBox::toggled,
            this, &DriverParametersDialog::schedulePreview);
    connect(page.scalarSummaryActive, &QCheckBox::toggled,
            this, &DriverParametersDialog::schedulePreview);
    connect(page.impedanceSummaryActive, &QCheckBox::toggled,
            this, &DriverParametersDialog::schedulePreview);
    connect(page.invertPhase, &QCheckBox::toggled,
            this, &DriverParametersDialog::schedulePreview);
    connect(page.fullCircuit, &QCheckBox::toggled,
            this, &DriverParametersDialog::schedulePreview);
}

bool DriverParametersDialog::readSpinBoxValue(const QDoubleSpinBox *spinBox,
                                              const QString& label,
                                              int driverIndex,
                                              double& value,
                                              QString *errorMessage) const
{
    double parsedValue = 0.0;
    const bool ok = spinBox != nullptr &&
                    parseSpinBoxText(spinBox,
                                     spinBox->text(),
                                     parsedValue,
                                     spinBox->minimum(),
                                     spinBox->maximum()) &&
                    std::isfinite(parsedValue);

    if (!ok) {
        if (errorMessage != nullptr && errorMessage->isEmpty()) {
            *errorMessage = tr("Driver %1: invalid value for %2.")
                                .arg(driverIndex + 1)
                                .arg(label);
        }
        return false;
    }

    value = parsedValue;
    return true;
}

void DriverParametersDialog::loadFromDrivers()
{
    m_loadingFromDrivers = true;

    QSettings settings;

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        DriverPage& page = m_pages.at(index);
        driver& drv = m_drivers[index];

        page.title->setText(drv.GetTitle());
        page.rdc->setValue(drv.getRdc());
        page.lspMilliHenry->setValue(drv.getLsp() * 1000.0);
        page.f0->setValue(drv.getF0());
        page.qts->setValue(drv.getQtc());
        page.qes->setValue(drv.getQes());
        page.qms->setValue(drv.getQms());
        page.vas->setValue(drv.getVas());
        page.dm->setValue(drv.getDm());
        page.vb->setValue(drv.Vb);
        page.ql->setValue(drv.getQl());
        page.fb->setValue(drv.Fb);
        page.tubeDiameter->setValue(settings.value(tubeDiameterSettingsKey(index), 0.0).toDouble());
        page.v2->setValue(drv.V2);
        page.gainDb->setValue(linearGainToDb(drv.gain));

        const int alignmentIndex = page.alignmentProposal->findData(drv.GTypProposal);
        page.alignmentProposal->setCurrentIndex(alignmentIndex >= 0 ? alignmentIndex : 0);

        page.pressureActive->setChecked(drv.PressureisActive);
        page.impedanceActive->setChecked(drv.ImpedanzisActive);
        page.summaryActive->setChecked(drv.SummaryisActive);
        page.scalarSummaryActive->setChecked(drv.ScalarSummaryisActive);
        page.impedanceSummaryActive->setChecked(drv.ImpedanzSummaryisActive);
        page.invertPhase->setChecked(drv.InvertPhase);
        page.fullCircuit->setChecked(drv.getFullCircuit());
        updateTubeLengthForPage(page);
    }

    m_loadingFromDrivers = false;
}

bool DriverParametersDialog::applyToDrivers(ApplyMode mode, QString *errorMessage)
{
    struct ParsedPage
    {
        QString title;
        double rdc = 0.0;
        double lspMilliHenry = 0.0;
        double f0 = 0.0;
        double qts = 0.0;
        double qes = 0.0;
        double qms = 0.0;
        double vas = 0.0;
        double dm = 0.0;
        double vb = 0.0;
        double ql = 0.0;
        double fb = 0.0;
        double tubeDiameterCm = 0.0;
        double v2 = 0.0;
        int alignmentProposal = 0;
        double gainDb = 0.0;
        bool pressureActive = false;
        bool impedanceActive = false;
        bool summaryActive = false;
        bool scalarSummaryActive = false;
        bool impedanceSummaryActive = false;
        bool invertPhase = false;
        bool fullCircuit = false;
    };

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    std::array<ParsedPage, KFilterProjectIo::DriverCount> parsedPages;

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        const DriverPage& page = m_pages.at(index);
        ParsedPage& parsed = parsedPages.at(index);

        if (!readSpinBoxValue(page.rdc, tr("Rdc"), index, parsed.rdc, errorMessage) ||
            !readSpinBoxValue(page.lspMilliHenry, tr("Lsp"), index, parsed.lspMilliHenry, errorMessage) ||
            !readSpinBoxValue(page.f0, tr("Fs"), index, parsed.f0, errorMessage) ||
            !readSpinBoxValue(page.qts, tr("Qts"), index, parsed.qts, errorMessage) ||
            !readSpinBoxValue(page.qes, tr("Qes"), index, parsed.qes, errorMessage) ||
            !readSpinBoxValue(page.qms, tr("Qms"), index, parsed.qms, errorMessage) ||
            !readSpinBoxValue(page.vas, tr("Vas"), index, parsed.vas, errorMessage) ||
            !readSpinBoxValue(page.dm, tr("Diameter"), index, parsed.dm, errorMessage) ||
            !readSpinBoxValue(page.vb, tr("Vb"), index, parsed.vb, errorMessage) ||
            !readSpinBoxValue(page.ql, tr("Ql"), index, parsed.ql, errorMessage) ||
            !readSpinBoxValue(page.fb, tr("Fb"), index, parsed.fb, errorMessage) ||
            !readSpinBoxValue(page.tubeDiameter, tr("Tube diameter"), index, parsed.tubeDiameterCm, errorMessage) ||
            !readSpinBoxValue(page.v2, tr("V2"), index, parsed.v2, errorMessage) ||
            !readSpinBoxValue(page.gainDb, tr("Gain"), index, parsed.gainDb, errorMessage)) {
            return false;
        }

        parsed.title = page.title->text();
        parsed.alignmentProposal = page.alignmentProposal->currentData().toInt();
        parsed.pressureActive = page.pressureActive->isChecked();
        parsed.impedanceActive = page.impedanceActive->isChecked();
        parsed.summaryActive = page.summaryActive->isChecked();
        parsed.scalarSummaryActive = page.scalarSummaryActive->isChecked();
        parsed.impedanceSummaryActive = page.impedanceSummaryActive->isChecked();
        parsed.invertPhase = page.invertPhase->isChecked();
        parsed.fullCircuit = page.fullCircuit->isChecked();
    }

    QSettings settings;

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        const ParsedPage& parsed = parsedPages.at(index);
        driver& drv = m_drivers[index];

        drv.SetTitle(parsed.title);
        drv.setRdc(parsed.rdc);
        drv.setLsp(parsed.lspMilliHenry / 1000.0);
        drv.setF0(parsed.f0);
        drv.setQtc(parsed.qts);
        drv.setQes(parsed.qes);
        drv.setQms(parsed.qms);
        drv.setVas(parsed.vas);
        drv.setDm(parsed.dm);
        drv.Vb = parsed.vb;
        drv.setQl(parsed.ql);
        drv.Fb = parsed.fb;
        drv.V2 = parsed.v2;
        drv.GTypProposal = parsed.alignmentProposal;
        drv.gain = dbToLinearGain(parsed.gainDb);

        if (mode == ApplyMode::Commit) {
            if (parsed.tubeDiameterCm > 0.0) {
                settings.setValue(tubeDiameterSettingsKey(index), parsed.tubeDiameterCm);
            } else {
                settings.remove(tubeDiameterSettingsKey(index));
            }
        }

        drv.PressureisActive = parsed.pressureActive;
        drv.ImpedanzisActive = parsed.impedanceActive;
        drv.SummaryisActive = parsed.summaryActive;
        drv.ScalarSummaryisActive = parsed.scalarSummaryActive;
        drv.ImpedanzSummaryisActive = parsed.impedanceSummaryActive;
        drv.InvertPhase = parsed.invertPhase;
        drv.setFullCircuit(parsed.fullCircuit);

        drv.Berechneparameter();
        drv.setmodified();
    }

    return true;
}

void DriverParametersDialog::rememberCommittedDrivers()
{
    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        m_committedDrivers[index] = m_drivers[index];
    }
}

void DriverParametersDialog::restoreCommittedDrivers()
{
    m_restoringDrivers = true;
    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        m_drivers[index] = m_committedDrivers[index];
        m_drivers[index].setmodified();
    }
    m_restoringDrivers = false;
}

void DriverParametersDialog::schedulePreview()
{
    if (m_previewTimer == nullptr || m_loadingFromDrivers || m_restoringDrivers) {
        return;
    }

    m_previewTimer->start(150);
}

void DriverParametersDialog::emitPreview()
{
    if (m_loadingFromDrivers || m_restoringDrivers) {
        return;
    }

    if (applyToDrivers(ApplyMode::Preview)) {
        emit parametersPreviewed();
    }
}

void DriverParametersDialog::applyClicked()
{
    if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }

    QString errorMessage;
    if (!applyToDrivers(ApplyMode::Commit, &errorMessage)) {
        QMessageBox::warning(this, tr("Driver parameters"), errorMessage);
        return;
    }

    rememberCommittedDrivers();
    emit parametersApplied();
}

void DriverParametersDialog::accept()
{
    if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }

    QString errorMessage;
    if (!applyToDrivers(ApplyMode::Commit, &errorMessage)) {
        QMessageBox::warning(this, tr("Driver parameters"), errorMessage);
        return;
    }

    rememberCommittedDrivers();
    emit parametersApplied();
    QDialog::accept();
}

void DriverParametersDialog::reject()
{
    if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }

    restoreCommittedDrivers();
    emit parametersRestored();
    QDialog::reject();
}

void DriverParametersDialog::updateQtsForPage(DriverPage& page)
{
    const double qes = page.qes->value();
    const double qms = page.qms->value();
    const double denominator = qes + qms;
    if (denominator > 0.0) {
        page.qts->setValue((qes * qms) / denominator);
    }
}

void DriverParametersDialog::updateTubeLengthForPage(DriverPage& page)
{
    const double lengthCm = calculateBassReflexTubeLengthCm(page.tubeDiameter->value(),
                                                            page.fb->value(),
                                                            page.vb->value());
    if (lengthCm > 0.0 && std::isfinite(lengthCm)) {
        page.tubeLength->setText(tr("%1 cm").arg(lengthCm, 0, 'f', 1));
    } else {
        page.tubeLength->setText(tr("n. a."));
    }
}

void DriverParametersDialog::calculateHogeForPage(DriverPage& page)
{
    const double qts = page.qts->value();
    const double vasLiter = page.vas->value();
    const double fsHz = page.f0->value();

    if (qts >= 0.8) {
        QMessageBox::warning(this,
                             tr("Hoge calculation"),
                             tr("The legacy Hoge formula is intended for bass reflex alignments with Qts < 0.8."));
        return;
    }

    if (qts <= 0.0 || vasLiter <= 0.0 || fsHz <= 0.0 ||
        !std::isfinite(qts) || !std::isfinite(vasLiter) || !std::isfinite(fsHz)) {
        QMessageBox::warning(this,
                             tr("Hoge calculation"),
                             tr("The Hoge calculation requires positive Fs, Qts, and Vas values."));
        return;
    }

    const double vbLiter = 15.0 * vasLiter * std::pow(qts, 2.87);
    const double fbHz = 0.42 * fsHz / std::pow(qts, 0.9);

    if (!std::isfinite(vbLiter) || !std::isfinite(fbHz)) {
        QMessageBox::warning(this,
                             tr("Hoge calculation"),
                             tr("The Hoge calculation did not produce finite Vb and Fb values."));
        return;
    }

    page.vb->setValue(vbLiter);
    page.fb->setValue(fbHz);
    updateTubeLengthForPage(page);
}

