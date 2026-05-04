/***************************************************************************
                          networkparametersdialog.h  -  Qt6 network editor
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#ifndef NETWORKPARAMETERSDIALOG_H
#define NETWORKPARAMETERSDIALOG_H

#include "kfilterprojectio.h"

#include <QDialog>

#include <array>

class QTableWidget;
class QWidget;
class driver;

/**
 * Temporary Qt6 network/filter parameter dialog used during the KDE3 -> Qt6/KF6 port.
 *
 * The legacy NetworkDialog uses Qt3/KDE3 widgets and fixed pixmap based circuit previews.
 * This dialog keeps the same data model: 8 network sections per driver, each section
 * containing series/shunt R/C/L values. Capacitors are shown in uF and inductors in mH,
 * while the driver core continues to store them in F and H.
 */
class NetworkParametersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkParametersDialog(driver (&drivers)[KFilterProjectIo::DriverCount], QWidget *parent = nullptr);

signals:
    void parametersApplied();

public slots:
    void accept() override;

private slots:
    void applyClicked();
    void resetCurrentDriver();
    void resetAllDrivers();

private:
    struct DriverPage
    {
        QWidget *page = nullptr;
        QTableWidget *table = nullptr;
    };

    QWidget *createDriverPage(int index);
    void loadFromDrivers();
    bool applyToDrivers();
    void resetTable(QTableWidget *table);
    bool readCellValue(QTableWidget *table, int row, int column, double& value) const;
    void setCellValue(QTableWidget *table, int row, int column, double value) const;

    static int unitIndex(int row, int column);
    static double displayFromInternal(int row, double value);
    static double internalFromDisplay(int row, double value);

    driver (&m_drivers)[KFilterProjectIo::DriverCount];
    std::array<DriverPage, KFilterProjectIo::DriverCount> m_pages;
    int m_currentDriverIndex = 0;
};

#endif // NETWORKPARAMETERSDIALOG_H
