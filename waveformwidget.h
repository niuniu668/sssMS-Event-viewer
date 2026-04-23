#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QWidget>
#include <QVector>
#include <QStringList>

class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    void setData(const QVector<QVector<double>> &samples, const QStringList &channelNames);
    void setStackedData(const QVector<QVector<double>> &traces, const QStringList &traceNames);
    void clearData();
    bool saveAsPng(const QString &filePath, int width, int height);
    int visibleStartSample() const;
    int visibleEndSample() const;

signals:
    void viewWindowChanged(int startSample, int endSample);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum class PlotMode {
        SingleFile,
        Stacked
    };

    void drawWaveforms(QPainter &painter, const QRect &plotRect);
    void drawSingleFileWaveforms(QPainter &painter, const QRect &plotRect);
    void drawStackedWaveforms(QPainter &painter, const QRect &plotRect);
    QRect makePlotRect(int w, int h) const;
    int totalSamples() const;
    void resetView();
    double visibleSampleCount() const;
    double clampOffset(double offset) const;
    bool pointInPlot(const QPoint &pt) const;
    double pixelToSampleX(int px) const;
    void updateHoverText(const QPoint &pt);

    QVector<QVector<double>> m_samples;
    QStringList m_channelNames;
    PlotMode m_plotMode = PlotMode::SingleFile;

    double m_zoomX = 1.0;
    double m_offsetX = 0.0;

    QRect m_lastPlotRect;

    bool m_dragging = false;
    QPoint m_dragStart;
    double m_dragStartOffset = 0.0;

    bool m_selecting = false;
    QPoint m_selectStart;
    QPoint m_selectEnd;

    int m_hoverSample = -1;
    double m_hoverAmp = 0.0;
    bool m_hasHover = false;
};

#endif
