#ifndef SPECTROGRAMWIDGET_H
#define SPECTROGRAMWIDGET_H

#include <QImage>
#include <QWidget>
#include <QVector>

class SpectrogramWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpectrogramWidget(QWidget *parent = nullptr);

    void setSignal(const QVector<double> &signal, int sampleRateHz);
    void setViewRange(int startSample, int endSample);

    void setWindowSize(int winSize);
    void setHopSize(int hopSize);
    int windowSize() const { return m_winSize; }
    int hopSize() const { return m_hopSize; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void recomputeSpectrogram();
    QRgb colorMap(double v) const;

    QVector<double> m_signal;
    QImage m_specImage;

    int m_sampleRateHz = 4000;
    int m_startSample = 0;
    int m_endSample = 0;
    int m_frameCount = 0;
    int m_winSize = 512;
    int m_hopSize = 128;
};

#endif
