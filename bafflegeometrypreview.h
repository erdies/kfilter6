/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLEGEOMETRYPREVIEW_H
#define BAFFLEGEOMETRYPREVIEW_H

#include <QWidget>

/**
 * Lightweight explanatory preview for the Baffle / Diffraction dialog.
 *
 * The widget is deliberately presentation-only: it mirrors the four geometry
 * values already edited by the dialog and never participates in validation,
 * persistence or DSP calculations.
 */
class BaffleGeometryPreview : public QWidget
{
public:
    enum class Highlight
    {
        None,
        BaffleWidth,
        BaffleHeight,
        DriverX,
        DriverY
    };

    explicit BaffleGeometryPreview(QWidget *parent = nullptr);

    void setHighlight(Highlight highlight);
    Highlight currentHighlight() const noexcept;

    void setGeometryValues(double widthMm,
                           double heightMm,
                           double driverXmm,
                           double driverYmm);

    double baffleWidthMm() const noexcept;
    double baffleHeightMm() const noexcept;
    double driverXmm() const noexcept;
    double driverYmm() const noexcept;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Highlight m_highlight = Highlight::None;
    double m_widthMm = 200.0;
    double m_heightMm = 0.0;
    double m_driverXmm = 0.0;
    double m_driverYmm = 0.0;
};

#endif // BAFFLEGEOMETRYPREVIEW_H
