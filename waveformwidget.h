#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QWidget>
#include <QVector>
#include <QStringList>

class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    struct PickMarker {
        int sampleIndex = 0;
        int channel = 0;
        QString phase;
        bool suggested = false;
    };

    enum class PickMode {
        Navigate,
        PickP,
        PickS,
        Erase
    };

    explicit WaveformWidget(QWidget *parent = nullptr);

    void setData(const QVector<QVector<double>> &samples, const QStringList &channelNames);
    void setStackedData(const QVector<QVector<double>> &traces, const QStringList &traceNames);
    void clearData();
    bool saveAsPng(const QString &filePath, int width, int height);
    int visibleStartSample() const;
    int visibleEndSample() const;
    void setPickMode(PickMode mode);
    PickMode pickMode() const;
    void setPickMarkers(const QVector<PickMarker> &markers);
    QVector<PickMarker> pickMarkers() const;
    void clearPickMarkers();
    int sampleIndexAt(const QPoint &pt) const;
    void setAssistRangeStart(int sampleIndex);
    void setAssistRangeEnd(int sampleIndex);
    void clearAssistRange();
    bool assistRange(int &startSample, int &endSample) const;
    void setAssistCurve(const QVector<double> &curve, const QString &name);
    void clearAssistCurve();

signals:
    void viewWindowChanged(int startSample, int endSample);
    void pickMarkerAdded(int sampleIndex, int channel, const QString &phase, bool suggested);
    void pickMarkerRemoved(int sampleIndex, int channel, const QString &phase, bool suggested);
    void mouseHovered(int channelIndex, double timeSec, double amplitude);
    void assistRangeChanged(int startSample, int endSample, bool valid);


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
    void drawPickMarkers(QPainter &painter, const QRect &plotRect);
    void drawAssistOverlay(QPainter &painter, const QRect &plotRect);
    QRect makePlotRect(int w, int h) const;
    int totalSamples() const;
    void resetView();
    double visibleSampleCount() const;
    double clampOffset(double offset) const;
    bool pointInPlot(const QPoint &pt) const;
    double pixelToSampleX(int px) const;
    int pixelToChannelY(int py) const;
    void updateHoverText(const QPoint &pt);
    void upsertPickMarker(const PickMarker &marker);
    int findClosestPickMarkerIndex(const QPoint &pt, int maxPixelDistance) const;

    QVector<QVector<double>> m_samples;
    QStringList m_channelNames;
    PlotMode m_plotMode = PlotMode::SingleFile;
    PickMode m_pickMode = PickMode::Navigate;
    QVector<PickMarker> m_pickMarkers;
    bool m_hasAssistRange = false;
    int m_assistRangeStart = 0;
    int m_assistRangeEnd = 0;
    QVector<double> m_assistCurve;
    QString m_assistCurveName;

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
