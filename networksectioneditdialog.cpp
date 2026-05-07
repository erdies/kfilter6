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
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>


namespace
{
void configureNumericEdit(QLineEdit *edit)
{
    edit->setAlignment(Qt::AlignRight);
    edit->setClearButtonEnabled(true);
    edit->setPlaceholderText(QStringLiteral("0"));
}
}

NetworkSectionEditDialog::NetworkSectionEditDialog(int driverIndex,
                                                   int sectionIndex,
                                                   const QString& groupName,
                                                   const Values& initialValues,
                                                   QWidget *parent)
    : QDialog(parent),
      m_values(initialValues)
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
    m_resistanceEdit = new QLineEdit(displayNumber(initialValues.resistanceOhm), this);
    m_capacitanceEdit = new QLineEdit(displayNumber(initialValues.capacitanceMicroFarad), this);
    m_inductanceEdit = new QLineEdit(displayNumber(initialValues.inductanceMilliHenry), this);
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

    connect(m_resistanceEdit, &QLineEdit::textChanged, this, &NetworkSectionEditDialog::validateInput);
    connect(m_capacitanceEdit, &QLineEdit::textChanged, this, &NetworkSectionEditDialog::validateInput);
    connect(m_inductanceEdit, &QLineEdit::textChanged, this, &NetworkSectionEditDialog::validateInput);
    validateInput();

    m_resistanceEdit->selectAll();
    m_resistanceEdit->setFocus(Qt::OtherFocusReason);
    resize(380, 190);
}

NetworkSectionEditDialog::Values NetworkSectionEditDialog::values() const
{
    return m_values;
}

void NetworkSectionEditDialog::accept()
{
    QString errorMessage;
    if (!validateFields(&errorMessage)) {
        QMessageBox::warning(this, tr("Edit Network Section"), errorMessage);
        return;
    }

    Values parsedValues;
    readField(m_resistanceEdit, tr("R [Ohm]"), parsedValues.resistanceOhm);
    readField(m_capacitanceEdit, tr("C [uF]"), parsedValues.capacitanceMicroFarad);
    readField(m_inductanceEdit, tr("L [mH]"), parsedValues.inductanceMilliHenry);

    m_values = parsedValues;
    QDialog::accept();
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
    double unused = 0.0;
    return readField(m_resistanceEdit, tr("R [Ohm]"), unused, errorMessage) &&
           readField(m_capacitanceEdit, tr("C [uF]"), unused, errorMessage) &&
           readField(m_inductanceEdit, tr("L [mH]"), unused, errorMessage);
}
