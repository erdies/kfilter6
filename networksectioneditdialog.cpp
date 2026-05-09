/***************************************************************************
                          networksectioneditdialog.cpp  -  Qt6 section editor
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "networksectioneditdialog.h"

#include "networkvalueutils.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QObject>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QValidator>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>


namespace
{
class SectionValueEdit : public QDoubleSpinBox
{
public:
    explicit SectionValueEdit(double value, QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setRange(0.0, std::numeric_limits<double>::max());
        setDecimals(10);
        setSingleStep(1.0);
        setAccelerated(false);
        setKeyboardTracking(true);
        setCorrectionMode(QAbstractSpinBox::CorrectToPreviousValue);
        setAlignment(Qt::AlignRight);
        setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        setFixedWidth(125);
        setToolTip(QObject::tr("Type a value directly, or use mouse wheel / Up/Down. Shift = coarse 10%, Ctrl = fine 0.2%, default = 2%."));
        lineEdit()->setClearButtonEnabled(true);
        lineEdit()->setPlaceholderText(QStringLiteral("0"));
        setValue(std::isfinite(value) && value >= 0.0 ? value : 0.0);
    }

    QString rawText() const
    {
        return lineEdit()->text().trimmed();
    }

protected:
    QString textFromValue(double value) const override
    {
        return NetworkValueUtils::displayNumber(value, 10);
    }

    double valueFromText(const QString& text) const override
    {
        double parsed = 0.0;
        if (NetworkValueUtils::parseNonNegativeDisplayValue(text, parsed)) {
            return parsed;
        }
        return value();
    }

    QValidator::State validate(QString& input, int& pos) const override
    {
        Q_UNUSED(pos);

        const QString trimmed = input.trimmed();
        if (trimmed.isEmpty()) {
            return QValidator::Acceptable;
        }
        if (trimmed == QLatin1String(".") || trimmed == QLatin1String(",")) {
            return QValidator::Intermediate;
        }
        if (trimmed.endsWith(QLatin1Char('.')) || trimmed.endsWith(QLatin1Char(','))) {
            double prefix = 0.0;
            const QString prefixText = trimmed.left(trimmed.size() - 1);
            if (NetworkValueUtils::parseNonNegativeDisplayValue(prefixText, prefix)) {
                return QValidator::Intermediate;
            }
        }

        double parsed = 0.0;
        if (NetworkValueUtils::parseNonNegativeDisplayValue(trimmed, parsed) && parsed <= maximum()) {
            return QValidator::Acceptable;
        }
        return QValidator::Invalid;
    }

    void stepBy(int steps) override
    {
        double current = 0.0;
        if (!NetworkValueUtils::parseNonNegativeDisplayValue(rawText(), current)) {
            current = value();
        }

        const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
        const double stepRatio = relativeStepRatio(modifiers);
        double adjusted = current;
        if (current == 0.0) {
            adjusted = steps > 0
                ? zeroStartStep(modifiers) * std::pow(1.0 + stepRatio, static_cast<double>(steps - 1))
                : 0.0;
        } else {
            adjusted = current * std::pow(1.0 + stepRatio, static_cast<double>(steps));
        }

        if (!std::isfinite(adjusted) || adjusted < 0.0) {
            return;
        }

        setValue(adjusted);
        lineEdit()->selectAll();
    }

private:
    static double relativeStepRatio(Qt::KeyboardModifiers modifiers)
    {
        if (modifiers.testFlag(Qt::ShiftModifier)) {
            return 0.10;
        }
        if (modifiers.testFlag(Qt::ControlModifier)) {
            return 0.002;
        }
        return 0.02;
    }

    static double zeroStartStep(Qt::KeyboardModifiers modifiers)
    {
        if (modifiers.testFlag(Qt::ShiftModifier)) {
            return 0.10;
        }
        if (modifiers.testFlag(Qt::ControlModifier)) {
            return 0.002;
        }
        return 0.01;
    }
};

QString currentFieldText(const QDoubleSpinBox *field)
{
    if (field == nullptr) {
        return QString();
    }

    // SectionValueEdit::rawText() reads the live editor contents.  QDoubleSpinBox::cleanText()
    // can lag behind while the user is still typing, so it is not suitable for immediate
    // slider activation from text edits.
    return static_cast<const SectionValueEdit *>(field)->rawText();
}
}

NetworkSectionEditDialog::NetworkSectionEditDialog(int driverIndex,
                                                   int sectionIndex,
                                                   const QString& groupName,
                                                   const Values& initialValues,
                                                   QWidget *parent)
    : QDialog(parent),
      m_values(initialValues),
      m_originalValues(initialValues),
      m_resistanceSliderBase(initialValues.resistanceOhm),
      m_capacitanceSliderBase(initialValues.capacitanceMicroFarad),
      m_inductanceSliderBase(initialValues.inductanceMilliHenry)
{
    setWindowTitle(tr("Edit %1 Section").arg(groupName));

    auto *mainLayout = new QVBoxLayout(this);
    auto *infoLabel = new QLabel(tr("Driver %1, Section %2, %3 R/C/L")
                                     .arg(driverIndex + 1)
                                     .arg(sectionIndex + 1)
                                     .arg(groupName),
                                 this);
    mainLayout->addWidget(infoLabel);

    auto *grid = new QGridLayout();
    m_resistanceEdit = new SectionValueEdit(initialValues.resistanceOhm, this);
    m_capacitanceEdit = new SectionValueEdit(initialValues.capacitanceMicroFarad, this);
    m_inductanceEdit = new SectionValueEdit(initialValues.inductanceMilliHenry, this);

    const auto configureSlider = [this](QSlider *slider, double baseValue) {
        slider->setOrientation(Qt::Horizontal);
        slider->setRange(-100, 100);
        slider->setValue(0);
        slider->setSingleStep(1);
        slider->setPageStep(10);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(50);
        slider->setMinimumWidth(180);

        setVariationSliderEnabled(slider, std::isfinite(baseValue) && baseValue > 0.0);
    };

    m_resistanceSlider = new QSlider(this);
    m_capacitanceSlider = new QSlider(this);
    m_inductanceSlider = new QSlider(this);
    configureSlider(m_resistanceSlider, initialValues.resistanceOhm);
    configureSlider(m_capacitanceSlider, initialValues.capacitanceMicroFarad);
    configureSlider(m_inductanceSlider, initialValues.inductanceMilliHenry);

    grid->addWidget(new QLabel(tr("R [Ohm]:"), this), 0, 0);
    grid->addWidget(m_resistanceEdit, 0, 1);
    grid->addWidget(m_resistanceSlider, 0, 2);
    grid->addWidget(new QLabel(tr("C [uF]:"), this), 1, 0);
    grid->addWidget(m_capacitanceEdit, 1, 1);
    grid->addWidget(m_capacitanceSlider, 1, 2);
    grid->addWidget(new QLabel(tr("L [mH]:"), this), 2, 0);
    grid->addWidget(m_inductanceEdit, 2, 1);
    grid->addWidget(m_inductanceSlider, 2, 2);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 1);
    mainLayout->addLayout(grid);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setWordWrap(true);
    mainLayout->addWidget(m_validationLabel);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &NetworkSectionEditDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &NetworkSectionEditDialog::reject);
    mainLayout->addWidget(m_buttonBox);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(150);
    connect(m_previewTimer, &QTimer::timeout, this, &NetworkSectionEditDialog::emitPreviewValues);

    connect(m_resistanceEdit, &QDoubleSpinBox::textChanged, this, &NetworkSectionEditDialog::handleInputChanged);
    connect(m_capacitanceEdit, &QDoubleSpinBox::textChanged, this, &NetworkSectionEditDialog::handleInputChanged);
    connect(m_inductanceEdit, &QDoubleSpinBox::textChanged, this, &NetworkSectionEditDialog::handleInputChanged);
    connect(m_resistanceEdit,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double) { handleInputChanged(); });
    connect(m_capacitanceEdit,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double) { handleInputChanged(); });
    connect(m_inductanceEdit,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double) { handleInputChanged(); });

    connect(m_resistanceSlider, &QSlider::valueChanged, this, [this](int position) {
        handleVariationSliderChanged(m_resistanceSlider,
                                     m_resistanceEdit,
                                     m_resistanceSliderBase,
                                     position);
    });
    connect(m_capacitanceSlider, &QSlider::valueChanged, this, [this](int position) {
        handleVariationSliderChanged(m_capacitanceSlider,
                                     m_capacitanceEdit,
                                     m_capacitanceSliderBase,
                                     position);
    });
    connect(m_inductanceSlider, &QSlider::valueChanged, this, [this](int position) {
        handleVariationSliderChanged(m_inductanceSlider,
                                     m_inductanceEdit,
                                     m_inductanceSliderBase,
                                     position);
    });

    validateInput();

    m_resistanceEdit->selectAll();
    m_resistanceEdit->setFocus(Qt::OtherFocusReason);
    resize(560, 210);
}

NetworkSectionEditDialog::Values NetworkSectionEditDialog::values() const
{
    return m_values;
}

NetworkSectionEditDialog::Values NetworkSectionEditDialog::originalValues() const
{
    return m_originalValues;
}

bool NetworkSectionEditDialog::currentValues(Values *values, QString *errorMessage) const
{
    Values parsedValues;
    if (!readField(m_resistanceEdit, tr("R [Ohm]"), parsedValues.resistanceOhm, errorMessage) ||
        !readField(m_capacitanceEdit, tr("C [uF]"), parsedValues.capacitanceMicroFarad, errorMessage) ||
        !readField(m_inductanceEdit, tr("L [mH]"), parsedValues.inductanceMilliHenry, errorMessage)) {
        return false;
    }

    if (values != nullptr) {
        *values = parsedValues;
    }
    return true;
}

void NetworkSectionEditDialog::accept()
{
    if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }

    QString errorMessage;
    Values parsedValues;
    if (!currentValues(&parsedValues, &errorMessage)) {
        QMessageBox::warning(this, tr("Edit Network Section"), errorMessage);
        return;
    }

    m_values = parsedValues;
    QDialog::accept();
}

void NetworkSectionEditDialog::reject()
{
    if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }

    QDialog::reject();
}

void NetworkSectionEditDialog::handleInputChanged()
{
    if (!m_updatingVariationControls) {
        syncSlidersFromFields();
    }

    validateInput();

    Values parsedValues;
    if (currentValues(&parsedValues)) {
        scheduleTextPreview();
    } else if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }
}

void NetworkSectionEditDialog::schedulePreview(int intervalMilliseconds, bool restartActiveTimer)
{
    if (m_previewTimer == nullptr) {
        return;
    }

    if (restartActiveTimer ||
        !m_previewTimer->isActive() ||
        m_previewTimer->remainingTime() > intervalMilliseconds) {
        m_previewTimer->start(intervalMilliseconds);
    }
}

void NetworkSectionEditDialog::scheduleTextPreview()
{
    schedulePreview(150, true);
}

void NetworkSectionEditDialog::scheduleSliderPreview()
{
    schedulePreview(40, false);
}

void NetworkSectionEditDialog::validateInput()
{
    QString errorMessage;
    const bool valid = validateFields(&errorMessage);
    if (m_buttonBox != nullptr && m_buttonBox->button(QDialogButtonBox::Ok) != nullptr) {
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
    }

    if (m_validationLabel != nullptr) {
        m_validationLabel->setText(valid ? NetworkValueUtils::validationHint()
                                         : errorMessage);
    }
}

void NetworkSectionEditDialog::emitPreviewValues()
{
    Values parsedValues;
    if (currentValues(&parsedValues)) {
        emit previewValuesChanged(parsedValues);
    }
}

void NetworkSectionEditDialog::handleVariationSliderChanged(QSlider *slider,
                                                            QDoubleSpinBox *field,
                                                            double baseValue,
                                                            int position)
{
    if (slider == nullptr || field == nullptr || !slider->isEnabled()) {
        return;
    }

    const double adjustedValue = valueFromSliderPosition(baseValue, position);
    if (!std::isfinite(adjustedValue) || adjustedValue < 0.0) {
        return;
    }

    m_updatingVariationControls = true;
    {
        const QSignalBlocker fieldBlocker(field);
        field->setValue(adjustedValue);
    }
    field->selectAll();
    m_updatingVariationControls = false;

    validateInput();

    Values parsedValues;
    if (currentValues(&parsedValues)) {
        scheduleSliderPreview();
    } else if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }
}

void NetworkSectionEditDialog::syncSlidersFromFields()
{
    syncSliderFromField(m_resistanceSlider, m_resistanceEdit, m_resistanceSliderBase);
    syncSliderFromField(m_capacitanceSlider, m_capacitanceEdit, m_capacitanceSliderBase);
    syncSliderFromField(m_inductanceSlider, m_inductanceEdit, m_inductanceSliderBase);
}

void NetworkSectionEditDialog::syncSliderFromField(QSlider *slider,
                                                   QDoubleSpinBox *field,
                                                   double& baseValue)
{
    if (slider == nullptr || field == nullptr) {
        return;
    }

    double parsedValue = 0.0;
    const QString text = currentFieldText(field);
    if (!NetworkValueUtils::parseNonNegativeDisplayValue(text, parsedValue)) {
        return;
    }

    const bool valueIsUsable = std::isfinite(parsedValue) && parsedValue > 0.0;
    if (!valueIsUsable) {
        baseValue = 0.0;
        const QSignalBlocker blocker(slider);
        slider->setValue(0);
        setVariationSliderEnabled(slider, false);
        return;
    }

    // Manual field edits define a new relative slider center immediately.
    // This also activates sliders whose section value was 0 when the dialog opened.
    baseValue = parsedValue;
    const QSignalBlocker blocker(slider);
    slider->setValue(0);
    setVariationSliderEnabled(slider, true);
}

void NetworkSectionEditDialog::setVariationSliderEnabled(QSlider *slider, bool enabled)
{
    if (slider == nullptr) {
        return;
    }

    slider->setEnabled(enabled);
    slider->setToolTip(enabled
        ? tr("Relative variation slider. Center = current field value; left = half, right = double.")
        : tr("Relative variation slider is disabled until the value is greater than 0."));
}

double NetworkSectionEditDialog::valueFromSliderPosition(double baseValue, int position)
{
    if (!std::isfinite(baseValue) || baseValue <= 0.0) {
        return 0.0;
    }

    return baseValue * std::pow(2.0, static_cast<double>(position) / 100.0);
}

int NetworkSectionEditDialog::sliderPositionFromValue(double baseValue, double value)
{
    if (!std::isfinite(baseValue) || baseValue <= 0.0 || !std::isfinite(value) || value <= 0.0) {
        return -100;
    }

    const double rawPosition = 100.0 * std::log(value / baseValue) / std::log(2.0);
    if (!std::isfinite(rawPosition)) {
        return 0;
    }

    const int roundedPosition = static_cast<int>(std::lround(rawPosition));
    return std::max(-100, std::min(100, roundedPosition));
}

bool NetworkSectionEditDialog::readField(QDoubleSpinBox *field,
                                         const QString& label,
                                         double& value,
                                         QString *errorMessage) const
{
    const QString text = currentFieldText(field);
    if (NetworkValueUtils::parseNonNegativeDisplayValue(text, value)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = NetworkValueUtils::validationError(label);
    }
    return false;
}

bool NetworkSectionEditDialog::validateFields(QString *errorMessage) const
{
    return currentValues(nullptr, errorMessage);
}
