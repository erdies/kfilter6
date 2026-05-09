/***************************************************************************
                          driverparametersdialog.cpp  -  Qt6 driver editor
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "driverparametersdialog.h"

#include "driver.h"

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
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QTabWidget>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace
{
QDoubleSpinBox *createSpinBox(double minimum,
                              double maximum,
                              int decimals,
                              double singleStep,
                              const QString& suffix = QString())
{
    auto *spinBox = new QDoubleSpinBox;
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
    page.fb = createSpinBox(0.0, 100000.0, 3, 1.0, tr(" Hz"));
    page.tubeDiameter = createSpinBox(0.0, 1000.0, 3, 0.1, tr(" cm"));
    page.tubeLength = new QLineEdit(page.page);
    page.tubeLength->setReadOnly(true);
    page.tubeLength->setText(tr("n. a."));
    page.v2 = createSpinBox(0.0, 100000.0, 3, 1.0, tr(" l"));
    page.gainDb = createSpinBox(-60.0, 60.0, 2, 0.5, tr(" dB"));

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
    addRow(boxForm, tr("Vb"), page.vb);
    addRow(boxForm, tr("Fb"), page.fb);

    auto *tubeLayoutWidget = new QWidget(page.page);
    auto *tubeLayout = new QHBoxLayout(tubeLayoutWidget);
    tubeLayout->setContentsMargins(0, 0, 0, 0);
    tubeLayout->addWidget(page.tubeDiameter);
    tubeLayout->addWidget(new QLabel(tr("Tube length"), tubeLayoutWidget));
    tubeLayout->addWidget(page.tubeLength);
    addRow(boxForm, tr("Tube diameter"), tubeLayoutWidget);

    addRow(boxForm, tr("V2"), page.v2);
    addRow(boxForm, tr("Enclosure type"), page.alignmentProposal);
    addRow(boxForm, tr("Gain"), page.gainDb);

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

    return page.page;
}

void DriverParametersDialog::loadFromDrivers()
{
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
}

void DriverParametersDialog::applyToDrivers()
{
    QSettings settings;

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        DriverPage& page = m_pages.at(index);
        driver& drv = m_drivers[index];

        drv.SetTitle(page.title->text());
        drv.setRdc(page.rdc->value());
        drv.setLsp(page.lspMilliHenry->value() / 1000.0);
        drv.setF0(page.f0->value());
        drv.setQtc(page.qts->value());
        drv.setQes(page.qes->value());
        drv.setQms(page.qms->value());
        drv.setVas(page.vas->value());
        drv.setDm(page.dm->value());
        drv.Vb = page.vb->value();
        drv.Fb = page.fb->value();
        const double tubeDiameterCm = page.tubeDiameter->value();
        if (tubeDiameterCm > 0.0) {
            settings.setValue(tubeDiameterSettingsKey(index), tubeDiameterCm);
        } else {
            settings.remove(tubeDiameterSettingsKey(index));
        }
        drv.V2 = page.v2->value();
        drv.GTypProposal = page.alignmentProposal->currentData().toInt();
        drv.gain = dbToLinearGain(page.gainDb->value());

        drv.PressureisActive = page.pressureActive->isChecked();
        drv.ImpedanzisActive = page.impedanceActive->isChecked();
        drv.SummaryisActive = page.summaryActive->isChecked();
        drv.ScalarSummaryisActive = page.scalarSummaryActive->isChecked();
        drv.ImpedanzSummaryisActive = page.impedanceSummaryActive->isChecked();
        drv.InvertPhase = page.invertPhase->isChecked();
        drv.setFullCircuit(page.fullCircuit->isChecked());

        drv.Berechneparameter();
        drv.setmodified();
    }
}

void DriverParametersDialog::applyClicked()
{
    applyToDrivers();
    emit parametersApplied();
}

void DriverParametersDialog::accept()
{
    applyClicked();
    QDialog::accept();
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

void DriverParametersDialog::updateQtsFromQesQms()
{
    for (DriverPage& page : m_pages) {
        updateQtsForPage(page);
    }
}
