/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef BAFFLEGEOMETRYPREVIEW_H
#define BAFFLEGEOMETRYPREVIEW_H

#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <functional>

class QEvent;
class QMouseEvent;

/**
 * Lightweight geometry preview/input surface for the Baffle / Diffraction
 * dialog.
 *
 * The widget mirrors values already edited by the dialog. It may report a
 * directly dragged driver-centre position back to the dialog, but it never
 * participates in validation, persistence or DSP calculations itself.
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
        DriverY,
        LeftChamfer,
        RightChamfer
    };

    using DriverPositionCallback = std::function<void(double xMm, double yMm)>;

    explicit BaffleGeometryPreview(QWidget *parent = nullptr);

    void setHighlight(Highlight highlight);
    Highlight currentHighlight() const noexcept;

    void setGeometryValues(double widthMm,
                           double heightMm,
                           double driverXmm,
                           double driverYmm,
                           double leftChamferSetbackMm = 0.0,
                           double rightChamferSetbackMm = 0.0);

    void setDriverPositionCallbacks(DriverPositionCallback previewCallback,
                                    DriverPositionCallback commitCallback);
    void setDriverDragEnabled(bool enabled);
    bool driverDragEnabled() const noexcept;

    double baffleWidthMm() const noexcept;
    double baffleHeightMm() const noexcept;
    double driverXmm() const noexcept;
    double driverYmm() const noexcept;
    double leftChamferSetbackMm() const noexcept;
    double rightChamferSetbackMm() const noexcept;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF currentBaffleRect() const;
    QPointF currentDriverCentre(const QRectF& baffle) const;
    qreal currentDriverRadius(const QRectF& baffle) const;
    bool driverHitTest(const QPointF& position) const;
    bool updateDraggedDriverPosition(const QPointF& pointerPosition);
    void updateHoverCursor(const QPointF& pointerPosition);

    Highlight m_highlight = Highlight::None;
    double m_widthMm = 200.0;
    double m_heightMm = 0.0;
    double m_driverXmm = 0.0;
    double m_driverYmm = 0.0;
    double m_leftChamferSetbackMm = 0.0;
    double m_rightChamferSetbackMm = 0.0;
    DriverPositionCallback m_driverPositionPreviewCallback;
    DriverPositionCallback m_driverPositionCommitCallback;
    bool m_driverDragEnabled = false;
    bool m_driverDragging = false;
    bool m_driverDragChanged = false;
    QPointF m_driverDragOffset;
    double m_driverDragStartXmm = 0.0;
    double m_driverDragStartYmm = 0.0;
};

#endif // BAFFLEGEOMETRYPREVIEW_H
