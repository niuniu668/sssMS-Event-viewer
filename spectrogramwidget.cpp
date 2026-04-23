#include "spectrogramwidget.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

SpectrogramWidget::SpectrogramWidget(QWidget *parent)
    : QWidget(parent) {
    setMinimumHeight(220);
}

void SpectrogramWidget::setSignal(const QVector<double> &signal, int sampleRateHz) {
    m_signal = signal;
    m_sampleRateHz = std::max(1, sampleRateHz);
    const int size = static_cast<int>(m_signal.size());
    m_startSample = 0;
    m_endSample = std::max(0, size - 1);
    recomputeSpectrogram();
    update();
}

void SpectrogramWidget::setViewRange(int startSample, int endSample) {
    m_startSample = std::max(0, startSample);
    m_endSample = std::max(m_startSample, endSample);
    recomputeSpectrogram();
    update();
}

QRgb SpectrogramWidget::colorMap(double v) const {
    const double x = std::clamp(v, 0.0, 1.0);
    const int r = static_cast<int>(255.0 * std::pow(x, 0.85));
    const int g = static_cast<int>(255.0 * std::sin(x * kPi));
    const int b = static_cast<int>(255.0 * (1.0 - std::pow(x, 0.55)));
    return qRgb(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

void SpectrogramWidget::recomputeSpectrogram() {
    m_specImage = QImage();
    m_frameCount = 0;

    if (m_signal.size() < 128) {
        return;
    }

    const int size = static_cast<int>(m_signal.size());
    const int s0 = std::clamp(m_startSample, 0, size - 1);
    const int s1 = std::clamp(m_endSample, s0, size - 1);
    const int len = s1 - s0 + 1;
    if (len < 128) {
        return;
    }

    const int maxLen = 4096;
    const int sampledLen = std::min(maxLen, len);
    QVector<double> seg(sampledLen, 0.0);
    for (int i = 0; i < sampledLen; ++i) {
        const int srcIdx = s0 + static_cast<int>((static_cast<double>(i) / std::max(1, sampledLen - 1)) * (len - 1));
        seg[i] = m_signal[srcIdx];
    }

    const int win = 256;
    const int hop = 64;
    const int bins = win / 2;
    if (sampledLen < win) {
        return;
    }

    m_frameCount = 1 + (sampledLen - win) / hop;
    if (m_frameCount <= 0) {
        return;
    }

    QVector<double> mags(m_frameCount * bins, -120.0);
    double dbMin = 1e9;
    double dbMax = -1e9;

    for (int frame = 0; frame < m_frameCount; ++frame) {
        const int base = frame * hop;

        QVector<double> x(win, 0.0);
        for (int n = 0; n < win; ++n) {
            const double hann = 0.5 - 0.5 * std::cos((2.0 * kPi * n) / (win - 1));
            x[n] = seg[base + n] * hann;
        }

        for (int k = 0; k < bins; ++k) {
            double re = 0.0;
            double im = 0.0;
            const double w = 2.0 * kPi * k / win;
            for (int n = 0; n < win; ++n) {
                const double ang = w * n;
                re += x[n] * std::cos(ang);
                im -= x[n] * std::sin(ang);
            }
            const double mag = std::sqrt(re * re + im * im) / win;
            const double db = 20.0 * std::log10(std::max(mag, 1e-6));

            mags[frame * bins + k] = db;
            dbMin = std::min(dbMin, db);
            dbMax = std::max(dbMax, db);
        }
    }

    if (dbMax - dbMin < 1e-6) {
        dbMin = dbMax - 1.0;
    }

    m_specImage = QImage(m_frameCount, bins, QImage::Format_RGB32);
    for (int x = 0; x < m_frameCount; ++x) {
        for (int y = 0; y < bins; ++y) {
            const int k = bins - 1 - y;
            const double db = mags[x * bins + k];
            const double t = (db - dbMin) / (dbMax - dbMin);
            m_specImage.setPixel(x, y, colorMap(t));
        }
    }
}

void SpectrogramWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);

    const QRect plot(58, 24, std::max(10, width() - 80), std::max(10, height() - 52));
    p.fillRect(plot, QColor(248, 252, 255));
    p.setPen(QPen(QColor(180, 190, 200), 1));
    p.drawRect(plot);

    p.setPen(QColor(50, 60, 70));
    p.drawText(10, 16, QString("Time-Frequency (Fs=%1 Hz, visible window)").arg(m_sampleRateHz));

    if (m_specImage.isNull()) {
        p.setPen(QColor(130, 130, 130));
        p.drawText(plot, Qt::AlignCenter, "No data");
        return;
    }

    p.drawImage(plot, m_specImage);

    p.setPen(QColor(70, 70, 70));
    const int xTickCount = 6;
    for (int i = 0; i <= xTickCount; ++i) {
        const double r = static_cast<double>(i) / xTickCount;
        const int x = plot.left() + static_cast<int>(r * plot.width());
        const int sample = static_cast<int>(std::round(m_startSample + r * std::max(1, m_endSample - m_startSample)));
        p.drawLine(x, plot.bottom(), x, plot.bottom() + 4);
        p.drawText(x - 26, plot.bottom() + 4, 52, 16, Qt::AlignHCenter | Qt::AlignTop, QString::number(sample));
    }
    p.drawText(plot.left(), height() - 6, "Sample Index");

    const int yTickCount = 5;
    for (int i = 0; i <= yTickCount; ++i) {
        const double r = static_cast<double>(i) / yTickCount;
        const int y = plot.bottom() - static_cast<int>(r * plot.height());
        const int hz = static_cast<int>(std::round(r * (m_sampleRateHz * 0.5)));
        p.drawText(plot.left() - 48, y - 8, 42, 16, Qt::AlignRight | Qt::AlignVCenter, QString::number(hz));
    }
    p.save();
    p.translate(14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRect(-58, -10, 116, 20), Qt::AlignCenter, "Frequency (Hz)");
    p.restore();
}
