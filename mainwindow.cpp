#include "mainwindow.h"
#include "spectrogramwidget.h"
#include "spectrumwidget.h"
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
#include <QListWidgetItem>
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
#include <QSplitter>
#include <QTimer>
#include <map>

namespace {
constexpr int kSampleRateHz = 4000;
constexpr int kMaxStackedCount = 10;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      folderEdit(new QLineEdit(this)),
      browseButton(new QPushButton("Browse", this)),
      loadButton(new QPushButton("Load Folder", this)),
      recursiveCheck(new QCheckBox("Recursive", this)),
      normalizeCheck(new QCheckBox("Normalize (overview)", this)),
            folderStackedCheck(new QCheckBox("Folder select => stacked preview (<=10)", this)),
            showSpectrumCheck(new QCheckBox("Show Spectrum", this)),
            showSpectrogramCheck(new QCheckBox("Show Time-Frequency", this)),
        folderList(new QListWidget(this)),
        fileList(new QListWidget(this)),
      infoLabel(new QLabel(this)),
      waveWidget(new WaveformWidget(this)),
            spectrumWidget(new SpectrumWidget(this)),
            spectrogramWidget(new SpectrogramWidget(this)),
      saveCurrentButton(new QPushButton("Save Current PNG", this)),
            saveOverviewButton(new QPushButton("Save Overview PNG", this)),
            previewStackedButton(new QPushButton("Preview Stacked", this)),
            saveStackedButton(new QPushButton("Save Stacked PNG", this)) {

    setWindowTitle("Qt Waveform Viewer");
    resize(1300, 820);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(5, 5, 5, 5);

    auto *topControlWidget = new QWidget(central);
    auto *topControlLayout = new QVBoxLayout(topControlWidget);
    topControlLayout->setContentsMargins(0, 0, 0, 8);

    auto *folderRow = new QHBoxLayout();
    folderEdit->setPlaceholderText("Input folder path here");
    folderRow->addWidget(new QLabel("Folder:"));
    folderRow->addWidget(folderEdit, 1);
    folderRow->addWidget(browseButton);
    folderRow->addWidget(loadButton);

    // Using layout and splitter natively without minimum size constraints allowing 15% ratio
    folderList->setMinimumWidth(50);
    fileList->setMinimumWidth(50);

    folderStackedCheck->setChecked(true);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color:#555;font-size:12px;");

    auto *optRow = new QHBoxLayout();
    optRow->addWidget(recursiveCheck);
    optRow->addWidget(normalizeCheck);
    optRow->addWidget(folderStackedCheck);
    optRow->addWidget(showSpectrumCheck);
    optRow->addWidget(showSpectrogramCheck);
    optRow->addStretch();

    auto *saveRow = new QHBoxLayout();
    saveRow->addWidget(saveCurrentButton);
    saveRow->addWidget(saveOverviewButton);
    saveRow->addWidget(previewStackedButton);
    saveRow->addWidget(saveStackedButton);
    saveRow->addStretch();

    auto *treeRow = new QHBoxLayout();
    auto *folderPane = new QVBoxLayout();
    auto *filePane = new QVBoxLayout();

    folderPane->addWidget(new QLabel("Folders:"));
    folderPane->addWidget(folderList, 1);

    filePane->addWidget(new QLabel("Waveform Files:"));
    filePane->addWidget(fileList, 1);

    treeRow->addLayout(folderPane, 1);
    treeRow->addLayout(filePane, 1);

    topControlLayout->addLayout(folderRow);
    topControlLayout->addLayout(optRow);
    topControlLayout->addLayout(saveRow);

    root->addWidget(topControlWidget);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    root->addWidget(splitter, 1);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    leftLayout->addLayout(treeRow, 1);
    leftLayout->addWidget(new QLabel("Info:"));
    leftLayout->addWidget(infoLabel);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(new QLabel("Waveform:"));
    rightLayout->addWidget(waveWidget, 3);
    rightLayout->addWidget(spectrumWidget, 1);
    rightLayout->addWidget(spectrogramWidget, 2);

    spectrumWidget->setVisible(false);
    spectrogramWidget->setVisible(false);

    splitter->addWidget(left);
    splitter->addWidget(right);
    
    // Adjusted width: smaller left side
    left->setMaximumWidth(350);
    splitter->setSizes({150, 1150});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(central);
    statusBar()->showMessage("Ready");

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseFolder);
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadFolder);
    connect(folderEdit, &QLineEdit::returnPressed, this, &MainWindow::loadFolder);
    connect(folderList, &QListWidget::itemClicked, this, &MainWindow::onFolderItemClicked);
    connect(fileList, &QListWidget::itemClicked, this, &MainWindow::onFileItemClicked);
    connect(waveWidget, &WaveformWidget::viewWindowChanged, this, &MainWindow::onWaveViewChanged);
    connect(showSpectrumCheck, &QCheckBox::toggled, this, &MainWindow::onShowSpectrumChanged);
    connect(showSpectrogramCheck, &QCheckBox::toggled, this, &MainWindow::onShowSpectrogramChanged);
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

    currentFolder = fi.absoluteFilePath();
    currentFiles = collectFiles(currentFolder, recursiveCheck->isChecked());
    populateFolderAndFileColumns(currentFolder, currentFiles);

    infoLabel->setText(QString("Loaded folder:\n%1\n\nFiles found: %2")
        .arg(QDir::toNativeSeparators(fi.absoluteFilePath()))
        .arg(currentFiles.size()));

    statusBar()->showMessage(QString("Folder loaded. Files: %1").arg(currentFiles.size()));

    if (!currentFiles.isEmpty()) {
        populateFilesForFolder(currentFolder);
        
        if (folderStackedCheck->isChecked()) {
            if (currentFiles.size() <= kMaxStackedCount) {
                previewStacked();
            } else {
                statusBar()->showMessage(
                    QString("Folder files=%1 > %2, skip automatic stacked preview.")
                        .arg(currentFiles.size())
                        .arg(kMaxStackedCount),
                    5000);
                loadSingleFile(currentFiles.first());
            }
        } else {
            loadSingleFile(currentFiles.first());
        }
    } else {
        waveWidget->clearData();
        currentFilePath.clear();
        currentSignal.clear();
        refreshAnalysisWidgets();
        fileList->clear();
    }
}

void MainWindow::populateFolderAndFileColumns(const QString &folder, const QStringList &files) {
    folderList->clear();
    fileList->clear();
    folderFiles.clear();

    folderFiles[folder] = files;

    QDir currentDir(folder);
    QDir parentDir = currentDir;
    // Get the parent directory of the current folder to list its siblings
    if (!parentDir.isRoot()) {
        parentDir.cdUp();
    }

    parentDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    parentDir.setSorting(QDir::Name);
    QFileInfoList siblings = parentDir.entryInfoList();

    // The left folder shows all folder names in the parent directory
    for (const QFileInfo &sibling : siblings) {
        const QString absPath = sibling.absoluteFilePath();
        auto *item = new QListWidgetItem(sibling.fileName());
        item->setData(Qt::UserRole, absPath);
        folderList->addItem(item);
        
        // Select the currently loaded folder in the list
        if (absPath == currentDir.absolutePath()) {
            folderList->setCurrentItem(item);
        }
    }
}

void MainWindow::populateFilesForFolder(const QString &folderPath) {
    currentFolderPath = folderPath;
    // We already have the currentFiles populated by loadFolder, so just use them
    fileList->clear();

    for (const QString &filePath : currentFiles) {
        auto *item = new QListWidgetItem(QFileInfo(filePath).fileName());
        item->setToolTip(QDir::toNativeSeparators(filePath));
        item->setData(Qt::UserRole, filePath);
        fileList->addItem(item);
    }

    if (!currentFiles.isEmpty()) {
        fileList->setCurrentRow(0);
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

    currentFilePath = filePath;
    currentSignal.clear();
    currentSignal.reserve(wd.samples.size());
    for (const auto &row : wd.samples) {
        currentSignal.push_back(row.isEmpty() ? 0.0 : row[0]);
    }

    waveWidget->setData(wd.samples, channelNames);
    currentViewStart = waveWidget->visibleStartSample();
    currentViewEnd = waveWidget->visibleEndSample();
    refreshAnalysisWidgets();

    statusBar()->showMessage(QString("Loaded: %1  shape=(%2,%3)")
        .arg(QFileInfo(filePath).fileName())
        .arg(wd.samples.size())
        .arg(wd.samples.isEmpty() ? 0 : wd.samples[0].size()));
    return true;
}

void MainWindow::onFolderItemClicked(QListWidgetItem *item) {
    if (!item) {
        return;
    }

    const QString folderPath = item->data(Qt::UserRole).toString();
    if (folderPath.isEmpty() || folderPath == currentFolder) {
        return;
    }

    // Set the folder path and asynchronously call loadFolder to avoid destroying items during click event
    folderEdit->setText(QDir::toNativeSeparators(folderPath));
    QTimer::singleShot(0, this, &MainWindow::loadFolder);
}

void MainWindow::onFileItemClicked(QListWidgetItem *item) {
    if (!item) {
        return;
    }

    const QString filePath = item->data(Qt::UserRole).toString();
    if (!filePath.isEmpty()) {
        loadSingleFile(filePath);
    }
}

void MainWindow::onWaveViewChanged(int startSample, int endSample) {
    currentViewStart = startSample;
    currentViewEnd = endSample;
    refreshAnalysisWidgets();
}

void MainWindow::onShowSpectrumChanged(bool checked) {
    spectrumWidget->setVisible(checked);
    refreshAnalysisWidgets();
}

void MainWindow::onShowSpectrogramChanged(bool checked) {
    spectrogramWidget->setVisible(checked);
    refreshAnalysisWidgets();
}

void MainWindow::refreshAnalysisWidgets() {
    if (showSpectrumCheck->isChecked()) {
        spectrumWidget->setSignal(currentSignal, kSampleRateHz);
        spectrumWidget->setViewRange(currentViewStart, currentViewEnd);
    }

    if (showSpectrogramCheck->isChecked()) {
        spectrogramWidget->setSignal(currentSignal, kSampleRateHz);
        spectrogramWidget->setViewRange(currentViewStart, currentViewEnd);
    }
}

void MainWindow::exportCurrentPng() {
    if (currentFilePath.isEmpty()) {
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

    if (currentFiles.size() > kMaxStackedCount) {
        QMessageBox::information(this,
                                 "Stacked",
                                 QString("Stacked preview is limited to %1 files. Current files: %2")
                                     .arg(kMaxStackedCount)
                                     .arg(currentFiles.size()));
        return;
    }

    QVector<QVector<double>> traces;
    QStringList names;
    if (!buildStackedTraces(traces, names)) {
        QMessageBox::warning(this, "Stacked", "Failed to build stacked traces from current folder.");
        return;
    }

    waveWidget->setStackedData(traces, names);
    currentFilePath.clear();
    currentSignal.clear();
    refreshAnalysisWidgets();
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
