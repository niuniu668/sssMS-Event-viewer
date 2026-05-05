#include "waveformwidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QImage>
#include <QPixmap>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent) {
    setMinimumHeight(320);
    setMouseTracking(true);
}

void WaveformWidget::setData(const QVector<QVector<double>> &samples, const QStringList &channelNames) {
    m_samples = samples;
    m_channelNames = channelNames;
    m_plotMode = PlotMode::SingleFile;
    resetView();
    emit viewWindowChanged(visibleStartSample(), visibleEndSample());
    update();
}

void WaveformWidget::setStackedData(const QVector<QVector<double>> &traces, const QStringList &traceNames) {
    m_samples = traces;
    m_channelNames = traceNames;
    m_plotMode = PlotMode::Stacked;
    resetView();
    emit viewWindowChanged(visibleStartSample(), visibleEndSample());
    update();
}

void WaveformWidget::clearData() {
    m_samples.clear();
    m_channelNames.clear();
    m_pickMarkers.clear();
    m_assistCurve.clear();
    m_assistCurveName.clear();
    m_hasAssistRange = false;
    m_assistRangeStart = 0;
    m_assistRangeEnd = 0;
    m_plotMode = PlotMode::SingleFile;
    resetView();
    emit viewWindowChanged(0, 0);
    update();
}

void WaveformWidget::setPickMode(PickMode mode) {
    m_pickMode = mode;
    update();
}

WaveformWidget::PickMode WaveformWidget::pickMode() const {
    return m_pickMode;
}

void WaveformWidget::setPickMarkers(const QVector<PickMarker> &markers) {
    m_pickMarkers = markers;
    update();
}

QVector<WaveformWidget::PickMarker> WaveformWidget::pickMarkers() const {
    return m_pickMarkers;
}

void WaveformWidget::clearPickMarkers() {
    m_pickMarkers.clear();
    update();
}

int WaveformWidget::sampleIndexAt(const QPoint &pt) const {
    if (m_samples.isEmpty() || !pointInPlot(pt)) {
        return -1;
    }
    return std::clamp(static_cast<int>(std::round(pixelToSampleX(pt.x()))), 0, std::max(0, totalSamples() - 1));
}

void WaveformWidget::setAssistRangeStart(int sampleIndex) {
    if (m_samples.isEmpty()) {
        return;
    }
    const int idx = std::clamp(sampleIndex, 0, std::max(0, totalSamples() - 1));
    m_assistRangeStart = idx;
    if (!m_hasAssistRange) {
        m_assistRangeEnd = idx;
    }
    m_hasAssistRange = true;
    int s = 0;
    int e = 0;
    emit assistRangeChanged(m_assistRangeStart, m_assistRangeEnd, assistRange(s, e));
    update();
}

void WaveformWidget::setAssistRangeEnd(int sampleIndex) {
    if (m_samples.isEmpty()) {
        return;
    }
    const int idx = std::clamp(sampleIndex, 0, std::max(0, totalSamples() - 1));
    m_assistRangeEnd = idx;
    if (!m_hasAssistRange) {
        m_assistRangeStart = idx;
    }
    m_hasAssistRange = true;
    int s = 0;
    int e = 0;
    emit assistRangeChanged(m_assistRangeStart, m_assistRangeEnd, assistRange(s, e));
    update();
}

void WaveformWidget::clearAssistRange() {
    m_hasAssistRange = false;
    m_assistRangeStart = 0;
    m_assistRangeEnd = 0;
    emit assistRangeChanged(0, 0, false);
    update();
}

bool WaveformWidget::assistRange(int &startSample, int &endSample) const {
    if (!m_hasAssistRange || m_samples.isEmpty()) {
        return false;
    }
    startSample = std::min(m_assistRangeStart, m_assistRangeEnd);
    endSample = std::max(m_assistRangeStart, m_assistRangeEnd);
    return (endSample - startSample) >= 2;
}

void WaveformWidget::setAssistCurve(const QVector<double> &curve, const QString &name) {
    m_assistCurve = curve;
    m_assistCurveName = name;
    update();
}

void WaveformWidget::clearAssistCurve() {
    m_assistCurve.clear();
    m_assistCurveName.clear();
    update();
}

int WaveformWidget::visibleStartSample() const {
    if (m_samples.isEmpty()) {
        return 0;
    }
    return std::max(0, static_cast<int>(std::floor(m_offsetX)));
}

int WaveformWidget::visibleEndSample() const {
    if (m_samples.isEmpty()) {
        return 0;
    }
    const int n = totalSamples();
    const int end = static_cast<int>(std::ceil(m_offsetX + visibleSampleCount()));
    return std::clamp(end, 0, std::max(0, n - 1));
}

bool WaveformWidget::saveAsPng(const QString &filePath, int width, int height) {
    if (m_samples.isEmpty()) {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect plotRect = makePlotRect(width, height);
    painter.fillRect(plotRect, QColor(250, 250, 250));
    drawWaveforms(painter, plotRect);

    return image.save(filePath, "PNG");
}

void WaveformWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::white);

    if (m_samples.isEmpty()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "No waveform loaded");
        return;
    }

    const QRect plotRect = makePlotRect(width(), height());
    m_lastPlotRect = plotRect;

    painter.fillRect(plotRect, QColor(250, 250, 250));
    drawWaveforms(painter, plotRect);
    drawAssistOverlay(painter, plotRect);
    drawPickMarkers(painter, plotRect);

    painter.setPen(Qt::black);
    painter.drawRect(plotRect);
    painter.drawText(10, 18, QString("ZoomX: %1x  Offset: %2").arg(m_zoomX, 0, 'f', 2).arg(m_offsetX, 0, 'f', 1));
    QString hint;
    if (m_pickMode == PickMode::Navigate) {
        hint = "Left drag: box zoom   Right click: reset   Middle drag: pan   Wheel: zoom";
    } else if (m_pickMode == PickMode::PickP) {
        hint = "Pick mode: P phase (left click inside waveform)";
    } else if (m_pickMode == PickMode::PickS) {
        hint = "Pick mode: S phase (left click inside waveform)";
    } else {
        hint = "Erase mode: left click near a marker to remove it";
    }
    painter.drawText(10, height() - 10, hint);

    if (m_selecting) {
        const QRect selRect = QRect(m_selectStart, m_selectEnd).normalized() & plotRect;
        if (!selRect.isEmpty()) {
            painter.setPen(QPen(QColor(25, 118, 210), 1, Qt::DashLine));
            painter.setBrush(QColor(25, 118, 210, 40));
            painter.drawRect(selRect);
        }
    }

    if (m_hasHover) {
        painter.setPen(QColor(40, 40, 40));
        QString hover = QString("sample=%1   AMP=%2").arg(m_hoverSample).arg(m_hoverAmp, 0, 'g', 7);
        const QRect textRect(plotRect.right() - 320, plotRect.top() + 6, 310, 20);
        painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, hover);
    }
}

void WaveformWidget::drawWaveforms(QPainter &painter, const QRect &plotRect) {
    if (m_samples.isEmpty()) {
        return;
    }

    if (m_plotMode == PlotMode::Stacked) {
        drawStackedWaveforms(painter, plotRect);
    } else {
        drawSingleFileWaveforms(painter, plotRect);
    }
}

void WaveformWidget::drawSingleFileWaveforms(QPainter &painter, const QRect &plotRect) {
    const int n = m_samples.size();
    if (n <= 0 || m_samples[0].isEmpty()) {
        return;
    }

    const int c = m_samples[0].size();
    const QVector<QColor> colors = {QColor(31,119,180), QColor(255,127,14), QColor(44,160,44)};

    const double visible = visibleSampleCount();
    const int iStart = std::max(0, static_cast<int>(std::floor(m_offsetX)));
    const int iEnd = std::min(n - 1, static_cast<int>(std::ceil(m_offsetX + visible)));

    painter.setPen(QPen(QColor(160, 160, 160), 1));
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());

    // Calculate optimal tick count based on visible samples
    const int visibleSamples = iEnd - iStart + 1;
    int xTickCount = 6;
    if (visibleSamples <= 1000) {
        xTickCount = 5;
    } else if (visibleSamples <= 2000) {
        xTickCount = 6;
    } else if (visibleSamples <= 4000) {
        xTickCount = 8;
    } else {
        xTickCount = 10;
    }

    for (int t = 0; t <= xTickCount; ++t) {
        const double ratio = static_cast<double>(t) / xTickCount;
        const int x = plotRect.left() + static_cast<int>(ratio * plotRect.width());
        painter.drawLine(x, plotRect.bottom(), x, plotRect.bottom() + 4);
        const int sampleIdx = static_cast<int>(std::round(m_offsetX + ratio * visible));
        painter.drawText(x - 20, plotRect.bottom() + 18, 52, 14, Qt::AlignHCenter | Qt::AlignTop, QString::number(sampleIdx));
    }

    painter.setPen(QColor(70, 70, 70));
    painter.drawText(plotRect.left(), plotRect.bottom() + 32, "Sample Index");

    for (int ch = 0; ch < c; ++ch) {
        double minV = m_samples[iStart][ch];
        double maxV = m_samples[iStart][ch];
        for (int i = iStart + 1; i <= iEnd; ++i) {
            minV = std::min(minV, m_samples[i][ch]);
            maxV = std::max(maxV, m_samples[i][ch]);
        }

        const double range = (maxV - minV < 1e-12) ? 1.0 : (maxV - minV);
        const double bandTop = plotRect.top() + (static_cast<double>(ch) / c) * plotRect.height();
        const double bandHeight = static_cast<double>(plotRect.height()) / c;
        const double bandBottom = bandTop + bandHeight;

        painter.setPen(QPen(QColor(220, 220, 220), 1));
        painter.drawLine(plotRect.left(), static_cast<int>(bandTop), plotRect.right(), static_cast<int>(bandTop));
        painter.drawLine(plotRect.left(), static_cast<int>(bandBottom), plotRect.right(), static_cast<int>(bandBottom));

        painter.setPen(QPen(colors[ch % colors.size()], 1.0));

        bool started = false;
        QPointF prev;
        for (int i = iStart; i <= iEnd; ++i) {
            double xNorm = (static_cast<double>(i) - m_offsetX) / visible;
            double x = plotRect.left() + xNorm * plotRect.width();
            if (x < plotRect.left() || x > plotRect.right()) {
                continue;
            }

            double yNorm = (m_samples[i][ch] - minV) / range;
            double y = bandTop + (1.0 - yNorm) * (bandHeight * 0.85) + bandHeight * 0.075;
            QPointF curr(x, y);

            if (!started) {
                started = true;
            } else {
                painter.drawLine(prev, curr);
            }
            prev = curr;
        }

        painter.setPen(Qt::darkGray);
        const QString name = (ch < m_channelNames.size()) ? m_channelNames[ch] : QString("Ch%1").arg(ch + 1);
        painter.drawText(8, static_cast<int>(bandTop + 18), name);
        painter.drawText(plotRect.left() - 66, static_cast<int>(bandTop + 14), 58, 14, Qt::AlignRight | Qt::AlignVCenter, QString::number(maxV, 'g', 4));
        painter.drawText(plotRect.left() - 66, static_cast<int>(bandBottom - 2), 58, 14, Qt::AlignRight | Qt::AlignVCenter, QString::number(minV, 'g', 4));

        // Add intermediate tick marks
        const int yTickCount = 3;
        for (int t = 1; t < yTickCount; ++t) {
            const double yRatio = static_cast<double>(t) / yTickCount;
            const double yVal = minV + yRatio * range;
            const double y = bandTop + (1.0 - yRatio) * (bandHeight * 0.85) + bandHeight * 0.075;
            painter.setPen(QPen(QColor(200, 200, 200), 1));
            painter.drawLine(plotRect.left(), static_cast<int>(y), plotRect.right(), static_cast<int>(y));
            painter.setPen(Qt::darkGray);
            painter.drawText(plotRect.left() - 66, static_cast<int>(y + 4), 58, 14, Qt::AlignRight | Qt::AlignVCenter, QString::number(yVal, 'g', 3));
        }

        if (minV <= 0.0 && maxV >= 0.0) {
            const double zeroY = bandTop + (1.0 - (0.0 - minV) / range) * (bandHeight * 0.85) + bandHeight * 0.075;
            painter.setPen(QPen(QColor(170, 170, 170), 1, Qt::DashLine));
            painter.drawLine(plotRect.left(), static_cast<int>(zeroY), plotRect.right(), static_cast<int>(zeroY));
        }
    }

    painter.setPen(QColor(70, 70, 70));
    painter.save();
    painter.translate(22, plotRect.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-80, -20, 160, 20), Qt::AlignCenter, "Amplitude");
    painter.restore();
}

void WaveformWidget::wheelEvent(QWheelEvent *event) {
    if (m_samples.isEmpty()) {
        return;
    }

    const int n = totalSamples();
    if (n <= 0) {
        return;
    }

    const double oldVisible = visibleSampleCount();
    const QPoint wheelPos = event->position().toPoint();
    const bool inPlot = pointInPlot(wheelPos);
    const double mouseSample = inPlot ? pixelToSampleX(wheelPos.x()) : (m_offsetX + oldVisible * 0.5);

    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    m_zoomX = std::clamp(m_zoomX * factor, 1.0, 120.0);

    const double newVisible = visibleSampleCount();
    if (inPlot) {
        const double r = static_cast<double>(wheelPos.x() - m_lastPlotRect.left()) / std::max(1, m_lastPlotRect.width());
        m_offsetX = clampOffset(mouseSample - r * newVisible);
    } else {
        m_offsetX = clampOffset(mouseSample - 0.5 * newVisible);
    }

    update();
    emit viewWindowChanged(visibleStartSample(), visibleEndSample());
    event->accept();
}

void WaveformWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        resetView();
        emit viewWindowChanged(visibleStartSample(), visibleEndSample());
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && pointInPlot(event->pos()) && m_plotMode == PlotMode::SingleFile) {
        if (m_pickMode == PickMode::PickP || m_pickMode == PickMode::PickS) {
            PickMarker marker;
            marker.sampleIndex = std::clamp(static_cast<int>(std::round(pixelToSampleX(event->pos().x()))), 0, std::max(0, totalSamples() - 1));
            marker.channel = pixelToChannelY(event->pos().y());
            marker.phase = (m_pickMode == PickMode::PickP) ? "P" : "S";
            marker.suggested = false;
            upsertPickMarker(marker);
            emit pickMarkerAdded(marker.sampleIndex, marker.channel, marker.phase, marker.suggested);
            update();
            event->accept();
            return;
        }

        if (m_pickMode == PickMode::Erase) {
            const int idx = findClosestPickMarkerIndex(event->pos(), 12);
            if (idx >= 0 && idx < m_pickMarkers.size()) {
                const PickMarker marker = m_pickMarkers[idx];
                m_pickMarkers.removeAt(idx);
                emit pickMarkerRemoved(marker.sampleIndex, marker.channel, marker.phase, marker.suggested);
                update();
            }
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::MiddleButton && pointInPlot(event->pos())) {
        m_dragging = true;
        m_dragStart = event->pos();
        m_dragStartOffset = m_offsetX;
    } else if (event->button() == Qt::LeftButton && pointInPlot(event->pos())) {
        m_selecting = true;
        m_selectStart = event->pos();
        m_selectEnd = event->pos();
    }

    QWidget::mousePressEvent(event);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event) {
    updateHoverText(event->pos());

    if (m_dragging && !m_samples.isEmpty()) {
        const double dx = event->pos().x() - m_dragStart.x();
        const double scale = visibleSampleCount() / std::max(1, m_lastPlotRect.width());
        m_offsetX = clampOffset(m_dragStartOffset - dx * scale);
        emit viewWindowChanged(visibleStartSample(), visibleEndSample());
        update();
    }

    if (m_selecting) {
        m_selectEnd = event->pos();
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::MiddleButton) {
        m_dragging = false;
    } else if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_selectEnd = event->pos();

        QRect sel = QRect(m_selectStart, m_selectEnd).normalized() & m_lastPlotRect;
        if (sel.width() >= 8) {
            const double sx = pixelToSampleX(sel.left());
            const double ex = pixelToSampleX(sel.right());
            const double span = std::abs(ex - sx);

            if (span >= 6.0) {
                const int n = totalSamples();
                m_zoomX = std::clamp(static_cast<double>(n) / span, 1.0, 120.0);
                m_offsetX = clampOffset(std::min(sx, ex));
                emit viewWindowChanged(visibleStartSample(), visibleEndSample());
            }
        }
        update();
    }

    QWidget::mouseReleaseEvent(event);
}

void WaveformWidget::drawStackedWaveforms(QPainter &painter, const QRect &plotRect) {
    const int traceCount = m_samples.size();
    if (traceCount <= 0) {
        return;
    }

    int maxLen = 0;
    double globalPeak = 0.0;
    for (const auto &trace : m_samples) {
        maxLen = std::max(maxLen, static_cast<int>(trace.size()));
        for (double v : trace) {
            globalPeak = std::max(globalPeak, std::abs(v));
        }
    }

    if (maxLen <= 0) {
        return;
    }
    if (globalPeak < 1e-12) {
        globalPeak = 1.0;
    }

    const double spacing = 2.5 * globalPeak;
    const double yMin = -1.2 * globalPeak;
    const double yMax = (traceCount - 1) * spacing + 1.2 * globalPeak;
    const double yRange = yMax - yMin;

    const double visible = visibleSampleCount();
    const int iStart = std::max(0, static_cast<int>(std::floor(m_offsetX)));
    const int iEnd = std::min(maxLen - 1, static_cast<int>(std::ceil(m_offsetX + visible)));

    painter.setPen(QPen(QColor(160, 160, 160), 1));
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
    painter.drawLine(plotRect.topLeft(), plotRect.bottomLeft());

    const int xTickCount = 6;
    for (int t = 0; t <= xTickCount; ++t) {
        const double ratio = static_cast<double>(t) / xTickCount;
        const int x = plotRect.left() + static_cast<int>(ratio * plotRect.width());
        painter.drawLine(x, plotRect.bottom(), x, plotRect.bottom() + 4);
        const int sampleIdx = static_cast<int>(std::round(m_offsetX + ratio * visible));
        painter.drawText(x - 20, plotRect.bottom() + 18, 52, 14, Qt::AlignHCenter | Qt::AlignTop, QString::number(sampleIdx));
    }
    painter.drawText(plotRect.left(), plotRect.bottom() + 32, "Sample Index");

    for (int t = 0; t < traceCount; ++t) {
        const double base = t * spacing;
        const double yRatio = (base - yMin) / yRange;
        const int yPix = plotRect.bottom() - static_cast<int>(yRatio * plotRect.height());

        painter.setPen(QPen(QColor(220, 220, 220), 1));
        painter.drawLine(plotRect.left(), yPix, plotRect.right(), yPix);

        const QString name = (t < m_channelNames.size()) ? m_channelNames[t] : QString("Trace%1").arg(t + 1);
        painter.setPen(QColor(90, 90, 90));
        painter.drawText(plotRect.left() - 115, yPix - 7, 110, 14, Qt::AlignRight | Qt::AlignVCenter, name.left(28));

        painter.setPen(QPen(QColor(31, 119, 180), 1));
        QPointF prev;
        bool started = false;
        const QVector<double> &trace = m_samples[t];

        for (int i = iStart; i <= iEnd; ++i) {
            if (i >= trace.size()) {
                continue;
            }

            const double xNorm = (static_cast<double>(i) - m_offsetX) / visible;
            const double x = plotRect.left() + xNorm * plotRect.width();
            const double yVal = trace[i] + base;
            const double yNorm = (yVal - yMin) / yRange;
            const double y = plotRect.bottom() - yNorm * plotRect.height();
            QPointF curr(x, y);

            if (started) {
                painter.drawLine(prev, curr);
            }
            prev = curr;
            started = true;
        }
    }

    painter.setPen(QColor(70, 70, 70));
    painter.save();
    painter.translate(20, plotRect.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-120, -20, 240, 20), Qt::AlignCenter, "Stacked amplitude / trace");
    painter.restore();
}

QRect WaveformWidget::makePlotRect(int w, int h) const {
    const int left = (m_plotMode == PlotMode::Stacked) ? 140 : 80;
    const int right = 24;
    const int top = 28;
    const int bottom = 70;
    return QRect(left, top, std::max(10, w - left - right), std::max(10, h - top - bottom));
}

int WaveformWidget::totalSamples() const {
    if (m_samples.isEmpty()) {
        return 0;
    }

    if (m_plotMode == PlotMode::Stacked) {
        int maxLen = 0;
        for (const auto &trace : m_samples) {
            maxLen = std::max(maxLen, static_cast<int>(trace.size()));
        }
        return maxLen;
    }

    return m_samples.size();
}

void WaveformWidget::resetView() {
    m_zoomX = 1.0;
    m_offsetX = 0.0;
    m_dragging = false;
    m_selecting = false;
    m_hoverSample = -1;
    m_hoverAmp = 0.0;
    m_hasHover = false;
}

double WaveformWidget::visibleSampleCount() const {
    const int n = std::max(1, totalSamples());
    return std::max(1.0, static_cast<double>(n) / std::max(1.0, m_zoomX));
}

double WaveformWidget::clampOffset(double offset) const {
    const int n = std::max(1, totalSamples());
    const double vis = std::max(1.0, visibleSampleCount());
    const double maxOffset = std::max(0.0, static_cast<double>(n) - vis);
    return std::clamp(offset, 0.0, maxOffset);
}

bool WaveformWidget::pointInPlot(const QPoint &pt) const {
    return m_lastPlotRect.contains(pt);
}

double WaveformWidget::pixelToSampleX(int px) const {
    if (m_lastPlotRect.width() <= 0) {
        return 0.0;
    }
    const double ratio = static_cast<double>(px - m_lastPlotRect.left()) / m_lastPlotRect.width();
    const double clampedRatio = std::clamp(ratio, 0.0, 1.0);
    return m_offsetX + clampedRatio * visibleSampleCount();
}

void WaveformWidget::updateHoverText(const QPoint &pt) {
    if (!pointInPlot(pt) || m_samples.isEmpty()) {
        m_hoverSample = -1;
        m_hoverAmp = 0.0;
        m_hasHover = false;
        update();
        return;
    }

    const int idx = static_cast<int>(std::round(pixelToSampleX(pt.x())));
    m_hasHover = true;
    m_hoverSample = idx;

    if (m_plotMode == PlotMode::Stacked) {
        m_hoverAmp = 0.0;
    } else {
        if (idx < 0 || idx >= m_samples.size()) {
            m_hoverAmp = 0.0;
        } else {
            const QVector<double> &row = m_samples[idx];
            m_hoverAmp = row.isEmpty() ? 0.0 : row[0];
        }
    }

    update();
}

void WaveformWidget::drawPickMarkers(QPainter &painter, const QRect &plotRect) {
    if (m_plotMode != PlotMode::SingleFile || m_samples.isEmpty() || m_pickMarkers.isEmpty()) {
        return;
    }

    const int channelCount = m_samples[0].size();
    if (channelCount <= 0) {
        return;
    }

    const double visible = visibleSampleCount();
    const double bandHeight = static_cast<double>(plotRect.height()) / channelCount;
    const int markerHalfHeight = 8;

    for (const PickMarker &marker : m_pickMarkers) {
        if (marker.channel < 0 || marker.channel >= channelCount) {
            continue;
        }

        const double xNorm = (static_cast<double>(marker.sampleIndex) - m_offsetX) / visible;
        if (xNorm < 0.0 || xNorm > 1.0) {
            continue;
        }

        const int x = plotRect.left() + static_cast<int>(std::round(xNorm * plotRect.width()));
        const double bandTop = plotRect.top() + marker.channel * bandHeight;
        const double bandBottom = bandTop + bandHeight;
        const int yCenter = static_cast<int>(std::round((bandTop + bandBottom) * 0.5));

        QColor color = (marker.phase == "P") ? QColor(220, 20, 60) : QColor(25, 118, 210);
        QPen pen(color, 1.6, marker.suggested ? Qt::DashLine : Qt::SolidLine);
        painter.setPen(pen);
        painter.drawLine(x, static_cast<int>(bandTop + 2), x, static_cast<int>(bandBottom - 2));

        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPoint(x, yCenter), markerHalfHeight / 2, markerHalfHeight / 2);

        painter.setPen(color.darker(150));
        const QString text = marker.suggested ? (marker.phase + "?") : marker.phase;
        painter.drawText(x + 4, yCenter - 4, text);
    }
}

void WaveformWidget::drawAssistOverlay(QPainter &painter, const QRect &plotRect) {
    if (m_plotMode != PlotMode::SingleFile || m_samples.isEmpty()) {
        return;
    }

    const int n = totalSamples();
    const double visible = visibleSampleCount();

    if (m_hasAssistRange) {
        const int rangeStart = std::clamp(std::min(m_assistRangeStart, m_assistRangeEnd), 0, std::max(0, n - 1));
        const int rangeEnd = std::clamp(std::max(m_assistRangeStart, m_assistRangeEnd), 0, std::max(0, n - 1));

        const double xNorm1 = (static_cast<double>(rangeStart) - m_offsetX) / visible;
        const double xNorm2 = (static_cast<double>(rangeEnd) - m_offsetX) / visible;
        const int x1 = plotRect.left() + static_cast<int>(std::round(xNorm1 * plotRect.width()));
        const int x2 = plotRect.left() + static_cast<int>(std::round(xNorm2 * plotRect.width()));

        const int sx = std::max(plotRect.left(), std::min(x1, x2));
        const int ex = std::min(plotRect.right(), std::max(x1, x2));
        if (ex > sx) {
            painter.fillRect(QRect(sx, plotRect.top(), ex - sx, plotRect.height()), QColor(255, 193, 7, 28));
        }

        QPen rangePen(QColor(255, 152, 0), 1.3, Qt::DashLine);
        painter.setPen(rangePen);
        if (x1 >= plotRect.left() && x1 <= plotRect.right()) {
            painter.drawLine(x1, plotRect.top(), x1, plotRect.bottom());
        }
        if (x2 >= plotRect.left() && x2 <= plotRect.right()) {
            painter.drawLine(x2, plotRect.top(), x2, plotRect.bottom());
        }
    }

    if (m_assistCurve.isEmpty() || m_assistCurve.size() != n || m_samples[0].isEmpty()) {
        return;
    }

    const int iStart = std::max(0, static_cast<int>(std::floor(m_offsetX)));
    const int iEnd = std::min(n - 1, static_cast<int>(std::ceil(m_offsetX + visible)));
    if (iEnd <= iStart) {
        return;
    }

    double minV = 0.0;
    double maxV = 0.0;
    bool hasFinite = false;
    for (int i = iStart; i <= iEnd; ++i) {
        const double v = m_assistCurve[i];
        if (!std::isfinite(v)) {
            continue;
        }
        if (!hasFinite) {
            minV = v;
            maxV = v;
            hasFinite = true;
        } else {
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
        }
    }
    if (!hasFinite) {
        return;
    }

    const double range = (maxV - minV < 1e-12) ? 1.0 : (maxV - minV);
    const int channelCount = m_samples[0].size();
    const double bandTop = plotRect.top();
    const double bandHeight = static_cast<double>(plotRect.height()) / std::max(1, channelCount);

    painter.setPen(QPen(QColor(156, 39, 176), 1.5, Qt::SolidLine));
    bool started = false;
    QPointF prev;
    for (int i = iStart; i <= iEnd; ++i) {
        const double v = m_assistCurve[i];
        if (!std::isfinite(v)) {
            started = false;
            continue;
        }

        const double xNorm = (static_cast<double>(i) - m_offsetX) / visible;
        const double x = plotRect.left() + xNorm * plotRect.width();
        if (x < plotRect.left() || x > plotRect.right()) {
            continue;
        }

        const double yNorm = (v - minV) / range;
        const double y = bandTop + (1.0 - yNorm) * (bandHeight * 0.85) + bandHeight * 0.075;
        const QPointF curr(x, y);

        if (started) {
            painter.drawLine(prev, curr);
        }
        prev = curr;
        started = true;
    }

    if (!m_assistCurveName.isEmpty()) {
        painter.setPen(QColor(90, 30, 120));
        painter.drawText(plotRect.right() - 220, plotRect.top() + 18, 210, 16, Qt::AlignRight | Qt::AlignVCenter,
                         QString("Assist: %1").arg(m_assistCurveName));
    }
}

int WaveformWidget::pixelToChannelY(int py) const {
    if (m_plotMode != PlotMode::SingleFile || m_samples.isEmpty()) {
        return 0;
    }

    const int channelCount = m_samples[0].size();
    if (channelCount <= 1) {
        return 0;
    }

    const int rel = py - m_lastPlotRect.top();
    const double ch = static_cast<double>(rel) / std::max(1, m_lastPlotRect.height()) * channelCount;
    return std::clamp(static_cast<int>(std::floor(ch)), 0, channelCount - 1);
}

void WaveformWidget::upsertPickMarker(const PickMarker &marker) {
    for (int i = 0; i < m_pickMarkers.size(); ++i) {
        if (m_pickMarkers[i].channel == marker.channel &&
            m_pickMarkers[i].phase == marker.phase &&
            m_pickMarkers[i].suggested == marker.suggested) {
            m_pickMarkers[i] = marker;
            return;
        }
    }
    m_pickMarkers.push_back(marker);
}

int WaveformWidget::findClosestPickMarkerIndex(const QPoint &pt, int maxPixelDistance) const {
    if (m_pickMarkers.isEmpty() || m_plotMode != PlotMode::SingleFile || m_samples.isEmpty()) {
        return -1;
    }

    const int channelCount = m_samples[0].size();
    if (channelCount <= 0) {
        return -1;
    }

    const double visible = visibleSampleCount();
    const double bandHeight = static_cast<double>(m_lastPlotRect.height()) / channelCount;

    int bestIdx = -1;
    double bestDist2 = static_cast<double>(maxPixelDistance) * maxPixelDistance;
    for (int i = 0; i < m_pickMarkers.size(); ++i) {
        const PickMarker &marker = m_pickMarkers[i];
        if (marker.channel < 0 || marker.channel >= channelCount) {
            continue;
        }

        const double xNorm = (static_cast<double>(marker.sampleIndex) - m_offsetX) / visible;
        if (xNorm < 0.0 || xNorm > 1.0) {
            continue;
        }

        const double x = m_lastPlotRect.left() + xNorm * m_lastPlotRect.width();
        const double y = m_lastPlotRect.top() + (marker.channel + 0.5) * bandHeight;
        const double dx = x - pt.x();
        const double dy = y - pt.y();
        const double dist2 = dx * dx + dy * dy;
        if (dist2 <= bestDist2) {
            bestDist2 = dist2;
            bestIdx = i;
        }
    }

    return bestIdx;
}
