/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef PLOTCOLORSDIALOG_H
#define PLOTCOLORSDIALOG_H

#include "kfilterview.h"

#include <QDialog>
#include <QString>
#include <QVector>

class QGridLayout;
class QPushButton;

/**
 * Dialog for editing application-wide plot color preferences.
 *
 * These colors are intentionally not part of the KFilter project file format.
 * They are user interface preferences and are stored through QSettings by the
 * main window.
 */
class PlotColorsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlotColorsDialog(const KFilterView::PlotColorSettings& initialSettings,
                              QWidget *parent = nullptr);

    KFilterView::PlotColorSettings settings() const;

private:
    struct ColorRow
    {
        QPushButton *button = nullptr;
        QColor *color = nullptr;
        QString title;
    };

    void addColorRow(QGridLayout *grid,
                     int row,
                     const QString& label,
                     QColor& color);
    void chooseColor(ColorRow& row);
    void resetToDefaults();
    void updateColorButtons();
    static void setColorButtonAppearance(QPushButton *button, const QColor& color);

    KFilterView::PlotColorSettings m_settings;
    QVector<ColorRow> m_colorRows;
};

#endif // PLOTCOLORSDIALOG_H
