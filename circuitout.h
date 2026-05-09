/***************************************************************************
                          circuitout.h  -  Qt6 circuit preview widget
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#ifndef CIRCUITOUT_H
#define CIRCUITOUT_H

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

#include <array>

class QContextMenuEvent;
class driver;

/**
 * Qt6 bring-up replacement for the legacy pixmap based CircuitOut widget.
 *
 * The original KDE3 version placed many QLabel/QPixmap fragments into a
 * QScrollView.  This version draws the same 8-section network model directly
 * with QPainter and exposes targeted editing entry points for the ported
 * parameter dialogs.
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

    enum class NetworkHitGroup
    {
        SeriesSection = 0,
        ShuntSection = 1
    };
    Q_ENUM(NetworkHitGroup)

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void networkSectionClicked(int driverIndex, int sectionIndex, NetworkHitGroup group);
    void networkSectionContextMenuRequested(int driverIndex, int sectionIndex, NetworkHitGroup group, const QPoint& globalPosition);
    void networkSectionHovered(int driverIndex, int sectionIndex, NetworkHitGroup group);
    void networkSectionHoverLeft();
    void driverClicked(int driverIndex);
    void driverHovered(int driverIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

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

    struct NetworkSectionHit
    {
        QRectF bounds;
        int driverIndex = 0;
        int sectionIndex = 0;
        NetworkHitGroup group = NetworkHitGroup::SeriesSection;
    };

    struct DriverHit
    {
        QRectF bounds;
        int driverIndex = 0;
    };

    enum class HoverHitKind
    {
        None = 0,
        NetworkSection,
        Driver
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
        bool curveOrTotalFlagActive = false;
        bool active = false;
        bool valid = false;
    };

    static DriverSnapshot snapshotFromDriver(driver& drv, int driverNumber);
    void applySnapshot(const DriverSnapshot& snapshot);
    void applyPreviewGeometry();
    void registerSectionHit(int section, NetworkHitGroup group, const QRectF& bounds) const;
    void registerDriverHit(const QRectF& bounds) const;
    bool findSectionHit(const QPoint& position, NetworkSectionHit& hit) const;
    bool findDriverHit(const QPoint& position, DriverHit& hit) const;
    bool sameSectionHit(const NetworkSectionHit& lhs, const NetworkSectionHit& rhs) const;
    bool sameDriverHit(const DriverHit& lhs, const DriverHit& rhs) const;
    void updateHoverHit(const QPoint& position);
    void clearHoverHit();
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
    void drawEquivalentDriverCircuit(QPainter& painter, const QRectF& rect, int signalY, int returnY, qreal shuntStartX = -1.0) const;
    void drawDriverActivityLamp(QPainter& painter, const QRectF& rect, bool active) const;
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
    bool m_curveOrTotalFlagActive = false;
    bool m_showValues = true;
    QColor m_backgroundColor = defaultBackgroundColor();
    mutable QVector<NetworkSectionHit> m_sectionHits;
    mutable QVector<DriverHit> m_driverHits;
    NetworkSectionHit m_hoverSectionHit;
    DriverHit m_hoverDriverHit;
    HoverHitKind m_hoverHitKind = HoverHitKind::None;
};

#endif // CIRCUITOUT_H
