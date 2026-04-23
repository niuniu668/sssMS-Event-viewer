#ifndef SPECTRUMWIDGET_H
#define SPECTRUMWIDGET_H

#include <QWidget>
#include <QVector>

class SpectrumWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

    void setSignal(const QVector<double> &signal, int sampleRateHz);
    void setViewRange(int startSample, int endSample);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void recomputeSpectrum();

    QVector<double> m_signal;
    QVector<double> m_freqAxisHz;
    QVector<double> m_magDb;

    int m_sampleRateHz = 4000;
    int m_startSample = 0;
    int m_endSample = 0;
};

#endif
