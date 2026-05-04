/***************************************************************************
                          circuitout.h  -  Qt6 circuit preview widget
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#ifndef CIRCUITOUT_H
#define CIRCUITOUT_H

#include <QColor>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <array>

class driver;

/**
 * Qt6 bring-up replacement for the legacy pixmap based CircuitOut widget.
 *
 * The original KDE3 version placed many QLabel/QPixmap fragments into a
 * QScrollView.  This version draws the same 8-section network model directly
 * with QPainter.  It is intentionally a read-only preview for now; editing
 * remains in NetworkParametersDialog until the port is further along.
 */
class CircuitOut : public QWidget
{
    Q_OBJECT

public:
    explicit CircuitOut(QWidget *parent = nullptr);
    ~CircuitOut() override = default;

    void setDriver(driver& drv, int driverNumber);
    void setDrivers(driver drivers[], int driverCount);
    void setvalues(double network[]);
    void setShowValues(bool showValues);
    static QColor defaultBackgroundColor();
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& color);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    enum NetworkRow
    {
        SeriesR = 0,
        SeriesC = 1,
        SeriesL = 2,
        ShuntR = 3,
        ShuntC = 4,
        ShuntL = 5,
        RowsPerSection = 6,
        SectionCount = 8,
        NetworkUnitCount = 48
    };

    struct DriverSnapshot
    {
        std::array<double, NetworkUnitCount + 1> network{};
        QString title;
        int driverNumber = 1;
        int boxTypeProposal = 0;
        double vb = 0.0;
        double fb = 0.0;
        double v2 = 0.0;
        bool active = false;
        bool valid = false;
    };

    static DriverSnapshot snapshotFromDriver(driver& drv, int driverNumber);
    void applySnapshot(const DriverSnapshot& snapshot);
    void applyPreviewGeometry();
    void drawCurrentDriverPreview(QPainter& painter, const QRect& previewRect) const;
    void drawNoActiveDriversMessage(QPainter& painter, const QRect& messageRect) const;

    bool useLightInk() const;
    QColor panelBorderColor() const;
    QColor primaryInkColor() const;
    QColor secondaryInkColor() const;
    QColor guideInkColor() const;
    QColor placeholderInkColor() const;
    QColor enclosureOutlineColor() const;

    double unit(int section, NetworkRow row) const;
    bool sectionHasSeriesElement(int section) const;
    bool sectionHasShuntElement(int section) const;
    bool hasAnyNetworkElement() const;

    QString valueText(double value, NetworkRow row) const;
    void drawComponentValue(QPainter& painter, const QRectF& rect, const QString& text) const;

    void drawSection(QPainter& painter, int section, int x0, int x1, int signalY, int returnY) const;
    void drawSeriesPath(QPainter& painter, int section, int x0, int x1, int signalY) const;
    void drawSeriesRlElement(QPainter& painter, int section, int x0, int x1, int signalY) const;
    void drawSeriesSuckCircuit(QPainter& painter, int section, int x0, int x1, int signalY) const;
    void drawShuntTrapBranch(QPainter& painter, int section, int branchX, int signalY, int returnY) const;
    void drawResistor(QPainter& painter, const QRectF& rect, Qt::Orientation orientation) const;
    void drawCapacitor(QPainter& painter, const QRectF& rect, Qt::Orientation orientation) const;
    void drawInductor(QPainter& painter, const QRectF& rect, Qt::Orientation orientation) const;
    void drawAcSource(QPainter& painter, qreal centerX, qreal signalY, qreal returnY) const;
    void drawDriver(QPainter& painter, const QRectF& rect, int signalY, int returnY) const;
    void drawEquivalentDriverCircuit(QPainter& painter, const QRectF& rect, int signalY, int returnY) const;
    void drawPort(QPainter& painter, const QRectF& rect, bool leftFacing) const;
    void drawSpeakerSymbol(QPainter& painter, const QRectF& rect) const;
    void drawBoxParameterText(QPainter& painter, const QRectF& boxRect, int boxType) const;
    void drawGround(QPainter& painter, QPointF center) const;

    std::array<DriverSnapshot, 4> m_driverSnapshots{};
    int m_driverSnapshotCount = 0;
    bool m_showAllDrivers = false;

    std::array<double, NetworkUnitCount + 1> m_network{}; // 1-based, Unit[0] kept unused like driver::Unit.
    QString m_driverTitle;
    int m_driverNumber = 1;
    int m_boxTypeProposal = 0;
    double m_vb = 0.0;
    double m_fb = 0.0;
    double m_v2 = 0.0;
    bool m_showValues = true;
    QColor m_backgroundColor = defaultBackgroundColor();
};

#endif // CIRCUITOUT_H
