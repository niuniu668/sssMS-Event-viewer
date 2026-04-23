#include "spectrumwidget.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent) {
    setMinimumHeight(180);
}

void SpectrumWidget::setSignal(const QVector<double> &signal, int sampleRateHz) {
    m_signal = signal;
    m_sampleRateHz = std::max(1, sampleRateHz);
    const int size = static_cast<int>(m_signal.size());
    m_startSample = 0;
    m_endSample = std::max(0, size - 1);
    recomputeSpectrum();
    update();
}

void SpectrumWidget::setViewRange(int startSample, int endSample) {
    m_startSample = std::max(0, startSample);
    m_endSample = std::max(m_startSample, endSample);
    recomputeSpectrum();
    update();
}

void SpectrumWidget::recomputeSpectrum() {
    m_freqAxisHz.clear();
    m_magDb.clear();

    if (m_signal.size() < 16) {
        return;
    }

    const int size = static_cast<int>(m_signal.size());
    const int s0 = std::clamp(m_startSample, 0, size - 1);
    const int s1 = std::clamp(m_endSample, s0, size - 1);
    const int len = s1 - s0 + 1;
    if (len < 16) {
        return;
    }

    const int maxN = 2048;
    const int n = std::min(maxN, len);
    QVector<double> windowed(n, 0.0);

    for (int i = 0; i < n; ++i) {
        const int srcIdx = s0 + static_cast<int>((static_cast<double>(i) / std::max(1, n - 1)) * (len - 1));
        const double hann = 0.5 - 0.5 * std::cos((2.0 * kPi * i) / std::max(1, n - 1));
        windowed[i] = m_signal[srcIdx] * hann;
    }

    const int bins = n / 2;
    if (bins <= 0) {
        return;
    }

    QVector<double> mags(bins, 0.0);
    double peak = 1e-12;

    for (int k = 0; k < bins; ++k) {
        double re = 0.0;
        double im = 0.0;
        const double w = 2.0 * kPi * k / n;
        for (int t = 0; t < n; ++t) {
            const double ang = w * t;
            re += windowed[t] * std::cos(ang);
            im -= windowed[t] * std::sin(ang);
        }

        const double mag = std::sqrt(re * re + im * im) / n;
        mags[k] = mag;
        peak = std::max(peak, mag);
    }

    m_freqAxisHz.resize(bins);
    m_magDb.resize(bins);
    for (int k = 0; k < bins; ++k) {
        const double norm = mags[k] / peak;
        m_freqAxisHz[k] = (static_cast<double>(k) * m_sampleRateHz) / n;
        m_magDb[k] = 20.0 * std::log10(std::max(norm, 1e-6));
    }
}

void SpectrumWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);

    const QRect plot(58, 24, std::max(10, width() - 80), std::max(10, height() - 52));
    p.fillRect(plot, QColor(248, 252, 255));
    p.setPen(QPen(QColor(180, 190, 200), 1));
    p.drawRect(plot);

    p.setPen(QColor(50, 60, 70));
    p.drawText(10, 16, QString("Spectrum (Fs=%1 Hz, visible window)").arg(m_sampleRateHz));

    if (m_freqAxisHz.isEmpty()) {
        p.setPen(QColor(130, 130, 130));
        p.drawText(plot, Qt::AlignCenter, "No data");
        return;
    }

    const double fMin = 0.0;
    const double fMax = m_sampleRateHz * 0.5;
    const double dbMin = -100.0;
    const double dbMax = 0.0;

    p.setPen(QPen(QColor(220, 226, 234), 1));
    const int xTickCount = 6;
    for (int i = 0; i <= xTickCount; ++i) {
        const double r = static_cast<double>(i) / xTickCount;
        const int x = plot.left() + static_cast<int>(r * plot.width());
        p.drawLine(x, plot.top(), x, plot.bottom());
    }
    const int yTickCount = 5;
    for (int i = 0; i <= yTickCount; ++i) {
        const double r = static_cast<double>(i) / yTickCount;
        const int y = plot.bottom() - static_cast<int>(r * plot.height());
        p.drawLine(plot.left(), y, plot.right(), y);
    }

    p.setPen(QPen(QColor(24, 105, 188), 1));
    QPointF prev;
    bool started = false;
    for (int i = 0; i < m_freqAxisHz.size(); ++i) {
        const double fx = (m_freqAxisHz[i] - fMin) / std::max(1e-9, fMax - fMin);
        const double fy = (m_magDb[i] - dbMin) / std::max(1e-9, dbMax - dbMin);
        const double x = plot.left() + std::clamp(fx, 0.0, 1.0) * plot.width();
        const double y = plot.bottom() - std::clamp(fy, 0.0, 1.0) * plot.height();
        const QPointF pt(x, y);
        if (started) {
            p.drawLine(prev, pt);
        }
        prev = pt;
        started = true;
    }

    p.setPen(QColor(70, 70, 70));
    for (int i = 0; i <= xTickCount; ++i) {
        const double r = static_cast<double>(i) / xTickCount;
        const int x = plot.left() + static_cast<int>(r * plot.width());
        const int f = static_cast<int>(std::round(fMin + r * (fMax - fMin)));
        p.drawText(x - 26, plot.bottom() + 4, 52, 16, Qt::AlignHCenter | Qt::AlignTop, QString::number(f));
    }
    p.drawText(plot.left(), height() - 6, "Frequency (Hz)");

    for (int i = 0; i <= yTickCount; ++i) {
        const double r = static_cast<double>(i) / yTickCount;
        const int y = plot.bottom() - static_cast<int>(r * plot.height());
        const int db = static_cast<int>(std::round(dbMin + r * (dbMax - dbMin)));
        p.drawText(plot.left() - 48, y - 8, 42, 16, Qt::AlignRight | Qt::AlignVCenter, QString::number(db));
    }
    p.save();
    p.translate(14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRect(-58, -10, 116, 20), Qt::AlignCenter, "Magnitude (dB)");
    p.restore();
}
