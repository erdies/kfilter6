/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "measurementimportdialog.h"

#include <QCheckBox>
#include <QColor>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <utility>

class MeasurementImportPreview final : public QWidget
{
public:
    explicit MeasurementImportPreview(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(640, 350);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const QVector<KFilterImportedMeasurementPoint>& rawPoints,
                 const KFilterCorrectionImportResult& importResult,
                 const KFilterCorrectionImportSettings& settings)
    {
        m_rawPoints = rawPoints;
        m_importResult = importResult;
        m_settings = settings;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), palette().color(QPalette::Base));

        if (m_rawPoints.size() < 2) {
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(rect(), Qt::AlignCenter, tr("No measurement preview available."));
            return;
        }

        const int leftMargin = 58;
        const int rightMargin = 20;
        const int topMargin = 24;
        const int bottomMargin = 30;
        const int panelGap = 42;
        const int availableHeight = height() - topMargin - bottomMargin - panelGap;
        if (width() <= leftMargin + rightMargin + 20 || availableHeight < 100) {
            return;
        }

        const int rawHeight = availableHeight * 2 / 5;
        const QRectF rawRect(leftMargin, topMargin,
                             width() - leftMargin - rightMargin, rawHeight);
        const QRectF relativeRect(leftMargin,
                                  topMargin + rawHeight + panelGap,
                                  width() - leftMargin - rightMargin,
                                  availableHeight - rawHeight);

        const double frequencyMinHz = m_rawPoints.constFirst().frequencyHz;
        const double frequencyMaxHz = m_rawPoints.constLast().frequencyHz;
        const double logMin = std::log10(frequencyMinHz);
        const double logMax = std::log10(frequencyMaxHz);
        if (!std::isfinite(logMin) || !std::isfinite(logMax) || logMax <= logMin) {
            return;
        }

        auto xForFrequency = [&](double frequencyHz, const QRectF& plotRect) {
            const double fraction = (std::log10(frequencyHz) - logMin) / (logMax - logMin);
            return plotRect.left() + (fraction * plotRect.width());
        };

        auto levelRange = [](const auto& points, auto valueGetter, double includeValue) {
            double minimum = includeValue;
            double maximum = includeValue;
            for (const auto& point : points) {
                const double value = valueGetter(point);
                if (std::isfinite(value)) {
                    minimum = std::min(minimum, value);
                    maximum = std::max(maximum, value);
                }
            }
            if (maximum - minimum < 6.0) {
                const double center = (minimum + maximum) / 2.0;
                minimum = center - 3.0;
                maximum = center + 3.0;
            } else {
                const double padding = (maximum - minimum) * 0.08;
                minimum -= padding;
                maximum += padding;
            }
            return std::pair<double, double>(minimum, maximum);
        };

        const auto rawRange = levelRange(
            m_rawPoints,
            [](const KFilterImportedMeasurementPoint& point) { return point.levelDb; },
            m_rawPoints.constFirst().levelDb);

        QVector<KFilterMeasurementPoint> relativePoints = m_importResult.calibratedMeasurement;
        for (const KFilterMeasurementPoint& point : m_importResult.correctionCurve.points()) {
            relativePoints.append(point);
        }
        const auto relativeRange = levelRange(
            relativePoints,
            [](const KFilterMeasurementPoint& point) { return point.value; },
            0.0);

        const QColor textColor = palette().color(QPalette::Text);
        const QColor mutedColor = palette().color(QPalette::Mid);
        const QColor gridColor = palette().color(QPalette::Midlight);
        const QColor highlightColor = palette().color(QPalette::Highlight);
        QColor calibrationBand = highlightColor;
        calibrationBand.setAlpha(32);
        QColor correctionBand = QColor(80, 150, 90);
        correctionBand.setAlpha(32);

        auto drawBand = [&](const QRectF& plotRect, double startHz, double endHz, const QColor& color) {
            if (startHz <= 0.0 || endHz <= startHz) {
                return;
            }
            const double left = xForFrequency(startHz, plotRect);
            const double right = xForFrequency(endHz, plotRect);
            painter.fillRect(QRectF(left, plotRect.top(), right - left, plotRect.height()), color);
        };

        drawBand(rawRect, m_settings.calibrationMinHz, m_settings.calibrationMaxHz,
                 calibrationBand);
        drawBand(relativeRect, m_settings.correctionMinHz, m_settings.correctionMaxHz,
                 correctionBand);

        auto drawFrameAndGrid = [&](const QRectF& plotRect,
                                    const std::pair<double, double>& range,
                                    const QString& title) {
            painter.setPen(QPen(gridColor, 1.0));
            for (int index = 0; index <= 4; ++index) {
                const double y = plotRect.top() + (plotRect.height() * index / 4.0);
                painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
                const double value = range.second -
                                     ((range.second - range.first) * index / 4.0);
                painter.setPen(textColor);
                painter.drawText(QRectF(0.0, y - 9.0, leftMargin - 7.0, 18.0),
                                 Qt::AlignRight | Qt::AlignVCenter,
                                 QString::number(value, 'f', 1));
                painter.setPen(QPen(gridColor, 1.0));
            }
            painter.setPen(QPen(mutedColor, 1.0));
            painter.drawRect(plotRect);
            painter.setPen(textColor);
            painter.drawText(QRectF(plotRect.left(), plotRect.top() - 21.0,
                                    plotRect.width(), 18.0),
                             Qt::AlignLeft | Qt::AlignVCenter, title);
        };

        drawFrameAndGrid(rawRect, rawRange, tr("Raw measurement (absolute dB)"));
        drawFrameAndGrid(relativeRect, relativeRange,
                         tr("Calibrated measurement and imported correction (relative dB)"));

        auto yForValue = [](double value, const QRectF& plotRect,
                            const std::pair<double, double>& range) {
            const double fraction = (range.second - value) / (range.second - range.first);
            return plotRect.top() + (fraction * plotRect.height());
        };

        auto drawPath = [&](const auto& points, auto frequencyGetter, auto valueGetter,
                            const QRectF& plotRect,
                            const std::pair<double, double>& range,
                            const QPen& pen) {
            if (points.size() < 2) {
                return;
            }
            QPainterPath path;
            bool started = false;
            for (const auto& point : points) {
                const double frequencyHz = frequencyGetter(point);
                const double value = valueGetter(point);
                if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0 ||
                    !std::isfinite(value)) {
                    continue;
                }
                const QPointF plotPoint(xForFrequency(frequencyHz, plotRect),
                                        yForValue(value, plotRect, range));
                if (!started) {
                    path.moveTo(plotPoint);
                    started = true;
                } else {
                    path.lineTo(plotPoint);
                }
            }
            painter.save();
            painter.setClipRect(plotRect.adjusted(-1.0, -1.0, 1.0, 1.0));
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
            painter.restore();
        };

        drawPath(m_rawPoints,
                 [](const KFilterImportedMeasurementPoint& point) { return point.frequencyHz; },
                 [](const KFilterImportedMeasurementPoint& point) { return point.levelDb; },
                 rawRect, rawRange, QPen(QColor(105, 105, 105), 1.6));

        drawPath(m_importResult.calibratedMeasurement,
                 [](const KFilterMeasurementPoint& point) { return point.frequencyHz; },
                 [](const KFilterMeasurementPoint& point) { return point.value; },
                 relativeRect, relativeRange, QPen(QColor(45, 105, 180), 1.5));

        drawPath(m_importResult.correctionCurve.points(),
                 [](const KFilterMeasurementPoint& point) { return point.frequencyHz; },
                 [](const KFilterMeasurementPoint& point) { return point.value; },
                 relativeRect, relativeRange, QPen(QColor(190, 65, 55), 2.2));

        if (relativeRange.first <= 0.0 && relativeRange.second >= 0.0) {
            const double zeroY = yForValue(0.0, relativeRect, relativeRange);
            painter.setPen(QPen(textColor, 1.0, Qt::DashLine));
            painter.drawLine(QPointF(relativeRect.left(), zeroY),
                             QPointF(relativeRect.right(), zeroY));
        }

        const int firstDecade = static_cast<int>(std::floor(logMin));
        const int lastDecade = static_cast<int>(std::ceil(logMax));
        painter.setPen(textColor);
        for (int decade = firstDecade; decade <= lastDecade; ++decade) {
            const double frequencyHz = std::pow(10.0, decade);
            if (frequencyHz < frequencyMinHz || frequencyHz > frequencyMaxHz) {
                continue;
            }
            const double x = xForFrequency(frequencyHz, relativeRect);
            painter.drawLine(QPointF(x, relativeRect.bottom()),
                             QPointF(x, relativeRect.bottom() + 4.0));
            const QString label = frequencyHz >= 1000.0
                                      ? tr("%1 kHz").arg(frequencyHz / 1000.0, 0, 'g', 3)
                                      : tr("%1 Hz").arg(frequencyHz, 0, 'g', 4);
            painter.drawText(QRectF(x - 38.0, relativeRect.bottom() + 5.0, 76.0, 18.0),
                             Qt::AlignHCenter | Qt::AlignTop, label);
        }

        painter.setPen(QColor(45, 105, 180));
        painter.drawText(QRectF(relativeRect.right() - 250.0,
                                relativeRect.top() - 21.0, 118.0, 18.0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         tr("Calibrated"));
        painter.setPen(QColor(190, 65, 55));
        painter.drawText(QRectF(relativeRect.right() - 126.0,
                                relativeRect.top() - 21.0, 126.0, 18.0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         tr("Imported correction"));
    }

private:
    QVector<KFilterImportedMeasurementPoint> m_rawPoints;
    KFilterCorrectionImportResult m_importResult;
    KFilterCorrectionImportSettings m_settings;
};

namespace
{
QDoubleSpinBox *createFrequencySpinBox(double minimumHz, double maximumHz, QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setDecimals(3);
    spin->setRange(minimumHz, maximumHz);
    spin->setSuffix(QObject::tr(" Hz"));
    spin->setKeyboardTracking(false);
    spin->setSingleStep(std::max(1.0, (maximumHz - minimumHz) / 200.0));
    return spin;
}

QDoubleSpinBox *createOffsetSpinBox(QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setDecimals(3);
    spin->setRange(-200.0, 200.0);
    spin->setSuffix(QObject::tr(" dB"));
    spin->setKeyboardTracking(false);
    spin->setSingleStep(0.1);
    return spin;
}

QDoubleSpinBox *createFadeSpinBox(QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setDecimals(3);
    spin->setRange(0.01, 4.0);
    spin->setSuffix(QObject::tr(" oct"));
    spin->setKeyboardTracking(false);
    spin->setSingleStep(1.0 / 12.0);
    spin->setValue(1.0 / 3.0);
    return spin;
}

QString formatDb(double value)
{
    return QObject::tr("%1 dB").arg(value, 0, 'f', 3);
}
}

MeasurementImportDialog::MeasurementImportDialog(
    const QString& sourceFileName,
    const QString& driverLabel,
    const KFilterMeasurementParseResult& parseResult,
    QWidget *parent)
    : QDialog(parent)
    , m_parseResult(parseResult)
{
    setWindowTitle(tr("Import Measurement for %1").arg(driverLabel));
    setModal(true);
    resize(820, 780);

    const double sourceMinHz = m_parseResult.points.constFirst().frequencyHz;
    const double sourceMaxHz = m_parseResult.points.constLast().frequencyHz;
    double sourceMinDb = m_parseResult.points.constFirst().levelDb;
    double sourceMaxDb = sourceMinDb;
    for (const KFilterImportedMeasurementPoint& point : m_parseResult.points) {
        sourceMinDb = std::min(sourceMinDb, point.levelDb);
        sourceMaxDb = std::max(sourceMaxDb, point.levelDb);
    }
    const qsizetype count = m_parseResult.points.size();
    const qsizetype calibrationStartIndex = std::min(count - 2, std::max<qsizetype>(0, count / 5));
    const qsizetype calibrationEndIndex =
        std::min(count - 1, std::max(calibrationStartIndex + 1, (count * 2) / 5));
    const qsizetype correctionStartIndex =
        std::min(count - 2, std::max(calibrationEndIndex, count / 3));

    auto *mainLayout = new QVBoxLayout(this);

    auto *introLabel = new QLabel(
        tr("Calibrate the arbitrary absolute measurement level to a 0 dB reference, "
           "then import only the frequency window in which the simulated driver model "
           "should be supplemented. Measurement phase is ignored."),
        this);
    introLabel->setWordWrap(true);
    mainLayout->addWidget(introLabel);

    auto *sourceGroup = new QGroupBox(tr("Source"), this);
    auto *sourceLayout = new QFormLayout(sourceGroup);
    auto *sourceFileLabel = new QLabel(QFileInfo(sourceFileName).fileName(), sourceGroup);
    sourceFileLabel->setToolTip(sourceFileName);
    sourceLayout->addRow(tr("File:"), sourceFileLabel);
    sourceLayout->addRow(tr("Valid points:"),
                         new QLabel(QString::number(static_cast<qlonglong>(m_parseResult.points.size())), sourceGroup));
    sourceLayout->addRow(tr("Frequency range:"),
                         new QLabel(tr("%1 Hz – %2 Hz")
                                        .arg(sourceMinHz, 0, 'g', 8)
                                        .arg(sourceMaxHz, 0, 'g', 8),
                                    sourceGroup));
    sourceLayout->addRow(tr("Level range:"),
                         new QLabel(tr("%1 dB – %2 dB")
                                        .arg(sourceMinDb, 0, 'f', 2)
                                        .arg(sourceMaxDb, 0, 'f', 2),
                                    sourceGroup));
    mainLayout->addWidget(sourceGroup);

    auto *settingsLayout = new QHBoxLayout;

    auto *calibrationGroup = new QGroupBox(tr("0 dB Calibration"), this);
    auto *calibrationLayout = new QFormLayout(calibrationGroup);
    m_calibrationMinSpin = createFrequencySpinBox(sourceMinHz, sourceMaxHz, calibrationGroup);
    m_calibrationMaxSpin = createFrequencySpinBox(sourceMinHz, sourceMaxHz, calibrationGroup);
    m_manualOffsetSpin = createOffsetSpinBox(calibrationGroup);
    m_referenceMedianValue = new QLabel(calibrationGroup);
    m_automaticOffsetValue = new QLabel(calibrationGroup);
    m_effectiveOffsetValue = new QLabel(calibrationGroup);
    calibrationLayout->addRow(tr("From:"), m_calibrationMinSpin);
    calibrationLayout->addRow(tr("To:"), m_calibrationMaxSpin);
    calibrationLayout->addRow(tr("Reference median:"), m_referenceMedianValue);
    calibrationLayout->addRow(tr("Automatic offset:"), m_automaticOffsetValue);
    calibrationLayout->addRow(tr("Manual offset:"), m_manualOffsetSpin);
    calibrationLayout->addRow(tr("Effective offset:"), m_effectiveOffsetValue);
    settingsLayout->addWidget(calibrationGroup, 1);

    auto *windowGroup = new QGroupBox(tr("Correction Window"), this);
    auto *windowLayout = new QFormLayout(windowGroup);
    m_correctionMinSpin = createFrequencySpinBox(sourceMinHz, sourceMaxHz, windowGroup);
    m_correctionMaxSpin = createFrequencySpinBox(sourceMinHz, sourceMaxHz, windowGroup);
    m_lowerFadeCheck = new QCheckBox(tr("Enable lower fade"), windowGroup);
    m_lowerFadeSpin = createFadeSpinBox(windowGroup);
    m_upperFadeCheck = new QCheckBox(tr("Enable upper fade"), windowGroup);
    m_upperFadeSpin = createFadeSpinBox(windowGroup);
    m_lowerFadeCheck->setChecked(true);
    m_upperFadeCheck->setChecked(false);
    m_upperFadeSpin->setEnabled(false);
    windowLayout->addRow(tr("Import from:"), m_correctionMinSpin);
    windowLayout->addRow(tr("Import to:"), m_correctionMaxSpin);
    windowLayout->addRow(m_lowerFadeCheck);
    windowLayout->addRow(tr("Lower fade width:"), m_lowerFadeSpin);
    windowLayout->addRow(m_upperFadeCheck);
    windowLayout->addRow(tr("Upper fade width:"), m_upperFadeSpin);
    settingsLayout->addWidget(windowGroup, 1);

    mainLayout->addLayout(settingsLayout);

    m_calibrationMinSpin->setValue(m_parseResult.points.at(calibrationStartIndex).frequencyHz);
    m_calibrationMaxSpin->setValue(m_parseResult.points.at(calibrationEndIndex).frequencyHz);
    m_correctionMinSpin->setValue(m_parseResult.points.at(correctionStartIndex).frequencyHz);
    m_correctionMaxSpin->setValue(sourceMaxHz);

    const bool defaultLowerFadeFits =
        m_correctionMinSpin->value() * std::pow(2.0, m_lowerFadeSpin->value()) <
        m_correctionMaxSpin->value();
    m_lowerFadeCheck->setChecked(defaultLowerFadeFits);
    m_lowerFadeSpin->setEnabled(defaultLowerFadeFits);

    m_preview = new MeasurementImportPreview(this);
    mainLayout->addWidget(m_preview, 1);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setWordWrap(true);
    mainLayout->addWidget(m_validationLabel);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setWordWrap(true);
    mainLayout->addWidget(m_warningLabel);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);

    const auto update = [this]() { updateImportResult(); };
    connect(m_calibrationMinSpin, &QDoubleSpinBox::valueChanged, this, update);
    connect(m_calibrationMaxSpin, &QDoubleSpinBox::valueChanged, this, update);
    connect(m_manualOffsetSpin, &QDoubleSpinBox::valueChanged, this, update);
    connect(m_correctionMinSpin, &QDoubleSpinBox::valueChanged, this, update);
    connect(m_correctionMaxSpin, &QDoubleSpinBox::valueChanged, this, update);
    connect(m_lowerFadeCheck, &QCheckBox::toggled, this, [this, update](bool enabled) {
        m_lowerFadeSpin->setEnabled(enabled);
        update();
    });
    connect(m_lowerFadeSpin, &QDoubleSpinBox::valueChanged, this, update);
    connect(m_upperFadeCheck, &QCheckBox::toggled, this, [this, update](bool enabled) {
        m_upperFadeSpin->setEnabled(enabled);
        update();
    });
    connect(m_upperFadeSpin, &QDoubleSpinBox::valueChanged, this, update);

    updateImportResult();
}

KFilterMeasurementCurve MeasurementImportDialog::correctionCurve() const
{
    return m_importResult.correctionCurve;
}

KFilterCorrectionImportSettings MeasurementImportDialog::importSettings() const
{
    return settingsFromUi();
}

KFilterCorrectionImportResult MeasurementImportDialog::importResult() const
{
    return m_importResult;
}

KFilterCorrectionImportSettings MeasurementImportDialog::settingsFromUi() const
{
    KFilterCorrectionImportSettings settings;
    settings.calibrationMinHz = m_calibrationMinSpin->value();
    settings.calibrationMaxHz = m_calibrationMaxSpin->value();
    settings.manualOffsetDb = m_manualOffsetSpin->value();
    settings.correctionMinHz = m_correctionMinSpin->value();
    settings.correctionMaxHz = m_correctionMaxSpin->value();
    settings.lowerFadeEnabled = m_lowerFadeCheck->isChecked();
    settings.lowerFadeOctaves = m_lowerFadeSpin->value();
    settings.upperFadeEnabled = m_upperFadeCheck->isChecked();
    settings.upperFadeOctaves = m_upperFadeSpin->value();
    return settings;
}

void MeasurementImportDialog::updateImportResult()
{
    const KFilterCorrectionImportSettings settings = settingsFromUi();
    m_importResult = createKFilterCorrectionCurve(m_parseResult.points, settings);

    if (m_importResult.isValid()) {
        m_referenceMedianValue->setText(formatDb(m_importResult.referenceMedianDb));
        m_automaticOffsetValue->setText(formatDb(m_importResult.automaticOffsetDb));
        m_effectiveOffsetValue->setText(formatDb(m_importResult.effectiveOffsetDb));
        m_validationLabel->setText(
            tr("Ready to import %1 correction points. Outside the selected window the correction remains neutral.")
                .arg(static_cast<qlonglong>(m_importResult.correctionCurve.size())));
    } else {
        m_referenceMedianValue->setText(tr("—"));
        m_automaticOffsetValue->setText(tr("—"));
        m_effectiveOffsetValue->setText(tr("—"));
        m_validationLabel->setText(m_importResult.errorMessage);
    }

    QStringList warnings = m_parseResult.warnings;
    for (const QString& warning : m_importResult.warnings) {
        warnings.append(warning);
    }
    m_warningLabel->setVisible(!warnings.isEmpty());
    m_warningLabel->setText(warnings.isEmpty()
                                ? QString()
                                : tr("Notes: %1").arg(warnings.join(QLatin1Char(' '))));

    if (m_buttonBox != nullptr) {
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_importResult.isValid());
    }
    if (m_preview != nullptr) {
        m_preview->setData(m_parseResult.points, m_importResult, settings);
    }
}
