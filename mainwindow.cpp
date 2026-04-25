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
#include <QSplitter>
#include <QTimer>
#include <QTabWidget>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kSampleRateHz = 4000;
constexpr int kMaxStackedCount = 10;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      modeTabs(new QTabWidget(this)),
      eventFolderEdit(new QLineEdit(this)),
      browseEventButton(new QPushButton("Browse", this)),
      loadEventButton(new QPushButton("Load Folder", this)),
      dataEventFolderEdit(new QLineEdit(this)),
      browseDataEventButton(new QPushButton("Browse Event", this)),
      loadDataEventButton(new QPushButton("Load Event", this)),
      dataFolderEdit(new QLineEdit(this)),
      browseDataButton(new QPushButton("Browse Data", this)),
      recursiveCheck(new QCheckBox("Recursive", this)),
      normalizeCheck(new QCheckBox("Normalize (overview)", this)),
      folderStackedCheck(new QCheckBox("Folder select => stacked preview (<=10)", this)),
      findDataCheck(new QCheckBox("Find corresponding Data (prioritized)", this)),
      showSpectrumCheck(new QCheckBox("Show Spectrum", this)),
      showSpectrogramCheck(new QCheckBox("Show Time-Frequency", this)),
      eventFolderList(new QListWidget(this)),
      eventFileList(new QListWidget(this)),
      dataFolderList(new QListWidget(this)),
      dataFileList(new QListWidget(this)),
      eventInfoLabel(new QLabel(this)),
      dataInfoLabel(new QLabel(this)),
      waveWidget(new WaveformWidget(this)),
      spectrumWidget(new SpectrumWidget(this)),
      spectrogramWidget(new SpectrogramWidget(this)),
      saveCurrentButton(new QPushButton("Save Current PNG", this)),
    saveOverviewButton(new QPushButton("Save Overview PNG", this)),
    previewStackedButton(new QPushButton("Preview Stacked", this)),
    saveStackedButton(new QPushButton("Save Stacked PNG", this)),
    showFullButton(new QPushButton("Show Full Waveform", this)) {

    setWindowTitle("Qt Waveform Viewer");
    resize(1300, 820);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(5, 5, 5, 5);

    auto *topControlWidget = new QWidget(central);
    auto *topControlLayout = new QVBoxLayout(topControlWidget);
    topControlLayout->setContentsMargins(0, 0, 0, 8);

    auto *globalRow = new QHBoxLayout();
    globalRow->addWidget(showSpectrumCheck);
    globalRow->addWidget(showSpectrogramCheck);
    globalRow->addStretch();

    auto *saveRow = new QHBoxLayout();
    saveRow->addWidget(saveCurrentButton);
    saveRow->addWidget(saveOverviewButton);
    saveRow->addWidget(previewStackedButton);
    saveRow->addWidget(saveStackedButton);
    saveRow->addWidget(showFullButton);
    saveRow->addStretch();

    topControlLayout->addLayout(globalRow);
    topControlLayout->addLayout(saveRow);
    root->addWidget(topControlWidget);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    root->addWidget(splitter, 1);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto *eventPage = new QWidget(left);
    auto *eventLayout = new QVBoxLayout(eventPage);
    eventLayout->setContentsMargins(0, 0, 0, 0);

    auto *eventFolderRow = new QHBoxLayout();
    eventFolderEdit->setPlaceholderText("Input Event folder path here");
    eventFolderRow->addWidget(new QLabel("Event Folder:"));
    eventFolderRow->addWidget(eventFolderEdit, 1);
    eventFolderRow->addWidget(browseEventButton);
    eventFolderRow->addWidget(loadEventButton);

    auto *eventOptRow = new QHBoxLayout();
    eventOptRow->addWidget(recursiveCheck);
    eventOptRow->addWidget(normalizeCheck);
    eventOptRow->addWidget(folderStackedCheck);
    eventOptRow->addStretch();

    auto *eventTreeRow = new QHBoxLayout();
    auto *eventFolderPane = new QVBoxLayout();
    auto *eventFilePane = new QVBoxLayout();
    eventFolderPane->addWidget(new QLabel("Folders:"));
    eventFolderPane->addWidget(eventFolderList, 1);
    eventFilePane->addWidget(new QLabel("Waveform Files:"));
    eventFilePane->addWidget(eventFileList, 1);
    eventTreeRow->addLayout(eventFolderPane, 1);
    eventTreeRow->addLayout(eventFilePane, 1);

    eventInfoLabel->setWordWrap(true);
    eventInfoLabel->setStyleSheet("color:#555;font-size:12px;");

    eventLayout->addLayout(eventFolderRow);
    eventLayout->addLayout(eventOptRow);
    eventLayout->addLayout(eventTreeRow, 1);
    eventLayout->addWidget(new QLabel("Info:"));
    eventLayout->addWidget(eventInfoLabel);

    auto *dataPage = new QWidget(left);
    auto *dataLayout = new QVBoxLayout(dataPage);
    dataLayout->setContentsMargins(0, 0, 0, 0);

    auto *dataEventRow = new QHBoxLayout();
    dataEventFolderEdit->setPlaceholderText("Input Event folder path here");
    dataEventRow->addWidget(new QLabel("Event Folder:"));
    dataEventRow->addWidget(dataEventFolderEdit, 1);
    dataEventRow->addWidget(browseDataEventButton);
    dataEventRow->addWidget(loadDataEventButton);

    auto *dataFolderRow = new QHBoxLayout();
    dataFolderEdit->setPlaceholderText("Input Data root folder here (e.g. E:\\GMS...\\Data)");
    dataFolderRow->addWidget(new QLabel("Data Folder:"));
    dataFolderRow->addWidget(dataFolderEdit, 1);
    dataFolderRow->addWidget(browseDataButton);

    auto *dataOptRow = new QHBoxLayout();
    dataOptRow->addWidget(findDataCheck);
    dataOptRow->addStretch();

    auto *dataTreeRow = new QHBoxLayout();
    auto *dataFolderPane = new QVBoxLayout();
    auto *dataFilePane = new QVBoxLayout();
    dataFolderPane->addWidget(new QLabel("Folders:"));
    dataFolderPane->addWidget(dataFolderList, 1);
    dataFilePane->addWidget(new QLabel("Waveform Files:"));
    dataFilePane->addWidget(dataFileList, 1);
    dataTreeRow->addLayout(dataFolderPane, 1);
    dataTreeRow->addLayout(dataFilePane, 1);

    dataInfoLabel->setWordWrap(true);
    dataInfoLabel->setStyleSheet("color:#555;font-size:12px;");

    dataLayout->addLayout(dataEventRow);
    dataLayout->addLayout(dataFolderRow);
    dataLayout->addLayout(dataOptRow);
    dataLayout->addLayout(dataTreeRow, 1);
    dataLayout->addWidget(new QLabel("Info:"));
    dataLayout->addWidget(dataInfoLabel);

    eventFolderList->setMinimumWidth(50);
    eventFileList->setMinimumWidth(50);
    dataFolderList->setMinimumWidth(50);
    dataFileList->setMinimumWidth(50);

    folderStackedCheck->setChecked(true);
    findDataCheck->setChecked(true);

    modeTabs->addTab(eventPage, "Event View");
    modeTabs->addTab(dataPage, "Data View");
    leftLayout->addWidget(modeTabs, 1);

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
    left->setMaximumWidth(380);
    splitter->setSizes({200, 1100});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(central);
    statusBar()->showMessage("Ready");

    connect(browseEventButton, &QPushButton::clicked, this, &MainWindow::browseEventFolder);
    connect(loadEventButton, &QPushButton::clicked, this, &MainWindow::loadEventFolder);
    connect(eventFolderEdit, &QLineEdit::returnPressed, this, &MainWindow::loadEventFolder);
    connect(eventFolderList, &QListWidget::itemClicked, this, &MainWindow::onEventFolderItemClicked);
    connect(eventFileList, &QListWidget::itemClicked, this, &MainWindow::onEventFileItemClicked);

    connect(browseDataEventButton, &QPushButton::clicked, this, &MainWindow::browseDataEventFolder);
    connect(loadDataEventButton, &QPushButton::clicked, this, &MainWindow::loadDataFolder);
    connect(dataEventFolderEdit, &QLineEdit::returnPressed, this, &MainWindow::loadDataFolder);
    connect(browseDataButton, &QPushButton::clicked, this, &MainWindow::browseDataFolder);
    connect(dataFolderList, &QListWidget::itemClicked, this, &MainWindow::onDataFolderItemClicked);
    connect(dataFileList, &QListWidget::itemClicked, this, &MainWindow::onDataFileItemClicked);

    connect(waveWidget, &WaveformWidget::viewWindowChanged, this, &MainWindow::onWaveViewChanged);
    connect(showSpectrumCheck, &QCheckBox::toggled, this, &MainWindow::onShowSpectrumChanged);
    connect(showSpectrogramCheck, &QCheckBox::toggled, this, &MainWindow::onShowSpectrogramChanged);
    connect(saveCurrentButton, &QPushButton::clicked, this, &MainWindow::exportCurrentPng);
    connect(saveOverviewButton, &QPushButton::clicked, this, &MainWindow::exportOverviewPng);
    connect(previewStackedButton, &QPushButton::clicked, this, &MainWindow::previewStacked);
    connect(saveStackedButton, &QPushButton::clicked, this, &MainWindow::exportStackedPng);
    connect(showFullButton, &QPushButton::clicked, this, &MainWindow::showFullWaveform);
}

MainWindow::~MainWindow() = default;

void MainWindow::browseEventFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Event folder", eventFolderEdit->text().trimmed());
    if (!dir.isEmpty()) {
        const QString native = QDir::toNativeSeparators(dir);
        eventFolderEdit->setText(native);
        if (dataEventFolderEdit->text().trimmed().isEmpty()) {
            dataEventFolderEdit->setText(native);
        }
        loadEventFolder();
    }
}

void MainWindow::browseDataEventFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Event folder", dataEventFolderEdit->text().trimmed());
    if (!dir.isEmpty()) {
        dataEventFolderEdit->setText(QDir::toNativeSeparators(dir));
        if (findDataCheck->isChecked() && !dataFolderEdit->text().trimmed().isEmpty()) {
            loadDataFolder();
        }
    }
}

void MainWindow::browseDataFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Data folder", dataFolderEdit->text().trimmed());
    if (!dir.isEmpty()) {
        dataFolderEdit->setText(QDir::toNativeSeparators(dir));
        if (!dataEventFolderEdit->text().trimmed().isEmpty()) {
            loadDataFolder();
        }
    }
}

QStringList MainWindow::collectFiles(const QString &folder, bool recursive) const {
    QStringList results;
    const QStringList exts = {"event", "evt", "dat", "bin", "csv", "txt"};

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

QStringList MainWindow::resolveDataFiles(const QString &eventFolder,
                                         const QString &dataRoot,
                                         QMap<QString, QString> &fileSensors,
                                         QString &projectName,
                                         QDateTime &eventTime) const {
    QStringList results;
    fileSensors.clear();
    projectName.clear();
    eventTime = QDateTime();

    QDir eventDir(eventFolder);
    const QString eventDirName = eventDir.dirName();

    // Prefer parsing project name from .../Event/<project>/... path layout.
    const QString normalizedEventPath = QDir::fromNativeSeparators(eventDir.absolutePath());
    const QStringList segs = normalizedEventPath.split('/', Qt::SkipEmptyParts);
    int eventIdx = -1;
    for (int i = 0; i < segs.size(); ++i) {
        if (segs[i].compare("Event", Qt::CaseInsensitive) == 0) {
            eventIdx = i;
            break;
        }
    }
    if (eventIdx >= 0 && eventIdx + 1 < segs.size()) {
        projectName = segs[eventIdx + 1];
    } else {
        QDir projectDir = eventDir;
        projectDir.cdUp();
        projectDir.cdUp();
        projectDir.cdUp();
        projectDir.cdUp();
        projectName = projectDir.dirName();
    }

    const QRegularExpression evRe("^(\\d{4})\\.(\\d{2})\\.(\\d{2})\\.(\\d{2})\\.(\\d{2})(?:\\.(\\d{2})(?:\\.(\\d{3})(?:-\\d+)?)?)?$");
    const QRegularExpressionMatch evMatch = evRe.match(eventDirName);
    if (!evMatch.hasMatch()) {
        return results;
    }

    const int y = evMatch.captured(1).toInt();
    const int mon = evMatch.captured(2).toInt();
    const int d = evMatch.captured(3).toInt();
    const int h = evMatch.captured(4).toInt();
    const int m = evMatch.captured(5).toInt();
    const int s = evMatch.captured(6).isEmpty() ? 0 : evMatch.captured(6).toInt();
    const int ms = evMatch.captured(7).isEmpty() ? 0 : evMatch.captured(7).toInt();
    eventTime = QDateTime(QDate(y, mon, d), QTime(h, m, s, ms));

    const QString yyyy = QString::number(y);
    const QString mmPad = QString("%1").arg(mon, 2, 10, QChar('0'));
    const QString mmNoPad = QString::number(mon);
    const QString ddPad = QString("%1").arg(d, 2, 10, QChar('0'));
    const QString ddNoPad = QString::number(d);
    const QString hourNoPad = QString::number(h);
    const QString hourPad = QString("%1").arg(h, 2, 10, QChar('0'));
    const QString minutePad = QString("%1").arg(m, 2, 10, QChar('0'));
    const QString minuteNoPad = QString::number(m);

    QDir baseDataDir(dataRoot);
    if (!baseDataDir.exists() || !baseDataDir.cd(projectName)) {
        return results;
    }

    const QFileInfoList sensorFolders = baseDataDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList allSensorDirs;
    QMap<QString, QString> sensorDirByLower;
    for (const QFileInfo &sensorFi : sensorFolders) {
        const QString dirName = sensorFi.fileName();
        allSensorDirs.push_back(dirName);
        sensorDirByLower.insert(dirName.toLower(), dirName);
    }

    auto scanSensors = [&](const QStringList &sensorDirNames) {
        for (const QString &sensorId : sensorDirNames) {
        QDir hDir = baseDataDir;
        if (!hDir.cd(sensorId)) continue;
        if (!hDir.cd(yyyy)) continue;
        if (!hDir.cd(mmPad) && !hDir.cd(mmNoPad)) continue;
        if (!hDir.cd(ddPad) && !hDir.cd(ddNoPad)) continue;
        if (!hDir.cd(hourNoPad) && !hDir.cd(hourPad)) continue;

            QStringList filters;
            filters << minutePad + "*" << minuteNoPad + "*";
            hDir.setNameFilters(filters);
            const QFileInfoList minuteFiles = hDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

            for (const QFileInfo &mFi : minuteFiles) {
                const QString base = mFi.baseName();
                const QString file = mFi.fileName();
                if (base == minutePad || base == minuteNoPad ||
                    file == minutePad || file == minuteNoPad ||
                    base.startsWith(minutePad) || base.startsWith(minuteNoPad) ||
                    file.startsWith(minutePad) || file.startsWith(minuteNoPad)) {
                const QString absPath = mFi.absoluteFilePath();
                if (!results.contains(absPath)) {
                    results.push_back(absPath);
                    fileSensors.insert(absPath, sensorId);
                }
            }
        }
        }
    };

    // Scan all sensor folders under project to find all available data files
    // This ensures we find all sensors, not just those in event files
    scanSensors(allSensorDirs);

    return results;
}

void MainWindow::loadEventFolder() {
    QString folder = eventFolderEdit->text().trimmed();
    if (folder.startsWith('"') && folder.endsWith('"') && folder.size() >= 2) {
        folder = folder.mid(1, folder.size() - 2).trimmed();
    }

    if (folder.isEmpty()) {
        QMessageBox::warning(this, "Input", "Please input an Event folder path.");
        return;
    }

    QFileInfo fi(folder);
    if (!fi.exists() || !fi.isDir()) {
        QMessageBox::warning(this, "Input", QString("Invalid folder: %1").arg(folder));
        return;
    }

    currentFolder = fi.absoluteFilePath();
    currentFiles = collectFiles(currentFolder, recursiveCheck->isChecked());

    populateFolderAndFileColumns(eventFolderList, eventFileList, currentFolder, currentFiles);

    eventInfoLabel->setText(QString("Loaded Event folder:\n%1\n\nFiles found: %2")
        .arg(QDir::toNativeSeparators(currentFolder))
        .arg(currentFiles.size()));

    statusBar()->showMessage(QString("Event folder loaded. Files: %1").arg(currentFiles.size()));

    if (!currentFiles.isEmpty()) {
        populateFilesForFolder(eventFileList, currentFiles);

        if (folderStackedCheck->isChecked()) {
            if (currentFiles.size() <= kMaxStackedCount) {
                previewStacked();
            } else {
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
        eventFileList->clear();
    }
}

void MainWindow::loadDataFolder() {
    dataFileSensors.clear();
    currentDataSensorId.clear();

    if (findDataCheck->isChecked()) {
        QString eventFolder = dataEventFolderEdit->text().trimmed();
        if (eventFolder.startsWith('"') && eventFolder.endsWith('"') && eventFolder.size() >= 2) {
            eventFolder = eventFolder.mid(1, eventFolder.size() - 2).trimmed();
        }
        if (eventFolder.isEmpty()) {
            QMessageBox::warning(this, "Input", "Please input Event folder in Data tab.");
            return;
        }

        QFileInfo eventFi(eventFolder);
        if (!eventFi.exists() || !eventFi.isDir()) {
            QMessageBox::warning(this, "Input", QString("Invalid Event folder: %1").arg(eventFolder));
            return;
        }

        const QString dataRoot = dataFolderEdit->text().trimmed();
        if (dataRoot.isEmpty()) {
            QMessageBox::warning(this, "Input", "Please input Data root folder.");
            return;
        }

        currentFolder = eventFi.absoluteFilePath();
        const QRegularExpression evRe("^(\\d{4})\\.(\\d{2})\\.(\\d{2})\\.(\\d{2})\\.(\\d{2})(?:\\.(\\d{2})(?:\\.(\\d{3})(?:-\\d+)?)?)?$");
        const bool isEventTimestampFolder = evRe.match(QDir(currentFolder).dirName()).hasMatch();

        if (isEventTimestampFolder) {
            currentFiles = resolveDataFiles(currentFolder, dataRoot, dataFileSensors, currentDataProject, currentEventTime);
        } else {
            currentFiles.clear();
            currentDataProject.clear();
            currentEventTime = QDateTime();
        }
        currentDataRoot = dataRoot;

        populateFolderAndFileColumns(dataFolderList, dataFileList, currentFolder, currentFiles);

        QString infoText = QString("Loaded Event folder for Data lookup:\n%1\nData Root: %2\n\nData files found: %3")
            .arg(QDir::toNativeSeparators(currentFolder))
            .arg(QDir::toNativeSeparators(dataRoot))
            .arg(currentFiles.size());
        if (!isEventTimestampFolder) {
            infoText += "\nHint: Please select one Event timestamp folder in left list.";
        } else if (currentFiles.isEmpty()) {
            infoText += "\nHint: Event folder should be timestamp folder, Data folder should point to .../Data";
        }
        dataInfoLabel->setText(infoText);
    } else {
        QString dataFolder = dataFolderEdit->text().trimmed();
        if (dataFolder.startsWith('"') && dataFolder.endsWith('"') && dataFolder.size() >= 2) {
            dataFolder = dataFolder.mid(1, dataFolder.size() - 2).trimmed();
        }
        if (dataFolder.isEmpty()) {
            QMessageBox::warning(this, "Input", "Please input Data folder path.");
            return;
        }

        QFileInfo dataFi(dataFolder);
        if (!dataFi.exists() || !dataFi.isDir()) {
            QMessageBox::warning(this, "Input", QString("Invalid Data folder: %1").arg(dataFolder));
            return;
        }

        currentFolder = dataFi.absoluteFilePath();
        currentFiles = collectFiles(currentFolder, recursiveCheck->isChecked());
        currentDataRoot.clear();
        currentDataProject.clear();
        currentEventTime = QDateTime();

        populateFolderAndFileColumns(dataFolderList, dataFileList, currentFolder, currentFiles);

        dataInfoLabel->setText(QString("Loaded Data folder:\n%1\n\nFiles found: %2")
            .arg(QDir::toNativeSeparators(currentFolder))
            .arg(currentFiles.size()));
    }

    statusBar()->showMessage(QString("Data view loaded. Files: %1").arg(currentFiles.size()));

    if (!currentFiles.isEmpty()) {
        populateFilesForFolder(dataFileList, currentFiles);

        if (folderStackedCheck->isChecked()) {
            if (currentFiles.size() <= kMaxStackedCount) {
                previewStacked();
            } else {
                if (findDataCheck->isChecked()) {
                    const QString firstSensor = dataFileSensors.value(currentFiles.first());
                    if (!firstSensor.isEmpty() && loadDataEventWindow(currentFiles.first(), firstSensor)) {
                        return;
                    }
                }
                loadSingleFile(currentFiles.first());
            }
        } else {
            if (findDataCheck->isChecked()) {
                const QString firstSensor = dataFileSensors.value(currentFiles.first());
                if (!firstSensor.isEmpty() && loadDataEventWindow(currentFiles.first(), firstSensor)) {
                    return;
                }
            }
            loadSingleFile(currentFiles.first());
        }
    } else {
        waveWidget->clearData();
        currentFilePath.clear();
        currentSignal.clear();
        refreshAnalysisWidgets();
        dataFileList->clear();
    }
}

void MainWindow::populateFolderAndFileColumns(QListWidget *folderListWidget,
                                              QListWidget *fileListWidget,
                                              const QString &folder,
                                              const QStringList &files) {
    folderListWidget->clear();
    fileListWidget->clear();
    folderFiles.clear();

    folderFiles[folder] = files;

    QDir currentDir(folder);

    bool listedAny = false;
    if (isDataMode() && findDataCheck->isChecked()) {
        // In Data mode, prioritize showing Event subfolders on the left.
        const QFileInfoList children = currentDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &child : children) {
            const QString absPath = child.absoluteFilePath();
            auto *item = new QListWidgetItem(child.fileName());
            item->setData(Qt::UserRole, absPath);
            folderListWidget->addItem(item);
            listedAny = true;
        }
    }

    if (!listedAny) {
        QDir parentDir = currentDir;
        if (!parentDir.isRoot()) {
            parentDir.cdUp();
        }

        const QFileInfoList siblings = parentDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &sibling : siblings) {
            const QString absPath = sibling.absoluteFilePath();
            auto *item = new QListWidgetItem(sibling.fileName());
            item->setData(Qt::UserRole, absPath);
            folderListWidget->addItem(item);
            if (absPath == currentDir.absolutePath()) {
                folderListWidget->setCurrentItem(item);
            }
        }
    }
}

void MainWindow::populateFilesForFolder(QListWidget *fileListWidget, const QStringList &files) {
    fileListWidget->clear();

    for (const QString &filePath : files) {
        const bool showFullPath = isDataMode() && findDataCheck->isChecked();
        auto *item = new QListWidgetItem(showFullPath ? QDir::toNativeSeparators(filePath)
                                                      : QFileInfo(filePath).fileName());
        item->setToolTip(QDir::toNativeSeparators(filePath));
        item->setData(Qt::UserRole, filePath);
        if (dataFileSensors.contains(filePath)) {
            item->setData(Qt::UserRole + 1, dataFileSensors.value(filePath));
        }
        fileListWidget->addItem(item);
    }

    if (!files.isEmpty()) {
        fileListWidget->setCurrentRow(0);
    }
}

bool MainWindow::loadSingleFile(const QString &filePath) {
    WaveData wd;
    QString error;

    QFileInfo fi(filePath);
    const QString ext = fi.suffix().toLower();
    bool success = false;

    if (ext == "csv" || ext == "txt") {
        success = WaveformReader::readTextWaveFile(filePath, wd, error);
    } else {
        success = WaveformReader::readWaveFile(filePath, wd, error);
        if (!success) {
            QString err2;
            if (WaveformReader::readTextWaveFile(filePath, wd, err2)) {
                success = true;
            }
        }
    }

    if (!success) {
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

bool MainWindow::loadDataEventWindow(const QString &filePath, const QString &sensorId) {
    if (!currentEventTime.isValid() || currentDataRoot.isEmpty() || currentDataProject.isEmpty()) {
        return false;
    }

    auto readMinuteFile = [](const QString &path, WaveData &out, QString &err) -> bool {
        if (WaveformReader::readTextWaveFile(path, out, err)) {
            return true;
        }
        err.clear();
        return WaveformReader::readWaveFile(path, out, err);
    };

    WaveData curr;
    QString err;
    if (!readMinuteFile(filePath, curr, err)) {
        QMessageBox::warning(this, "Read error", err);
        return false;
    }

    const int sec = currentEventTime.time().second();
    const int ms = currentEventTime.time().msec();
    const int sampleIndex = sec * kSampleRateHz + (ms * kSampleRateHz) / 1000;

    const int pre = 1000;
    const int post = 3000;
    const int startIndex = sampleIndex - pre;
    const int endIndex = sampleIndex + post;

    auto findMinuteFile = [&](const QDateTime &dt) -> QString {
        QDir hDir(currentDataRoot);
        if (!hDir.cd(currentDataProject)) return {};
        if (!hDir.cd(sensorId)) return {};
        if (!hDir.cd(dt.date().toString("yyyy"))) return {};
        if (!hDir.cd(dt.date().toString("MM"))) return {};
        if (!hDir.cd(dt.date().toString("dd"))) return {};

        const QString hNoPad = QString::number(dt.time().hour());
        const QString hPad = dt.time().toString("HH");
        if (!hDir.cd(hNoPad) && !hDir.cd(hPad)) return {};

        const QString mPad = dt.time().toString("mm");
        const QString mNoPad = QString::number(dt.time().minute());
        QStringList filters;
        filters << mPad + ".*" << mPad << mNoPad + ".*" << mNoPad;
        hDir.setNameFilters(filters);
        const QFileInfoList files = hDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &fi : files) {
            if (fi.baseName() == mPad || fi.baseName() == mNoPad || fi.fileName() == mPad || fi.fileName() == mNoPad) {
                return fi.absoluteFilePath();
            }
        }
        return {};
    };

    WaveData prev;
    WaveData next;
    const int currCount = curr.samples.size();
    if (startIndex < 0) {
        const QString prevPath = findMinuteFile(currentEventTime.addSecs(-60));
        if (!prevPath.isEmpty()) {
            QString e;
            readMinuteFile(prevPath, prev, e);
        }
    }
    if (endIndex > currCount) {
        const QString nextPath = findMinuteFile(currentEventTime.addSecs(60));
        if (!nextPath.isEmpty()) {
            QString e;
            readMinuteFile(nextPath, next, e);
        }
    }

    const int channelCount = curr.isThreeComponent ? 3 : (curr.samples.isEmpty() ? 1 : curr.samples[0].size());
    const int outChannels = std::max(1, channelCount);

    QVector<QVector<double>> outSamples;
    outSamples.reserve(pre + post);

    for (int i = startIndex; i < endIndex; ++i) {
        QVector<double> row(outChannels, 0.0);
        if (i < 0) {
            const int idx = prev.samples.size() + i;
            if (idx >= 0 && idx < prev.samples.size()) {
                row = prev.samples[idx];
                row.resize(outChannels);
            }
        } else if (i < currCount) {
            row = curr.samples[i];
            row.resize(outChannels);
        } else {
            const int idx = i - currCount;
            if (idx >= 0 && idx < next.samples.size()) {
                row = next.samples[idx];
                row.resize(outChannels);
            }
        }
        outSamples.push_back(row);
    }

    WaveData wd;
    wd.samples = outSamples;
    wd.isThreeComponent = (outChannels == 3);

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

    statusBar()->showMessage(QString("Loaded event window: %1  shape=(%2,%3)")
        .arg(QFileInfo(filePath).fileName())
        .arg(wd.samples.size())
        .arg(wd.samples.isEmpty() ? 0 : wd.samples[0].size()));
    return true;
}

void MainWindow::onEventFolderItemClicked(QListWidgetItem *item) {
    if (!item) return;

    const QString folderPath = item->data(Qt::UserRole).toString();
    if (folderPath.isEmpty() || folderPath == currentFolder) return;

    eventFolderEdit->setText(QDir::toNativeSeparators(folderPath));
    QTimer::singleShot(0, this, &MainWindow::loadEventFolder);
}

void MainWindow::onDataFolderItemClicked(QListWidgetItem *item) {
    if (!item) return;

    const QString folderPath = item->data(Qt::UserRole).toString();
    if (folderPath.isEmpty() || folderPath == currentFolder) return;

    if (findDataCheck->isChecked()) {
        dataEventFolderEdit->setText(QDir::toNativeSeparators(folderPath));
    } else {
        dataFolderEdit->setText(QDir::toNativeSeparators(folderPath));
    }
    QTimer::singleShot(0, this, &MainWindow::loadDataFolder);
}

void MainWindow::onEventFileItemClicked(QListWidgetItem *item) {
    if (!item) return;

    const QString filePath = item->data(Qt::UserRole).toString();
    if (!filePath.isEmpty()) {
        loadSingleFile(filePath);
    }
}

void MainWindow::onDataFileItemClicked(QListWidgetItem *item) {
    if (!item) return;

    const QString filePath = item->data(Qt::UserRole).toString();
    currentDataSensorId = item->data(Qt::UserRole + 1).toString();

    if (filePath.isEmpty()) {
        return;
    }

    if (findDataCheck->isChecked() && currentEventTime.isValid() && !currentDataSensorId.isEmpty()) {
        if (loadDataEventWindow(filePath, currentDataSensorId)) {
            return;
        }
    }

    loadSingleFile(filePath);
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
            QString err2;
            if (!WaveformReader::readTextWaveFile(currentFiles[i], wd, err2) || wd.samples.isEmpty()) {
                continue;
            }
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
        const double range = (maxV - minV < 1e-12) ? 1.0 : (maxV - minV);

        p.setPen(QPen(QColor(31, 119, 180), 1));
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

    const int kSampleRateHz = 4000;
    const int pre = 1000;
    const int post = 3000;
    const int totalPoints = pre + post;

    auto readMinuteFile = [](const QString &path, WaveData &out, QString &err) -> bool {
        if (WaveformReader::readTextWaveFile(path, out, err)) {
            return true;
        }
        err.clear();
        return WaveformReader::readWaveFile(path, out, err);
    };

    auto findMinuteFile = [&](const QString &sensorId, const QDateTime &dt) -> QString {
        if (currentDataRoot.isEmpty() || currentDataProject.isEmpty()) {
            return {};
        }
        QDir hDir(currentDataRoot);
        if (!hDir.cd(currentDataProject)) return {};
        if (!hDir.cd(sensorId)) return {};
        if (!hDir.cd(dt.date().toString("yyyy"))) return {};
        if (!hDir.cd(dt.date().toString("MM"))) return {};
        if (!hDir.cd(dt.date().toString("dd"))) return {};

        const QString hNoPad = QString::number(dt.time().hour());
        const QString hPad = dt.time().toString("HH");
        if (!hDir.cd(hNoPad) && !hDir.cd(hPad)) return {};

        const QString mPad = dt.time().toString("mm");
        const QString mNoPad = QString::number(dt.time().minute());
        QStringList filters;
        filters << mPad + ".*" << mPad << mNoPad + ".*" << mNoPad;
        hDir.setNameFilters(filters);
        const QFileInfoList files = hDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &fi : files) {
            if (fi.baseName() == mPad || fi.baseName() == mNoPad || fi.fileName() == mPad || fi.fileName() == mNoPad) {
                return fi.absoluteFilePath();
            }
        }
        return {};
    };

    for (const QString &filePath : currentFiles) {
        WaveData curr;
        QString err;
        if (!readMinuteFile(filePath, curr, err) || curr.samples.isEmpty()) {
            continue;
        }

        QVector<double> trace;
        if (currentEventTime.isValid()) {
            const int sec = currentEventTime.time().second();
            const int ms = currentEventTime.time().msec();
            const int sampleIndex = sec * kSampleRateHz + (ms * kSampleRateHz) / 1000;

            const int startIndex = sampleIndex - pre;
            const int endIndex = sampleIndex + post;

            WaveData prev;
            WaveData next;
            const int currCount = curr.samples.size();
            if (startIndex < 0) {
                const QString sensorId = dataFileSensors.value(filePath);
                const QString prevPath = findMinuteFile(sensorId, currentEventTime.addSecs(-60));
                if (!prevPath.isEmpty()) {
                    QString e;
                    readMinuteFile(prevPath, prev, e);
                }
            }
            if (endIndex > currCount) {
                const QString sensorId = dataFileSensors.value(filePath);
                const QString nextPath = findMinuteFile(sensorId, currentEventTime.addSecs(60));
                if (!nextPath.isEmpty()) {
                    QString e;
                    readMinuteFile(nextPath, next, e);
                }
            }

            trace.reserve(totalPoints);
            for (int i = startIndex; i < endIndex; ++i) {
                double value = 0.0;
                if (i < 0) {
                    const int idx = prev.samples.size() + i;
                    if (idx >= 0 && idx < prev.samples.size() && !prev.samples[idx].isEmpty()) {
                        value = prev.samples[idx][0];
                    }
                } else if (i < currCount) {
                    if (!curr.samples[i].isEmpty()) {
                        value = curr.samples[i][0];
                    }
                } else {
                    const int idx = i - currCount;
                    if (idx >= 0 && idx < next.samples.size() && !next.samples[idx].isEmpty()) {
                        value = next.samples[idx][0];
                    }
                }
                trace.push_back(value);
            }
        } else {
            // Fallback: use full trace if no event time
            trace.reserve(curr.samples.size());
            for (const auto &row : curr.samples) {
                trace.push_back(row.isEmpty() ? 0.0 : row[0]);
            }
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
        if (!currentFiles.isEmpty() && loadSingleFile(currentFiles.first())) {
            statusBar()->showMessage("Stacked preview unavailable for this data, loaded first trace instead.", 5000);
            return;
        }
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

void MainWindow::showFullWaveform() {
    if (currentFilePath.isEmpty()) {
        QMessageBox::information(this, "Show Full Waveform", "Please select a file first.");
        return;
    }

    // Load the full file without event window cropping
    if (loadSingleFile(currentFilePath)) {
        statusBar()->showMessage("Loaded full waveform data", 5000);
    }
}

bool MainWindow::isDataMode() const {
    return modeTabs->currentIndex() == 1;
}
