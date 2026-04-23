#include "waveformreader.h"

#include <QFile>
#include <QFileInfo>
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
