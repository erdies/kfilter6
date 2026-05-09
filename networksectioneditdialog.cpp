/***************************************************************************
                          networksectioneditdialog.cpp  -  Qt6 section editor
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "networksectioneditdialog.h"

#include "networkvalueutils.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QObject>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>


namespace
{
class AdjustableNetworkValueEdit : public QLineEdit
{
public:
    explicit AdjustableNetworkValueEdit(const QString& text, QWidget *parent = nullptr)
        : QLineEdit(text, parent)
    {
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        const QPoint angleDelta = event == nullptr ? QPoint() : event->angleDelta();
        if (angleDelta.y() == 0) {
            QLineEdit::wheelEvent(event);
            return;
        }

        int steps = angleDelta.y() / 120;
        if (steps == 0) {
            steps = angleDelta.y() > 0 ? 1 : -1;
        }

        if (adjustBySteps(steps, event->modifiers())) {
            event->accept();
            return;
        }

        QLineEdit::wheelEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event != nullptr && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
            const int steps = event->key() == Qt::Key_Up ? 1 : -1;
            if (adjustBySteps(steps, event->modifiers())) {
                event->accept();
                return;
            }
        }

        QLineEdit::keyPressEvent(event);
    }

private:
    bool adjustBySteps(int steps, Qt::KeyboardModifiers modifiers)
    {
        double value = 0.0;
        if (!NetworkValueUtils::parseNonNegativeDisplayValue(text(), value)) {
            return false;
        }

        const double stepRatio = relativeStepRatio(modifiers);
        double adjusted = value;
        if (value == 0.0) {
            adjusted = steps > 0
                ? zeroStartStep(modifiers) * std::pow(1.0 + stepRatio, static_cast<double>(steps - 1))
                : 0.0;
        } else {
            adjusted = value * std::pow(1.0 + stepRatio, static_cast<double>(steps));
        }

        if (!std::isfinite(adjusted) || adjusted < 0.0) {
            return false;
        }

        setText(NetworkValueUtils::displayNumber(adjusted, 10));
        selectAll();
        return true;
    }

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

void configureNumericEdit(QLineEdit *edit)
{
    edit->setAlignment(Qt::AlignRight);
    edit->setClearButtonEnabled(true);
    edit->setPlaceholderText(QStringLiteral("0"));
    edit->setToolTip(QObject::tr("Use mouse wheel or Up/Down to adjust. Shift = coarse, Ctrl = fine."));
}
}

NetworkSectionEditDialog::NetworkSectionEditDialog(int driverIndex,
                                                   int sectionIndex,
                                                   const QString& groupName,
                                                   const Values& initialValues,
                                                   QWidget *parent)
    : QDialog(parent),
      m_values(initialValues),
      m_originalValues(initialValues)
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
    m_resistanceEdit = new AdjustableNetworkValueEdit(displayNumber(initialValues.resistanceOhm), this);
    m_capacitanceEdit = new AdjustableNetworkValueEdit(displayNumber(initialValues.capacitanceMicroFarad), this);
    m_inductanceEdit = new AdjustableNetworkValueEdit(displayNumber(initialValues.inductanceMilliHenry), this);
    configureNumericEdit(m_resistanceEdit);
    configureNumericEdit(m_capacitanceEdit);
    configureNumericEdit(m_inductanceEdit);

    grid->addWidget(new QLabel(tr("R [Ohm]:"), this), 0, 0);
    grid->addWidget(m_resistanceEdit, 0, 1);
    grid->addWidget(new QLabel(tr("C [uF]:"), this), 1, 0);
    grid->addWidget(m_capacitanceEdit, 1, 1);
    grid->addWidget(new QLabel(tr("L [mH]:"), this), 2, 0);
    grid->addWidget(m_inductanceEdit, 2, 1);
    grid->setColumnStretch(1, 1);
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

    connect(m_resistanceEdit, &QLineEdit::textChanged, this, &NetworkSectionEditDialog::handleInputChanged);
    connect(m_capacitanceEdit, &QLineEdit::textChanged, this, &NetworkSectionEditDialog::handleInputChanged);
    connect(m_inductanceEdit, &QLineEdit::textChanged, this, &NetworkSectionEditDialog::handleInputChanged);
    validateInput();

    m_resistanceEdit->selectAll();
    m_resistanceEdit->setFocus(Qt::OtherFocusReason);
    resize(380, 190);
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
    validateInput();

    Values parsedValues;
    if (currentValues(&parsedValues)) {
        if (m_previewTimer != nullptr) {
            m_previewTimer->start();
        }
    } else if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }
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

QString NetworkSectionEditDialog::displayNumber(double value)
{
    return NetworkValueUtils::displayNumber(value, 10);
}

bool NetworkSectionEditDialog::readField(QLineEdit *field,
                                         const QString& label,
                                         double& value,
                                         QString *errorMessage) const
{
    const QString text = field == nullptr ? QString() : field->text().trimmed();
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
