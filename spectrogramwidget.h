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
};

#endif
