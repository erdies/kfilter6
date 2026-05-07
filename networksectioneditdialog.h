/***************************************************************************
                          networksectioneditdialog.h  -  Qt6 section editor
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#ifndef NETWORKSECTIONEDITDIALOG_H
#define NETWORKSECTIONEDITDIALOG_H

#include <QDialog>
#include <QString>

class QDialogButtonBox;
class QLabel;
class QLineEdit;

/**
 * Small Qt6 editor for one logical network section group.
 *
 * The dialog intentionally edits one R/C/L triple only.  It mirrors the
 * NetworkParametersDialog display units: Ohm for R, uF for C and mH for L.
 */
class NetworkSectionEditDialog : public QDialog
{
    Q_OBJECT

public:
    struct Values
    {
        double resistanceOhm = 0.0;
        double capacitanceMicroFarad = 0.0;
        double inductanceMilliHenry = 0.0;
    };

    explicit NetworkSectionEditDialog(int driverIndex,
                                      int sectionIndex,
                                      const QString& groupName,
                                      const Values& initialValues,
                                      QWidget *parent = nullptr);

    Values values() const;

public slots:
    void accept() override;

private slots:
    void validateInput();

private:
    static QString displayNumber(double value);
    bool readField(QLineEdit *field,
                   const QString& label,
                   double& value,
                   QString *errorMessage = nullptr) const;
    bool validateFields(QString *errorMessage = nullptr) const;

    QLineEdit *m_resistanceEdit = nullptr;
    QLineEdit *m_capacitanceEdit = nullptr;
    QLineEdit *m_inductanceEdit = nullptr;
    QLabel *m_validationLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    Values m_values;
};

#endif // NETWORKSECTIONEDITDIALOG_H
