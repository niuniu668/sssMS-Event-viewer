#ifndef WAVEFORMREADER_H
#define WAVEFORMREADER_H

#include <QString>
#include <QVector>

struct WaveData {
    QVector<QVector<double>> samples; // shape: (N, C)
    bool isThreeComponent = false;
};

class WaveformReader {
public:
    static bool readWaveFile(const QString &filePath, WaveData &outData, QString &error);
    static bool readTextWaveFile(const QString &filePath, WaveData &outData, QString &error);
};

#endif
