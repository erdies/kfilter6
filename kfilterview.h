/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERVIEW_H
#define KFILTERVIEW_H

#include <QColor>
#include <QPointF>
#include <QString>
#include <QWidget>

class KFilterDoc;
class QPaintEvent;
class QPainter;
class QPrinter;

/**
 * Qt6 porting version of the original KFilter plot view.
 *
 * The widget intentionally keeps the old drawing model for now: logarithmic
 * frequency grid, pressure curves, impedance curves and summary curves are
 * still calculated through KFilterDoc/driver. This patch only removes the
 * KDE3/Qt3 dependencies that prevented the view from being embedded in the
 * temporary Qt6 application shell.
 */
class KFilterView : public QWidget
{
    Q_OBJECT

public:
    explicit KFilterView(KFilterDoc *document, QWidget *parent = nullptr);
    ~KFilterView() override;

    KFilterDoc *getDocument() const;

    /** contains the implementation for printing functionality */
    void print(QPrinter *pPrinter);

    /** sets the gridcolor */
    void setGridColor(const QColor& color);
    /** gives back the gridcolor */
    QColor& gridColor();

    /** sets the pressurecolor */
    void setPressureColor(const QColor& color);
    /** gives back the pressurecolor */
    QColor& pressureColor();

    /** sets the pressurecolor */
    void setImpedanceColor(const QColor& color);
    /** gives back the pressurecolor */
    QColor& impedanceColor();

    /** sets the pressuresummarycolor */
    void setPressureSummaryColor(const QColor& color);
    /** gives back the pressuresummarycolor */
    QColor& pressureSummaryColor();

    /** sets the impedancesummarycolor */
    void setImpedanceSummaryColor(const QColor& color);
    /** gives back the impedancesummarycolor */
    QColor& impedanceSummaryColor();

    /** sets the scalarpressuresummarycolor */
    void setScalarPressureSummaryColor(const QColor& color);
    /** gives back the scalarpressuresummarycolor */
    QColor& scalarPressureSummaryColor();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    KFilterDoc *m_document = nullptr;

    QColor cgrid;
    QColor cpressure;
    QColor cimpedance;
    QColor cpressureS;
    QColor cimpedanceS;
    QColor cscalarpressureS;

    int m_Schall1 = 0;
    int m_Schall2 = 0;
    int m_Schall3 = 0;
    double Faktor = 1.047128548;
    double Start = 125.6637061;
    double Xvalue[150] = {};

    int YScale(double value, int type) const;  // type: pressure=0, impedance=1
    void initXvalue();
    int XK(double x) const;
    QColor pressureCurveColor(int driverIndex) const;
    QColor impedanceCurveColor(int driverIndex) const;
    struct CurveLabelAnchor {
        QPointF point;
        bool valid = false;
    };

    CurveLabelAnchor findLastVisibleCurvePoint(const double values[200], int type) const;
    void drawCurve(QPainter& painter, const double values[200], int type);
    void drawCurveLabel(QPainter& painter, const QPointF& point, const QString& label) const;
    void drawDriverCurveLabels(QPainter& painter);
    void drawLegend(QPainter& painter);
};

#endif // KFILTERVIEW_H
