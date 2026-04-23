#include "mainwindow.h"
#include "waveformreader.h"
#include "waveformwidget.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QFileDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QPainter>
#include <QImage>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      folderEdit(new QLineEdit(this)),
      browseButton(new QPushButton("Browse", this)),
      loadButton(new QPushButton("Load Folder", this)),
      recursiveCheck(new QCheckBox("Recursive", this)),
      normalizeCheck(new QCheckBox("Normalize (overview)", this)),
      fileList(new QListWidget(this)),
      infoLabel(new QLabel(this)),
      waveWidget(new WaveformWidget(this)),
      saveCurrentButton(new QPushButton("Save Current PNG", this)),
    saveOverviewButton(new QPushButton("Save Overview PNG", this)),
    previewStackedButton(new QPushButton("Preview Stacked", this)),
    saveStackedButton(new QPushButton("Save Stacked PNG", this)) {

    setWindowTitle("Qt Waveform Viewer");
    resize(1300, 820);

    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);

    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);

    auto *folderRow = new QHBoxLayout();
    folderEdit->setPlaceholderText("Input folder path here");
    folderRow->addWidget(new QLabel("Folder:"));
    folderRow->addWidget(folderEdit, 1);
    folderRow->addWidget(browseButton);
    folderRow->addWidget(loadButton);

    fileList->setMinimumWidth(320);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color:#555;font-size:12px;");

    auto *optRow = new QHBoxLayout();
    optRow->addWidget(recursiveCheck);
    optRow->addWidget(normalizeCheck);
    optRow->addStretch();

    auto *saveRow = new QHBoxLayout();
    saveRow->addWidget(saveCurrentButton);
    saveRow->addWidget(saveOverviewButton);
    saveRow->addWidget(previewStackedButton);
    saveRow->addWidget(saveStackedButton);
    saveRow->addStretch();

    leftLayout->addLayout(folderRow);
    leftLayout->addLayout(optRow);
    leftLayout->addWidget(new QLabel("Binary Files:"));
    leftLayout->addWidget(fileList, 1);
    leftLayout->addLayout(saveRow);
    leftLayout->addWidget(new QLabel("Info:"));
    leftLayout->addWidget(infoLabel);

    auto *right = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->addWidget(new QLabel("Waveform:"));
    rightLayout->addWidget(waveWidget, 1);

    root->addWidget(left, 1);
    root->addWidget(right, 2);

    setCentralWidget(central);
    statusBar()->showMessage("Ready");

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseFolder);
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadFolder);
    connect(folderEdit, &QLineEdit::returnPressed, this, &MainWindow::loadFolder);
    connect(fileList, &QListWidget::clicked, this, &MainWindow::onFileClicked);
    connect(saveCurrentButton, &QPushButton::clicked, this, &MainWindow::exportCurrentPng);
    connect(saveOverviewButton, &QPushButton::clicked, this, &MainWindow::exportOverviewPng);
    connect(previewStackedButton, &QPushButton::clicked, this, &MainWindow::previewStacked);
    connect(saveStackedButton, &QPushButton::clicked, this, &MainWindow::exportStackedPng);
}

MainWindow::~MainWindow() = default;

void MainWindow::browseFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select folder", folderEdit->text().trimmed());
    if (!dir.isEmpty()) {
        folderEdit->setText(QDir::toNativeSeparators(dir));
        loadFolder();
    }
}

QStringList MainWindow::collectFiles(const QString &folder, bool recursive) const {
    QStringList results;
    const QStringList exts = {"event", "evt", "dat", "bin"};

    if (recursive) {
        QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            const QString suffix = fi.suffix().toLower();
            if (suffix.isEmpty() || exts.contains(suffix)) {
                results.push_back(fi.absoluteFilePath());
            }
        }
    } else {
        QDir dir(folder);
        const QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo &fi : files) {
            const QString suffix = fi.suffix().toLower();
            if (suffix.isEmpty() || exts.contains(suffix)) {
                results.push_back(fi.absoluteFilePath());
            }
        }
    }

    std::sort(results.begin(), results.end(), [](const QString &a, const QString &b) {
        return a.toLower() < b.toLower();
    });
    return results;
}

void MainWindow::loadFolder() {
    QString folder = folderEdit->text().trimmed();
    if (folder.startsWith('"') && folder.endsWith('"') && folder.size() >= 2) {
        folder = folder.mid(1, folder.size() - 2).trimmed();
    }

    if (folder.isEmpty()) {
        QMessageBox::warning(this, "Input", "Please input a folder path.");
        return;
    }

    QFileInfo fi(folder);
    if (!fi.exists() || !fi.isDir()) {
        QMessageBox::warning(this, "Input", QString("Invalid folder: %1").arg(folder));
        return;
    }

    currentFiles = collectFiles(fi.absoluteFilePath(), recursiveCheck->isChecked());
    fileList->clear();

    for (const QString &p : currentFiles) {
        fileList->addItem(QFileInfo(p).fileName());
    }

    infoLabel->setText(QString("Loaded folder:\n%1\n\nFiles found: %2")
        .arg(QDir::toNativeSeparators(fi.absoluteFilePath()))
        .arg(currentFiles.size()));

    statusBar()->showMessage(QString("Folder loaded. Files: %1").arg(currentFiles.size()));

    if (!currentFiles.isEmpty()) {
        fileList->setCurrentRow(0);
        loadSingleFile(currentFiles.first());
    } else {
        waveWidget->clearData();
    }
}

bool MainWindow::loadSingleFile(const QString &filePath) {
    WaveData wd;
    QString error;
    if (!WaveformReader::readWaveFile(filePath, wd, error)) {
        QMessageBox::warning(this, "Read error", error);
        return false;
    }

    QStringList channelNames;
    if (wd.isThreeComponent) {
        channelNames << "X" << "Y" << "Z";
    } else {
        channelNames << "Amp";
    }

    waveWidget->setData(wd.samples, channelNames);
    statusBar()->showMessage(QString("Loaded: %1  shape=(%2,%3)")
        .arg(QFileInfo(filePath).fileName())
        .arg(wd.samples.size())
        .arg(wd.samples.isEmpty() ? 0 : wd.samples[0].size()));
    return true;
}

void MainWindow::onFileClicked(const QModelIndex &index) {
    if (index.row() >= 0 && index.row() < currentFiles.size()) {
        loadSingleFile(currentFiles[index.row()]);
    }
}

void MainWindow::exportCurrentPng() {
    if (fileList->currentRow() < 0 || fileList->currentRow() >= currentFiles.size()) {
        QMessageBox::information(this, "Export", "Please select a file first.");
        return;
    }

    const QString savePath = QFileDialog::getSaveFileName(this, "Save current waveform PNG", "current_waveform.png", "PNG (*.png)");
    if (savePath.isEmpty()) {
        return;
    }

    if (!waveWidget->saveAsPng(savePath, 1600, 900)) {
        QMessageBox::warning(this, "Export", "Failed to save current waveform image.");
        return;
    }

    statusBar()->showMessage(QString("Saved: %1").arg(QDir::toNativeSeparators(savePath)), 5000);
}

void MainWindow::exportOverviewPng() {
    if (currentFiles.isEmpty()) {
        QMessageBox::information(this, "Export", "No files loaded.");
        return;
    }

    const QString savePath = QFileDialog::getSaveFileName(this, "Save overview PNG", "all_waveforms_subplots.png", "PNG (*.png)");
    if (savePath.isEmpty()) {
        return;
    }

    const int fileCount = currentFiles.size();
    const int cols = 3;
    const int rows = (fileCount + cols - 1) / cols;
    const int cellW = 520;
    const int cellH = 260;
    const int margin = 20;
    const int width = cols * cellW + margin * 2;
    const int height = rows * cellH + margin * 2 + 40;

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(Qt::black);
    p.setFont(QFont("Arial", 12));
    p.drawText(QRect(0, 8, width, 24), Qt::AlignHCenter, QString("All Waveforms Subplots | files=%1").arg(fileCount));

    for (int i = 0; i < fileCount; ++i) {
        WaveData wd;
        QString err;
        if (!WaveformReader::readWaveFile(currentFiles[i], wd, err) || wd.samples.isEmpty()) {
            continue;
        }

        const int r = i / cols;
        const int c = i % cols;
        QRect rect(margin + c * cellW, margin + 40 + r * cellH, cellW - 18, cellH - 18);

        p.setPen(QPen(QColor(220, 220, 220), 1));
        p.drawRect(rect);

        QVector<double> y;
        y.reserve(wd.samples.size());
        for (const auto &row : wd.samples) {
            y.push_back(row[0]);
        }

        if (normalizeCheck->isChecked()) {
            double maxAbs = 0.0;
            for (double v : y) {
                maxAbs = std::max(maxAbs, std::abs(v));
            }
            if (maxAbs > 0.0) {
                for (double &v : y) {
                    v /= maxAbs;
                }
            }
        }

        double minV = y[0], maxV = y[0];
        for (int k = 1; k < y.size(); ++k) {
            minV = std::min(minV, y[k]);
            maxV = std::max(maxV, y[k]);
        }
        double range = (maxV - minV < 1e-12) ? 1.0 : (maxV - minV);

        p.setPen(QPen(QColor(31,119,180), 1));
        QPointF prev;
        bool started = false;
        for (int k = 0; k < y.size(); ++k) {
            const int denom = std::max(1, static_cast<int>(y.size()) - 1);
            const double x = rect.left() + (static_cast<double>(k) / denom) * rect.width();
            const double yn = (y[k] - minV) / range;
            const double yy = rect.bottom() - yn * rect.height();
            QPointF curr(x, yy);
            if (started) {
                p.drawLine(prev, curr);
            }
            prev = curr;
            started = true;
        }

        p.setPen(Qt::darkGray);
        p.setFont(QFont("Arial", 9));
        p.drawText(QRect(rect.left() + 4, rect.top() + 4, rect.width() - 8, 16),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QFileInfo(currentFiles[i]).fileName());
    }

    if (!image.save(savePath, "PNG")) {
        QMessageBox::warning(this, "Export", "Failed to save overview image.");
        return;
    }

    statusBar()->showMessage(QString("Saved overview: %1").arg(QDir::toNativeSeparators(savePath)), 5000);
}

bool MainWindow::buildStackedTraces(QVector<QVector<double>> &traces, QStringList &traceNames) const {
    traces.clear();
    traceNames.clear();

    if (currentFiles.isEmpty()) {
        return false;
    }

    for (const QString &filePath : currentFiles) {
        WaveData wd;
        QString err;
        if (!WaveformReader::readWaveFile(filePath, wd, err) || wd.samples.isEmpty()) {
            continue;
        }

        QVector<double> trace;
        trace.reserve(wd.samples.size());
        for (const auto &row : wd.samples) {
            trace.push_back(row[0]);
        }

        if (normalizeCheck->isChecked() && !trace.isEmpty()) {
            double maxAbs = 0.0;
            for (double v : trace) {
                maxAbs = std::max(maxAbs, std::abs(v));
            }
            if (maxAbs > 0.0) {
                for (double &v : trace) {
                    v /= maxAbs;
                }
            }
        }

        traces.push_back(trace);
        traceNames.push_back(QFileInfo(filePath).fileName());
    }

    return !traces.isEmpty();
}

void MainWindow::previewStacked() {
    if (currentFiles.isEmpty()) {
        QMessageBox::information(this, "Stacked", "No files loaded.");
        return;
    }

    QVector<QVector<double>> traces;
    QStringList names;
    if (!buildStackedTraces(traces, names)) {
        QMessageBox::warning(this, "Stacked", "Failed to build stacked traces from current folder.");
        return;
    }

    waveWidget->setStackedData(traces, names);
    statusBar()->showMessage(QString("Stacked preview loaded. traces=%1").arg(traces.size()), 5000);
}

void MainWindow::exportStackedPng() {
    if (currentFiles.isEmpty()) {
        QMessageBox::information(this, "Export", "No files loaded.");
        return;
    }

    const QString savePath = QFileDialog::getSaveFileName(this, "Save stacked PNG", "all_waveforms_stacked.png", "PNG (*.png)");
    if (savePath.isEmpty()) {
        return;
    }

    QVector<QVector<double>> traces;
    QStringList names;
    if (!buildStackedTraces(traces, names)) {
        QMessageBox::warning(this, "Export", "Failed to build stacked traces from current folder.");
        return;
    }

    WaveformWidget exportWidget;
    exportWidget.setStackedData(traces, names);
    const int outW = 2200;
    const int outH = std::max(1000, 160 + static_cast<int>(traces.size()) * 30);

    if (!exportWidget.saveAsPng(savePath, outW, outH)) {
        QMessageBox::warning(this, "Export", "Failed to save stacked image.");
        return;
    }

    statusBar()->showMessage(QString("Saved stacked image: %1").arg(QDir::toNativeSeparators(savePath)), 5000);
}
