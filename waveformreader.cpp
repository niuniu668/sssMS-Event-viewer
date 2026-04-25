#include "waveformreader.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <cstring>

bool WaveformReader::readWaveFile(const QString &filePath, WaveData &outData, QString &error) {
    outData = WaveData{};

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        error = QString("File does not exist: %1").arg(filePath);
        return false;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        error = QString("Cannot open file: %1").arg(filePath);
        return false;
    }

    const QByteArray rawBytes = f.readAll();
    f.close();

    const int doubleCount = rawBytes.size() / 8;
    if (doubleCount <= 6) {
        error = QString("Data too short (<=6 doubles): %1").arg(filePath);
        return false;
    }

    QVector<double> values(doubleCount);
    std::memcpy(values.data(), rawBytes.constData(), static_cast<size_t>(doubleCount) * 8);

    values = values.mid(6); // skip header doubles
    const bool isThree = rawBytes.size() > 43 && static_cast<unsigned char>(rawBytes[43]) == 3;
    outData.isThreeComponent = isThree;

    if (isThree) {
        const int usable = (values.size() / 3) * 3;
        if (usable == 0) {
            error = QString("Three-component data too short: %1").arg(filePath);
            return false;
        }

        outData.samples.resize(usable / 3);
        for (int i = 0; i < usable / 3; ++i) {
            outData.samples[i] = {values[i * 3], values[i * 3 + 1], values[i * 3 + 2]};
        }
    } else {
        outData.samples.resize(values.size());
        for (int i = 0; i < values.size(); ++i) {
            outData.samples[i] = {values[i]};
        }
    }

    return true;
}

bool WaveformReader::readTextWaveFile(const QString &filePath, WaveData &outData, QString &error) {
    outData = WaveData{};

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        error = QString("File does not exist: %1").arg(filePath);
        return false;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QString("Cannot open text file: %1").arg(filePath);
        return false;
    }

    QVector<QVector<double>> samples;
    bool isThreeComp = false;
    int lineCount = 0;

    QTextStream in(&f);
    QRegularExpression re("[,\\s]+");
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QStringList parts = line.split(re, Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        QVector<double> vals;
        for (const QString &s : parts) {
            bool ok;
            double d = s.toDouble(&ok);
            if (ok) vals.push_back(d);
        }

        if (vals.isEmpty()) continue;

        if (lineCount == 0) {
            isThreeComp = (vals.size() >= 3);
            outData.isThreeComponent = isThreeComp;
        }

        QVector<double> sample(isThreeComp ? 3 : 1, 0.0);
        for (int i = 0; i < qMin((int)vals.size(), isThreeComp ? 3 : 1); ++i) {
            sample[i] = vals[i];
        }
        samples.push_back(sample);
        lineCount++;
    }
    
    f.close();
    
    if (samples.isEmpty()) {
        error = QString("No data found in text file: %1").arg(filePath);
        return false;
    }
    
    outData.samples = samples;
    return true;
}

