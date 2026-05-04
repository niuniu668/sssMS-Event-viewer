#include "mainwindow.h"
#include "spectrogramwidget.h"
#include "spectrumwidget.h"
#include "waveformreader.h"
#include "waveformwidget.h"

#include <algorithm>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStatusBar>
#include <QPainter>
#include <QImage>
#include <QSplitter>
#include <QTimer>
#include <QTabWidget>
#include <QDockWidget>
#include <QGroupBox>
#include <QApplication>
#include <QStyleFactory>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QRegularExpression>
#include <QSettings>
#include <QCloseEvent>
#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int kSampleRateHz = 4000;
constexpr int kMaxStackedCount = 10;

void openPathInExplorer(const QString &path) {
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
    if (info.isDir()) {
        QProcess::startDetached("explorer.exe", {nativePath});
    } else {
        QProcess::startDetached("explorer.exe", {"/select,", nativePath});
    }
}

void attachPathContextMenu(QListWidget *listWidget, QWidget *parentWidget) {
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(listWidget, &QWidget::customContextMenuRequested, parentWidget,
                     [listWidget, parentWidget](const QPoint &pos) {
        QListWidgetItem *item = listWidget->itemAt(pos);
        if (!item) {
            return;
        }

        listWidget->setCurrentItem(item);
        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) {
            return;
        }

        QMenu menu(parentWidget);
        QAction *copyNameAction = menu.addAction("复制名称");
        QAction *openExplorerAction = menu.addAction("在资源管理器中打开");

        QAction *chosen = menu.exec(listWidget->viewport()->mapToGlobal(pos));
        if (!chosen) {
            return;
        }

        if (chosen == copyNameAction) {
            QString name = QFileInfo(path).fileName();
            if (name.isEmpty()) {
                name = QFileInfo(path).dir().dirName();
            }
            QGuiApplication::clipboard()->setText(name);
            return;
        }

        if (chosen == openExplorerAction) {
            openPathInExplorer(path);
            return;
        }
    });
}

bool looksLikeTextWaveFile(const QString &filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray probe = f.read(4096);
    if (probe.isEmpty()) {
        return false;
    }

    int printableCount = 0;
    int digitCount = 0;
    int separatorCount = 0;

    for (unsigned char ch : probe) {
        if (ch == 0) {
            return false;
        }
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
            ++printableCount;
            if (ch >= '0' && ch <= '9') {
                ++digitCount;
            }
            continue;
        }
        if (ch == ',' || ch == ';' || ch == '\t' || ch == ' ' || ch == '\r' || ch == '\n') {
            ++separatorCount;
            ++printableCount;
            continue;
        }
    }

    return printableCount > 0 && (digitCount + separatorCount) * 1.0 / probe.size() > 0.75;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      modeTabs(new QTabWidget(this)),
    waveTabs(new QTabWidget(this)),
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
            pickInfoLabel(new QLabel(this)),
            pickFolderEdit(new QLineEdit(this)),
            browsePickFolderButton(new QPushButton("Browse", this)),
            loadPickFolderButton(new QPushButton("Load Folder", this)),
            pickFolderList(new QListWidget(this)),
            pickFileList(new QListWidget(this)),
            pickModeCombo(new QComboBox(this)),
            pickDisplayCombo(new QComboBox(this)),
            pickFilterCombo(new QComboBox(this)),
            pickHighpassHzSpin(new QDoubleSpinBox(this)),
            pickLowpassHzSpin(new QDoubleSpinBox(this)),
            staWindowSpin(new QSpinBox(this)),
            ltaWindowSpin(new QSpinBox(this)),
            staPThresholdSpin(new QDoubleSpinBox(this)),
            staSThresholdSpin(new QDoubleSpinBox(this)),
            staMinGapSpin(new QSpinBox(this)),
            aicPSplitRatioSpin(new QDoubleSpinBox(this)),
            aicSSplitRatioSpin(new QDoubleSpinBox(this)),
            clearPickButton(new QPushButton("Clear Current File", this)),
            savePickButton(new QPushButton("Save Picks", this)),
            exportPickCsvButton(new QPushButton("Export CSV", this)),
            runStaLtaButton(new QPushButton("STA/LTA Assist", this)),
            runAicButton(new QPushButton("AIC Assist", this)),
            runBatchAutoPickButton(new QPushButton("Batch Auto Pick", this)),
            acceptSuggestedButton(new QPushButton("Accept Suggestions", this)),
            pickTable(new QTableWidget(this)),
      waveWidget(new WaveformWidget(this)),
      spectrumWidget(new SpectrumWidget(this)),
      spectrogramWidget(new SpectrogramWidget(this)),
      saveCurrentButton(new QPushButton("Save Current PNG", this)),
            saveOverviewButton(new QPushButton("Save Overview PNG", this)),
            previewStackedButton(new QPushButton("Preview Stacked", this)),
            saveStackedButton(new QPushButton("Save Stacked PNG", this)),
            showFullButton(new QPushButton("Show Full Waveform", this)),
        dataSliceButton(new QPushButton("Show Slice", this)),
            sendToPickButton(new QPushButton("Send to Pick", this)),
            receiveFromDataViewButton(new QPushButton("Receive from Data View", this)),
            pickToggleFullButton(new QPushButton("Show Full Waveform", this)) {

    setWindowTitle("Qt Waveform Viewer");
    resize(1350, 850);

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
    dataOptRow->addWidget(dataSliceButton);
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

    auto *pickPage = new QWidget(left);
    auto *pickLayout = new QVBoxLayout(pickPage);
    pickLayout->setContentsMargins(0, 0, 0, 0);

    auto *pickFolderRow = new QHBoxLayout();
    pickFolderEdit->setPlaceholderText("Input Event/Data folder for picking");
    pickFolderRow->addWidget(new QLabel("Pick Folder:"));
    pickFolderRow->addWidget(pickFolderEdit, 1);
    pickFolderRow->addWidget(browsePickFolderButton);
    pickFolderRow->addWidget(loadPickFolderButton);

    pickModeCombo->addItem("Browse");
    pickModeCombo->addItem("Pick P");
    pickModeCombo->addItem("Pick S");
    pickModeCombo->addItem("Erase");
    pickDisplayCombo->addItems({"Display Raw", "Display Filtered"});

    pickFilterCombo->addItems({"No Filter", "High-pass", "Low-pass", "Band-pass"});
    pickHighpassHzSpin->setRange(0.1, 1800.0);
    pickHighpassHzSpin->setDecimals(1);
    pickHighpassHzSpin->setValue(20.0);
    pickLowpassHzSpin->setRange(0.1, 1900.0);
    pickLowpassHzSpin->setDecimals(1);
    pickLowpassHzSpin->setValue(350.0);

    staWindowSpin->setRange(5, 2000);
    staWindowSpin->setValue(80);
    ltaWindowSpin->setRange(20, 4000);
    ltaWindowSpin->setValue(400);
    staPThresholdSpin->setRange(1.0, 20.0);
    staPThresholdSpin->setDecimals(2);
    staPThresholdSpin->setValue(2.8);
    staSThresholdSpin->setRange(1.0, 20.0);
    staSThresholdSpin->setDecimals(2);
    staSThresholdSpin->setValue(2.2);
    staMinGapSpin->setRange(1, 4000);
    staMinGapSpin->setValue(120);

    aicPSplitRatioSpin->setRange(0.10, 0.90);
    aicPSplitRatioSpin->setDecimals(2);
    aicPSplitRatioSpin->setSingleStep(0.05);
    aicPSplitRatioSpin->setValue(0.50);
    aicSSplitRatioSpin->setRange(0.10, 0.95);
    aicSSplitRatioSpin->setDecimals(2);
    aicSSplitRatioSpin->setSingleStep(0.05);
    aicSSplitRatioSpin->setValue(0.55);




    auto *pickSettingsDock = new QDockWidget("Pick Settings", this);
    pickSettingsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto *dockContent = new QWidget();
    auto *dockLayout = new QVBoxLayout(dockContent);
    dockLayout->setContentsMargins(8, 8, 8, 8);
    
    auto *pickModeGroup = new QGroupBox("Phase & Display");
    auto *pickModeLayout = new QGridLayout(pickModeGroup);
    pickModeLayout->addWidget(new QLabel("Mode:"), 0, 0);
    pickModeLayout->addWidget(pickModeCombo, 0, 1);
    pickModeLayout->addWidget(pickDisplayCombo, 1, 0, 1, 2);
    
    auto *pickFilterGroup = new QGroupBox("Filter Settings");
    auto *pickFilterLayout = new QGridLayout(pickFilterGroup);
    pickFilterLayout->addWidget(new QLabel("Type:"), 0, 0);
    pickFilterLayout->addWidget(pickFilterCombo, 0, 1);
    pickFilterLayout->addWidget(new QLabel("High(Hz):"), 1, 0);
    pickFilterLayout->addWidget(pickHighpassHzSpin, 1, 1);
    pickFilterLayout->addWidget(new QLabel("Low(Hz):"), 2, 0);
    pickFilterLayout->addWidget(pickLowpassHzSpin, 2, 1);
    
    auto *pickStaGroup = new QGroupBox("STA/LTA Settings");
    auto *pickStaLayout = new QGridLayout(pickStaGroup);
    pickStaLayout->addWidget(new QLabel("STA win:"), 0, 0);
    pickStaLayout->addWidget(staWindowSpin, 0, 1);
    pickStaLayout->addWidget(new QLabel("LTA win:"), 1, 0);
    pickStaLayout->addWidget(ltaWindowSpin, 1, 1);
    pickStaLayout->addWidget(new QLabel("P th:"), 2, 0);
    pickStaLayout->addWidget(staPThresholdSpin, 2, 1);
    pickStaLayout->addWidget(new QLabel("S th:"), 3, 0);
    pickStaLayout->addWidget(staSThresholdSpin, 3, 1);
    pickStaLayout->addWidget(new QLabel("Min gap:"), 4, 0);
    pickStaLayout->addWidget(staMinGapSpin, 4, 1);
    
    auto *pickAicGroup = new QGroupBox("AIC Settings");
    auto *pickAicLayout = new QGridLayout(pickAicGroup);
    pickAicLayout->addWidget(new QLabel("P split<="), 0, 0);
    pickAicLayout->addWidget(aicPSplitRatioSpin, 0, 1);
    pickAicLayout->addWidget(new QLabel("S split>="), 1, 0);
    pickAicLayout->addWidget(aicSSplitRatioSpin, 1, 1);

    auto *actionGroup = new QGroupBox("Actions");
    auto *actionLayout = new QVBoxLayout(actionGroup);
    actionLayout->addWidget(runStaLtaButton);
    actionLayout->addWidget(runAicButton);
    actionLayout->addWidget(runBatchAutoPickButton);
    actionLayout->addWidget(acceptSuggestedButton);
    actionLayout->addWidget(clearPickButton);
    actionLayout->addWidget(savePickButton);
    actionLayout->addWidget(exportPickCsvButton);

    dockLayout->addWidget(pickModeGroup);
    dockLayout->addWidget(pickFilterGroup);
    dockLayout->addWidget(pickStaGroup);
    dockLayout->addWidget(pickAicGroup);
    dockLayout->addWidget(actionGroup);
    dockLayout->addStretch();
    
    QScrollArea* dockScroll = new QScrollArea();
    dockScroll->setWidget(dockContent);
    dockScroll->setWidgetResizable(true);
    pickSettingsDock->setWidget(dockScroll);
    addDockWidget(Qt::RightDockWidgetArea, pickSettingsDock);

    auto *pickSplitRow = new QHBoxLayout();
    auto *pickFolderPane = new QVBoxLayout();
    auto *pickFilePane = new QVBoxLayout();
    pickFolderPane->addWidget(new QLabel("Folders:"));
    pickFolderPane->addWidget(pickFolderList, 1);
    pickFilePane->addWidget(new QLabel("Waveform Files:"));
    pickFilePane->addWidget(pickFileList, 1);
    pickSplitRow->addLayout(pickFolderPane, 1);
    pickSplitRow->addLayout(pickFilePane, 1);

    pickTable->setColumnCount(5);
    pickTable->setHorizontalHeaderLabels({"Phase", "Channel", "Sample", "Time(s)", "Source"});
    pickTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pickTable->verticalHeader()->setVisible(false);
    pickTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pickTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    pickInfoLabel->setWordWrap(true);
    pickInfoLabel->setStyleSheet("color:#555;font-size:12px;");

    pickLayout->addLayout(pickFolderRow);
        pickLayout->addLayout(pickSplitRow, 1);
    pickLayout->addWidget(new QLabel("Pick Result:"));
    pickLayout->addWidget(pickTable, 1);
    pickLayout->addWidget(new QLabel("Info:"));
    pickLayout->addWidget(pickInfoLabel);
    pickLayout->addWidget(receiveFromDataViewButton);
    pickLayout->addWidget(pickToggleFullButton);

    dataLayout->addLayout(dataEventRow);
    dataLayout->addLayout(dataFolderRow);
    dataLayout->addLayout(dataOptRow);
    dataLayout->addLayout(dataTreeRow, 1);
    dataLayout->addWidget(new QLabel("Info:"));
    dataLayout->addWidget(dataInfoLabel);
    dataLayout->addWidget(sendToPickButton);

    eventFolderList->setMinimumWidth(50);
    eventFileList->setMinimumWidth(50);
    dataFolderList->setMinimumWidth(50);
    dataFileList->setMinimumWidth(50);
    pickFolderList->setMinimumWidth(50);
    pickFileList->setMinimumWidth(50);

    attachPathContextMenu(eventFolderList, this);
    attachPathContextMenu(eventFileList, this);
    attachPathContextMenu(dataFolderList, this);
    attachPathContextMenu(dataFileList, this);
    attachPathContextMenu(pickFolderList, this);
    attachPathContextMenu(pickFileList, this);
    
    // Set button styles
    sendToPickButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 6px; border-radius: 4px; } QPushButton:hover { background-color: #45a049; }");
    receiveFromDataViewButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; padding: 6px; border-radius: 4px; } QPushButton:hover { background-color: #1976D2; }");
    pickToggleFullButton->setStyleSheet("QPushButton { background-color: #FF9800; color: white; font-weight: bold; padding: 6px; border-radius: 4px; } QPushButton:hover { background-color: #F57C00; }");

    folderStackedCheck->setChecked(true);
    findDataCheck->setChecked(true);

    waveTabs->addTab(eventPage, "Event View");
    waveTabs->addTab(dataPage, "Data View");

    auto *wavePage = new QWidget(left);
    auto *wavePageLayout = new QVBoxLayout(wavePage);
    wavePageLayout->setContentsMargins(0, 0, 0, 0);
    wavePageLayout->addWidget(waveTabs, 1);

    modeTabs->addTab(wavePage, "Waveform View");
    modeTabs->addTab(pickPage, "Pick View");
    leftLayout->addWidget(modeTabs, 1);

    pickSettingsDock->setVisible(modeTabs->currentIndex() == 1);


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
    connect(browsePickFolderButton, &QPushButton::clicked, this, &MainWindow::browsePickFolder);
    connect(loadPickFolderButton, &QPushButton::clicked, this, &MainWindow::loadPickFolder);
    connect(pickFolderEdit, &QLineEdit::returnPressed, this, &MainWindow::loadPickFolder);
    connect(pickFolderList, &QListWidget::itemClicked, this, &MainWindow::onPickFolderItemClicked);
    connect(pickFileList, &QListWidget::itemClicked, this, &MainWindow::onPickFileItemClicked);
    connect(pickModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onPickModeChanged);
    connect(clearPickButton, &QPushButton::clicked, this, &MainWindow::clearCurrentPickMarkers);
    connect(savePickButton, &QPushButton::clicked, this, &MainWindow::savePickMarkers);
    connect(exportPickCsvButton, &QPushButton::clicked, this, &MainWindow::exportPickCsv);
    connect(runStaLtaButton, &QPushButton::clicked, this, &MainWindow::runStaLtaAssist);
    connect(runAicButton, &QPushButton::clicked, this, &MainWindow::runAicAssist);
    connect(runBatchAutoPickButton, &QPushButton::clicked, this, &MainWindow::runBatchAutoPick);
    connect(acceptSuggestedButton, &QPushButton::clicked, this, &MainWindow::acceptSuggestedPickMarkers);
    connect(pickDisplayCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        reloadPickWaveformDisplay();
        refreshPickInfo();
    });
    connect(pickFilterCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        waveWidget->clearAssistCurve();
        reloadPickWaveformDisplay();
        refreshPickInfo();
    });
    connect(pickHighpassHzSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        waveWidget->clearAssistCurve();
        reloadPickWaveformDisplay();
        refreshPickInfo();
    });
    connect(pickLowpassHzSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        waveWidget->clearAssistCurve();
        reloadPickWaveformDisplay();
        refreshPickInfo();
    });
    connect(staWindowSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { refreshPickInfo(); });
    connect(ltaWindowSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { refreshPickInfo(); });
    connect(staPThresholdSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { refreshPickInfo(); });
    connect(staSThresholdSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { refreshPickInfo(); });
    connect(staMinGapSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { refreshPickInfo(); });
    connect(aicPSplitRatioSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { refreshPickInfo(); });
    connect(aicSSplitRatioSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { refreshPickInfo(); });
    connect(waveWidget, &WaveformWidget::pickMarkerAdded, this, &MainWindow::onPickMarkerAdded);
    connect(waveWidget, &WaveformWidget::pickMarkerRemoved, this, &MainWindow::onPickMarkerRemoved);
    connect(waveWidget, &WaveformWidget::assistRangeChanged, this, [this](int s, int e, bool valid) {
        if (valid) {
            const int a = std::min(s, e);
            const int b = std::max(s, e);
            statusBar()->showMessage(QString("Assist range: [%1, %2], width=%3 samples")
                                         .arg(a)
                                         .arg(b)
                                         .arg(b - a),
                                     4000);
        } else {
            statusBar()->showMessage("Assist range cleared", 2500);
        }
        refreshPickInfo();
    });

    connect(waveWidget, &WaveformWidget::viewWindowChanged, this, &MainWindow::onWaveViewChanged);

    // === UX Enhancements: Status Bar coordinates ===
    waveWidget->setMouseTracking(true); 
    connect(waveWidget, &WaveformWidget::mouseHovered, this, [this](int channel, double timeSec, double value) {
        if (channel >= 0) {
            statusBar()->showMessage(QString("Channel: %1 | Time: %2 s | Amplitude: %3")
                                     .arg(channel + 1)
                                     .arg(timeSec, 0, 'f', 4)
                                     .arg(value, 0, 'f', 2));
        } else {
            statusBar()->showMessage("Ready");
        }
    });
    connect(showSpectrumCheck, &QCheckBox::toggled, this, &MainWindow::onShowSpectrumChanged);
    connect(showSpectrogramCheck, &QCheckBox::toggled, this, &MainWindow::onShowSpectrogramChanged);
    connect(saveCurrentButton, &QPushButton::clicked, this, &MainWindow::exportCurrentPng);
    connect(saveOverviewButton, &QPushButton::clicked, this, &MainWindow::exportOverviewPng);
    connect(previewStackedButton, &QPushButton::clicked, this, &MainWindow::previewStacked);
    connect(saveStackedButton, &QPushButton::clicked, this, &MainWindow::exportStackedPng);
    connect(showFullButton, &QPushButton::clicked, this, &MainWindow::showFullWaveform);
    connect(dataSliceButton, &QPushButton::clicked, this, &MainWindow::toggleDataSliceMode);
    connect(sendToPickButton, &QPushButton::clicked, this, &MainWindow::sendToPick);
    connect(receiveFromDataViewButton, &QPushButton::clicked, this, &MainWindow::receiveFromDataView);
    connect(pickToggleFullButton, &QPushButton::clicked, this, &MainWindow::togglePickWaveformMode);


    // === UX Enhancements: Shortcuts ===
    auto *shortcutB = new QShortcut(QKeySequence("B"), this);
    connect(shortcutB, &QShortcut::activated, this, [this]() {
        modeTabs->setCurrentIndex(1);
        pickModeCombo->setCurrentIndex(0); 
    });

    auto *shortcutP = new QShortcut(QKeySequence("P"), this);
    connect(shortcutP, &QShortcut::activated, this, [this]() {
        modeTabs->setCurrentIndex(1);
        pickModeCombo->setCurrentIndex(1);
    });

    auto *shortcutS = new QShortcut(QKeySequence("S"), this);
    connect(shortcutS, &QShortcut::activated, this, [this]() {
        modeTabs->setCurrentIndex(1);
        pickModeCombo->setCurrentIndex(2);
    });

    auto *shortcutE = new QShortcut(QKeySequence("E"), this);
    connect(shortcutE, &QShortcut::activated, this, [this]() {
        modeTabs->setCurrentIndex(1);
        pickModeCombo->setCurrentIndex(3);
    });

    // === UX Enhancements: Right-click Context Menu on Waveform ===
    waveWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(waveWidget, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (modeTabs->currentIndex() != 1) return; // Only show in Pick View
        
        QMenu menu(this);
        
        QAction *actB = menu.addAction("Mode: Browse (B)");
        QAction *actP = menu.addAction("Mode: Pick P (P)");
        QAction *actS = menu.addAction("Mode: Pick S (S)");
        QAction *actE = menu.addAction("Mode: Erase (E)");
        menu.addSeparator();
        QAction *actSetRangeStart = menu.addAction("Set Assist Range Start Here");
        QAction *actSetRangeEnd = menu.addAction("Set Assist Range End Here");
        QAction *actClearRange = menu.addAction("Clear Assist Range");
        menu.addSeparator();
        QAction *actAutoStaLta = menu.addAction("Run STA/LTA Assist");
        QAction *actAutoAic = menu.addAction("Run AIC Assist");
        menu.addSeparator();
        QAction *actClear = menu.addAction("Clear Current Configs/Picks");

        const int sampleAtCursor = waveWidget->sampleIndexAt(pos);

        connect(actB, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(0); });
        connect(actP, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(1); });
        connect(actS, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(2); });
        connect(actE, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(3); });
        connect(actSetRangeStart, &QAction::triggered, this, [this, sampleAtCursor](){
            if (sampleAtCursor >= 0) {
                waveWidget->setAssistRangeStart(sampleAtCursor);
            }
        });
        connect(actSetRangeEnd, &QAction::triggered, this, [this, sampleAtCursor](){
            if (sampleAtCursor >= 0) {
                waveWidget->setAssistRangeEnd(sampleAtCursor);
            }
        });
        connect(actClearRange, &QAction::triggered, this, [this](){ waveWidget->clearAssistRange(); });
        connect(actAutoStaLta, &QAction::triggered, this, &MainWindow::runStaLtaAssist);
        connect(actAutoAic, &QAction::triggered, this, &MainWindow::runAicAssist);
        connect(actClear, &QAction::triggered, this, &MainWindow::clearCurrentPickMarkers);

        menu.exec(waveWidget->mapToGlobal(pos));
    });

    connect(modeTabs, &QTabWidget::currentChanged, this, [this, left, splitter, pickSettingsDock](int idx) {
        const bool isPick = (idx == 1);
        pickSettingsDock->setVisible(isPick);

        // Keep pick context aligned with the latest file loaded in Data/Event view.
        if (isPick && !currentFilePath.isEmpty() && currentFilePath != pickCurrentFilePath) {
            loadPickFile(currentFilePath);
        }

        if (isPick) {
            left->setMaximumWidth(560);
            splitter->setSizes({520, 780});
        } else {
            left->setMaximumWidth(380);
            splitter->setSizes({320, 980});
        }
    });

    QTimer::singleShot(0, this, [this]() {
        restoreSessionState();
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSessionState();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveSessionState() const {
    QSettings settings("QtWaveformViewer", "QtWaveformViewer");
    settings.setValue("event/folder", eventFolderEdit->text().trimmed());
    settings.setValue("event/file", lastEventFilePath.isEmpty() ? currentFilePath : lastEventFilePath);
    settings.setValue("data/eventFolder", dataEventFolderEdit->text().trimmed());
    settings.setValue("data/folder", dataFolderEdit->text().trimmed());
    settings.setValue("data/file", lastDataFilePath.isEmpty() ? currentFilePath : lastDataFilePath);
    settings.setValue("pick/folder", pickFolderEdit->text().trimmed());
    settings.setValue("pick/file", lastPickFilePath.isEmpty() ? pickCurrentFilePath : lastPickFilePath);
    settings.setValue("ui/mainTab", modeTabs->currentIndex());
    settings.setValue("ui/waveTab", waveTabs->currentIndex());
    settings.setValue("ui/recursive", recursiveCheck->isChecked());
    settings.setValue("ui/normalize", normalizeCheck->isChecked());
    settings.setValue("ui/folderStacked", folderStackedCheck->isChecked());
    settings.setValue("ui/findData", findDataCheck->isChecked());
    settings.setValue("ui/showSpectrum", showSpectrumCheck->isChecked());
    settings.setValue("ui/showSpectrogram", showSpectrogramCheck->isChecked());
    settings.setValue("ui/dataSlice", dataShowSlice);
    settings.setValue("ui/pickShowFullWaveform", pickShowFullWaveform);
}

void MainWindow::restoreSessionState() {
    QSettings settings("QtWaveformViewer", "QtWaveformViewer");
    lastEventFolderPath = settings.value("event/folder").toString();
    lastEventFilePath = settings.value("event/file").toString();
    lastDataEventFolderPath = settings.value("data/eventFolder").toString();
    lastDataFolderPath = settings.value("data/folder").toString();
    lastDataFilePath = settings.value("data/file").toString();
    lastPickFolderPath = settings.value("pick/folder").toString();
    lastPickFilePath = settings.value("pick/file").toString();

    recursiveCheck->setChecked(settings.value("ui/recursive", recursiveCheck->isChecked()).toBool());
    normalizeCheck->setChecked(settings.value("ui/normalize", normalizeCheck->isChecked()).toBool());
    folderStackedCheck->setChecked(settings.value("ui/folderStacked", folderStackedCheck->isChecked()).toBool());
    findDataCheck->setChecked(settings.value("ui/findData", findDataCheck->isChecked()).toBool());
    showSpectrumCheck->setChecked(settings.value("ui/showSpectrum", showSpectrumCheck->isChecked()).toBool());
    showSpectrogramCheck->setChecked(settings.value("ui/showSpectrogram", showSpectrogramCheck->isChecked()).toBool());
    dataShowSlice = settings.value("ui/dataSlice", dataShowSlice).toBool();
    pickShowFullWaveform = settings.value("ui/pickShowFullWaveform", pickShowFullWaveform).toBool();

    eventFolderEdit->setText(lastEventFolderPath);
    dataEventFolderEdit->setText(lastDataEventFolderPath);
    dataFolderEdit->setText(lastDataFolderPath);
    pickFolderEdit->setText(lastPickFolderPath);

    lastMainTabIndex = settings.value("ui/mainTab", modeTabs->currentIndex()).toInt();
    lastWaveTabIndex = settings.value("ui/waveTab", waveTabs->currentIndex()).toInt();
    modeTabs->setCurrentIndex(lastMainTabIndex);
    waveTabs->setCurrentIndex(lastWaveTabIndex);

    if (lastMainTabIndex == 0 && !lastEventFolderPath.isEmpty()) {
        loadEventFolder();
    } else if (lastMainTabIndex == 1 && !lastPickFolderPath.isEmpty()) {
        loadPickFolder();
    } else if (lastMainTabIndex == 2 && !lastDataFolderPath.isEmpty()) {
        loadDataFolder();
    }
}

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

void MainWindow::browsePickFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Pick folder", pickFolderEdit->text().trimmed());
    if (!dir.isEmpty()) {
        pickFolderEdit->setText(QDir::toNativeSeparators(dir));
        loadPickFolder();
    }
}

void MainWindow::loadPickFolder() {
    QString folder = pickFolderEdit->text().trimmed();
    if (folder.startsWith('"') && folder.endsWith('"') && folder.size() >= 2) {
        folder = folder.mid(1, folder.size() - 2).trimmed();
    }

    if (folder.isEmpty()) {
        QMessageBox::warning(this, "Input", "Please input a folder for picking.");
        return;
    }

    QFileInfo fi(folder);
    if (!fi.exists() || !fi.isDir()) {
        QMessageBox::warning(this, "Input", QString("Invalid folder: %1").arg(folder));
        return;
    }

    pickCurrentFolder = fi.absoluteFilePath();
    lastPickFolderPath = pickCurrentFolder;
    pickCurrentFiles = collectFiles(pickCurrentFolder, recursiveCheck->isChecked());
    pickMarkersByFile.clear();

    pickFolderList->clear();
    pickFileList->clear();

    QDir currentDir(pickCurrentFolder);
    QDir parentDir = currentDir;
    if (!parentDir.isRoot()) {
        parentDir.cdUp();
    }
    const QFileInfoList siblings = parentDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &sibling : siblings) {
        auto *item = new QListWidgetItem(sibling.fileName());
        item->setData(Qt::UserRole, sibling.absoluteFilePath());
        pickFolderList->addItem(item);
        if (sibling.absoluteFilePath() == pickCurrentFolder) {
            pickFolderList->setCurrentItem(item);
        }
    }

    for (const QString &filePath : pickCurrentFiles) {
        auto *item = new QListWidgetItem(QDir::toNativeSeparators(filePath));
        item->setToolTip(QDir::toNativeSeparators(filePath));
        item->setData(Qt::UserRole, filePath);
        pickFileList->addItem(item);
    }
    if (!pickCurrentFiles.isEmpty()) {
        pickFileList->setCurrentRow(0);
    }

    loadPickMarkersFromDisk();
    refreshPickInfo();

    if (!pickCurrentFiles.isEmpty()) {
        QString chosenPath = pickCurrentFiles.first();
        if (!lastPickFilePath.isEmpty()) {
            for (const QString &filePath : pickCurrentFiles) {
                if (filePath == lastPickFilePath) {
                    chosenPath = filePath;
                    break;
                }
            }
        }
        loadPickFile(chosenPath);
    } else {
        pickCurrentFilePath.clear();
        waveWidget->clearData();
        refreshPickTable();
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

            // Collect candidates per sensor and pick the best single file per sensor
            QString chosenPath;
            for (const QFileInfo &mFi : minuteFiles) {
                const QString base = mFi.baseName();
                const QString file = mFi.fileName();
                if (!(base == minutePad || base == minuteNoPad ||
                      file == minutePad || file == minuteNoPad ||
                      base.startsWith(minutePad) || base.startsWith(minuteNoPad) ||
                      file.startsWith(minutePad) || file.startsWith(minuteNoPad))) {
                    continue;
                }

                const QString absPath = mFi.absoluteFilePath();
                // Prefer exact base name match
                if (base == minutePad || base == minuteNoPad || file == minutePad || file == minuteNoPad) {
                    chosenPath = absPath;
                    break;
                }

                // Otherwise pick first candidate if none chosen yet
                if (chosenPath.isEmpty()) {
                    chosenPath = absPath;
                }
            }

            if (!chosenPath.isEmpty() && !results.contains(chosenPath)) {
                results.push_back(chosenPath);
                fileSensors.insert(chosenPath, sensorId);
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
    lastEventFolderPath = currentFolder;
    currentFiles = collectFiles(currentFolder, recursiveCheck->isChecked());

    populateFolderAndFileColumns(eventFolderList, eventFileList, currentFolder, currentFiles);

    eventInfoLabel->setText(QString("Loaded Event folder:\n%1\n\nFiles found: %2")
        .arg(QDir::toNativeSeparators(currentFolder))
        .arg(currentFiles.size()));

    statusBar()->showMessage(QString("Event folder loaded. Files: %1").arg(currentFiles.size()));

    if (!currentFiles.isEmpty()) {
        populateFilesForFolder(eventFileList, currentFiles);

        QString chosenFile = currentFiles.first();
        if (!lastEventFilePath.isEmpty()) {
            for (const QString &filePath : currentFiles) {
                if (filePath == lastEventFilePath) {
                    chosenFile = filePath;
                    break;
                }
            }
        }

        if (folderStackedCheck->isChecked()) {
            if (currentFiles.size() <= kMaxStackedCount) {
                previewStacked();
            } else {
                loadSingleFile(chosenFile);
            }
        } else {
            loadSingleFile(chosenFile);
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
        lastDataEventFolderPath = currentFolder;
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
        lastDataFolderPath = dataRoot;

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
        lastDataFolderPath = currentFolder;
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

        QString chosenFile = currentFiles.first();
        if (!lastDataFilePath.isEmpty()) {
            for (const QString &filePath : currentFiles) {
                if (filePath == lastDataFilePath) {
                    chosenFile = filePath;
                    break;
                }
            }
        }

        if (folderStackedCheck->isChecked()) {
            if (currentFiles.size() <= kMaxStackedCount) {
                previewStacked();
            } else {
                loadSingleFile(chosenFile);
            }
        } else {
            loadSingleFile(chosenFile);
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

    if (!readWaveformFileAuto(filePath, wd, error)) {
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
    currentRawSamples = wd.samples;
    currentChannelNames = channelNames;

    QVector<QVector<double>> displaySamples = currentRawSamples;
    int sliceStartIndex = 0;
    if (isDataMode() && dataShowSlice && buildDataSliceSamples(currentRawSamples, displaySamples, sliceStartIndex)) {
        // displaySamples already updated
    }

    currentSignal.clear();
    currentSignal.reserve(displaySamples.size());
    for (const auto &row : displaySamples) {
        currentSignal.push_back(row.isEmpty() ? 0.0 : row[0]);
    }

    waveWidget->setData(displaySamples, channelNames);
    waveWidget->setPickMarkers({});
    currentViewStart = waveWidget->visibleStartSample();
    currentViewEnd = waveWidget->visibleEndSample();
    refreshAnalysisWidgets();

    if (isDataMode() && dataShowSlice) {
        statusBar()->showMessage(QString("Loaded: %1  display=(%2,%3)  raw=(%4,%5)")
        .arg(QFileInfo(filePath).fileName())
        .arg(displaySamples.size())
        .arg(displaySamples.isEmpty() ? 0 : displaySamples[0].size())
        .arg(currentRawSamples.size())
        .arg(currentRawSamples.isEmpty() ? 0 : currentRawSamples[0].size()));
    } else {
        statusBar()->showMessage(QString("Loaded: %1  shape=(%2,%3)")
        .arg(QFileInfo(filePath).fileName())
        .arg(displaySamples.size())
        .arg(displaySamples.isEmpty() ? 0 : displaySamples[0].size()));
    }
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
    waveWidget->setPickMarkers({});
    currentViewStart = waveWidget->visibleStartSample();
    currentViewEnd = waveWidget->visibleEndSample();
    refreshAnalysisWidgets();

    statusBar()->showMessage(QString("Loaded event window: %1  shape=(%2,%3)")
        .arg(QFileInfo(filePath).fileName())
        .arg(wd.samples.size())
        .arg(wd.samples.isEmpty() ? 0 : wd.samples[0].size()));
    return true;
}

bool MainWindow::buildDataSliceSamples(const QVector<QVector<double>> &source, QVector<QVector<double>> &outSamples, int &usedStartIndex) const {
    if (source.isEmpty()) {
        return false;
    }

    const int sourceCount = static_cast<int>(source.size());
    const int targetCount = std::min(4000, sourceCount);
    int startIndex = 0;
    if (currentEventTime.isValid()) {
        const int sec = currentEventTime.time().second();
        const int ms = currentEventTime.time().msec();
        const int anchorIndex = sec * kSampleRateHz + (ms * kSampleRateHz) / 1000;
        startIndex = anchorIndex - targetCount / 2;
    } else {
        startIndex = (sourceCount - targetCount) / 2;
    }
    startIndex = std::clamp(startIndex, 0, std::max(0, sourceCount - targetCount));

    outSamples = source.mid(startIndex, targetCount);
    usedStartIndex = startIndex;
    return !outSamples.isEmpty();
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

    loadSingleFile(filePath);
}

void MainWindow::onPickFolderItemClicked(QListWidgetItem *item) {
    if (!item) {
        return;
    }

    const QString folderPath = item->data(Qt::UserRole).toString();
    if (folderPath.isEmpty() || folderPath == pickCurrentFolder) {
        return;
    }

    pickFolderEdit->setText(QDir::toNativeSeparators(folderPath));
    QTimer::singleShot(0, this, &MainWindow::loadPickFolder);
}

void MainWindow::onPickFileItemClicked(QListWidgetItem *item) {
    if (!item) {
        return;
    }
    const QString filePath = item->data(Qt::UserRole).toString();
    if (!filePath.isEmpty()) {
        loadPickFile(filePath);
    }
}

void MainWindow::onPickModeChanged(int index) {
    WaveformWidget::PickMode mode = WaveformWidget::PickMode::Navigate;
    if (index == 1) {
        mode = WaveformWidget::PickMode::PickP;
    } else if (index == 2) {
        mode = WaveformWidget::PickMode::PickS;
    } else if (index == 3) {
        mode = WaveformWidget::PickMode::Erase;
    }
    waveWidget->setPickMode(mode);
}

void MainWindow::onPickMarkerAdded(int sampleIndex, int channel, const QString &phase, bool suggested) {
    if (pickCurrentFilePath.isEmpty()) {
        return;
    }

    auto &markers = pickMarkersByFile[pickCurrentFilePath];
    WaveformWidget::PickMarker marker;
    marker.sampleIndex = sampleIndex;
    marker.channel = channel;
    marker.phase = phase;
    marker.suggested = suggested;

    bool replaced = false;
    for (WaveformWidget::PickMarker &m : markers) {
        if (m.channel == marker.channel && m.phase == marker.phase && m.suggested == marker.suggested) {
            m = marker;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        markers.push_back(marker);
    }

    refreshPickTable();
    refreshPickInfo();
}

void MainWindow::onPickMarkerRemoved(int sampleIndex, int channel, const QString &phase, bool suggested) {
    if (pickCurrentFilePath.isEmpty() || !pickMarkersByFile.contains(pickCurrentFilePath)) {
        return;
    }

    auto &markers = pickMarkersByFile[pickCurrentFilePath];
    for (int i = 0; i < markers.size(); ++i) {
        const auto &m = markers[i];
        if (m.sampleIndex == sampleIndex && m.channel == channel && m.phase == phase && m.suggested == suggested) {
            markers.removeAt(i);
            break;
        }
    }

    refreshPickTable();
    refreshPickInfo();
}

void MainWindow::clearCurrentPickMarkers() {
    if (pickCurrentFilePath.isEmpty()) {
        return;
    }
    pickMarkersByFile[pickCurrentFilePath].clear();
    waveWidget->setPickMarkers({});
    refreshPickTable();
    refreshPickInfo();
}

void MainWindow::savePickMarkers() {
    if (pickCurrentFolder.isEmpty()) {
        QMessageBox::information(this, "Pick", "Please load a pick folder first.");
        return;
    }

    QJsonArray items;
    for (auto it = pickMarkersByFile.constBegin(); it != pickMarkersByFile.constEnd(); ++it) {
        const QString relPath = QDir(pickCurrentFolder).relativeFilePath(it.key());
        for (const auto &m : it.value()) {
            QJsonObject obj;
            obj["file"] = relPath;
            obj["phase"] = m.phase;
            obj["channel"] = m.channel;
            obj["sample"] = m.sampleIndex;
            obj["source"] = m.suggested ? "assist" : "manual";
            items.append(obj);
        }
    }

    QJsonObject root;
    root["version"] = 1;
    root["sampleRateHz"] = kSampleRateHz;
    root["items"] = items;

    QFile f(pickMarkerStorePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Pick", "Failed to save pick markers.");
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();

    statusBar()->showMessage(QString("Pick markers saved: %1").arg(QDir::toNativeSeparators(pickMarkerStorePath())), 5000);
}

void MainWindow::exportPickCsv() {
    if (pickCurrentFolder.isEmpty()) {
        QMessageBox::information(this, "Pick", "Please load a pick folder first.");
        return;
    }

    const QString defaultPath = QDir(pickCurrentFolder).filePath("pick_markers.csv");
    const QString savePath = QFileDialog::getSaveFileName(this,
                                                          "Export pick markers CSV",
                                                          defaultPath,
                                                          "CSV (*.csv)");
    if (savePath.isEmpty()) {
        return;
    }

    QFile outFile(savePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, "Pick", "Failed to open CSV file for writing.");
        return;
    }

    QTextStream out(&outFile);
    out << "file,phase,channel,sample,time_sec,source\n";

    int rowCount = 0;
    for (auto it = pickMarkersByFile.constBegin(); it != pickMarkersByFile.constEnd(); ++it) {
        const QString relPath = QDir(pickCurrentFolder).relativeFilePath(it.key());
        for (const auto &m : it.value()) {
            const double t = static_cast<double>(m.sampleIndex) / kSampleRateHz;
            out << '"' << relPath << "\",";
            out << '"' << m.phase << "\",";
            out << (m.channel + 1) << ',';
            out << m.sampleIndex << ',';
            out << QString::number(t, 'f', 6) << ',';
            out << (m.suggested ? "assist" : "manual") << "\n";
            ++rowCount;
        }
    }

    outFile.close();
    statusBar()->showMessage(QString("Exported %1 markers to CSV: %2")
                                 .arg(rowCount)
                                 .arg(QDir::toNativeSeparators(savePath)),
                             5000);
}

void MainWindow::runStaLtaAssist() {
    if (pickCurrentFilePath.isEmpty() || currentSignal.isEmpty()) {
        QMessageBox::information(this, "STA/LTA", "Please load one waveform in Pick View first.");
        return;
    }

    const QVector<double> assistSignal = buildAssistSignal();
    const int n = assistSignal.size();
    const int sta = staWindowSpin->value();
    const int lta = ltaWindowSpin->value();
    const double pThreshold = staPThresholdSpin->value();
    const double sThreshold = staSThresholdSpin->value();
    const int minGap = staMinGapSpin->value();

    if (lta <= sta) {
        QMessageBox::warning(this, "STA/LTA", "LTA window must be larger than STA window.");
        return;
    }
    if (n <= lta + 1) {
        return;
    }

    QVector<double> absSig(n, 0.0);
    for (int i = 0; i < n; ++i) {
        absSig[i] = std::abs(assistSignal[i]);
    }

    QVector<double> prefix(n + 1, 0.0);
    for (int i = 0; i < n; ++i) {
        prefix[i + 1] = prefix[i] + absSig[i];
    }

    auto meanRange = [&](int start, int end) {
        start = std::clamp(start, 0, n);
        end = std::clamp(end, 0, n);
        if (end <= start) {
            return 0.0;
        }
        return (prefix[end] - prefix[start]) / (end - start);
    };

    int rangeStart = 0;
    int rangeEnd = n - 1;
    if (waveWidget->assistRange(rangeStart, rangeEnd)) {
        rangeStart = std::clamp(rangeStart, 0, n - 1);
        rangeEnd = std::clamp(rangeEnd, 0, n - 1);
    }
    if (rangeEnd - rangeStart <= lta + 2) {
        QMessageBox::warning(this, "STA/LTA", "Selected range is too short for current STA/LTA window.");
        return;
    }

    QVector<double> ratioCurve(n, std::numeric_limits<double>::quiet_NaN());
    int pPick = -1;
    int sPick = -1;
    const int loopStart = std::max(lta, rangeStart + lta);
    for (int i = loopStart; i <= rangeEnd; ++i) {
        const double staMean = meanRange(i - sta, i);
        const double ltaMean = meanRange(i - lta, i);
        const double ratio = (ltaMean < 1e-9) ? 0.0 : (staMean / ltaMean);
        ratioCurve[i] = ratio;
        if (pPick < 0 && ratio > pThreshold) {
            pPick = i;
        }
        if (pPick >= 0 && i > pPick + minGap && i <= rangeEnd && ratio > sThreshold) {
            sPick = i;
            break;
        }
    }

    if (pPick >= 0) {
        WaveformWidget::PickMarker p;
        p.sampleIndex = pPick;
        p.channel = 0;
        p.phase = "P";
        p.suggested = true;
        upsertFileMarker(pickCurrentFilePath, p);
    }

    if (sPick >= 0) {
        WaveformWidget::PickMarker s;
        s.sampleIndex = sPick;
        s.channel = 0;
        s.phase = "S";
        s.suggested = true;
        upsertFileMarker(pickCurrentFilePath, s);
    }

    waveWidget->setAssistCurve(ratioCurve, "STA/LTA");
    waveWidget->setPickMarkers(pickMarkersByFile.value(pickCurrentFilePath));
    refreshPickTable();
    refreshPickInfo();
    statusBar()->showMessage(QString("STA/LTA done in range [%1, %2], P=%3, S=%4")
                                 .arg(rangeStart)
                                 .arg(rangeEnd)
                                 .arg(pPick)
                                 .arg(sPick),
                             4500);
}

void MainWindow::runAicAssist() {
    if (pickCurrentFilePath.isEmpty() || currentSignal.size() < 120) {
        QMessageBox::information(this, "AIC", "Please load one waveform in Pick View first.");
        return;
    }

    const QVector<double> assistSignal = buildAssistSignal();
    const int n = assistSignal.size();

    int rangeStart = 0;
    int rangeEnd = n - 1;
    if (waveWidget->assistRange(rangeStart, rangeEnd)) {
        rangeStart = std::clamp(rangeStart, 0, n - 1);
        rangeEnd = std::clamp(rangeEnd, 0, n - 1);
    }

    const int begin = rangeStart;
    const int endExclusive = rangeEnd + 1;
    const int m = endExclusive - begin;
    if (m < 60) {
        QMessageBox::warning(this, "AIC", "Selected range is too short. Please choose a wider range.");
        return;
    }

    auto segmentVariance = [&](int segBegin, int segEnd) {
        const int len = segEnd - segBegin;
        const int absBegin = begin + segBegin;
        const int absEnd = begin + segEnd;
        if (len <= 1) {
            return 1e-9;
        }
        double sum = 0.0;
        for (int i = absBegin; i < absEnd; ++i) {
            sum += assistSignal[i];
        }
        const double mean = sum / len;
        double var = 0.0;
        for (int i = absBegin; i < absEnd; ++i) {
            const double d = assistSignal[i] - mean;
            var += d * d;
        }
        var /= (len - 1);
        return std::max(var, 1e-9);
    };

    auto bestAicIndex = [&](int start, int stop, QVector<double> *curveOut) {
        int bestIdx = -1;
        double bestVal = std::numeric_limits<double>::max();
        for (int k = start; k < stop; ++k) {
            const int leftLen = k;
            const int rightLen = m - k;
            if (leftLen <= 2 || rightLen <= 2) {
                continue;
            }
            const double v1 = segmentVariance(0, k);
            const double v2 = segmentVariance(k, m);
            const double aic = leftLen * std::log(v1) + rightLen * std::log(v2);
            if (curveOut) {
                (*curveOut)[begin + k] = aic;
            }
            if (aic < bestVal) {
                bestVal = aic;
                bestIdx = k;
            }
        }
        return bestIdx;
    };

    QVector<double> aicCurve(n, std::numeric_limits<double>::quiet_NaN());
    const int pStop = std::clamp(static_cast<int>(std::round(m * aicPSplitRatioSpin->value())), 20, m - 20);
    const int sStart = std::clamp(static_cast<int>(std::round(m * aicSSplitRatioSpin->value())), 20, m - 20);
    const int pLocal = bestAicIndex(10, pStop, &aicCurve);
    const int sLocal = bestAicIndex(std::max(10, sStart), m - 10, &aicCurve);
    const int pPick = (pLocal >= 0) ? (begin + pLocal) : -1;
    const int sPick = (sLocal >= 0) ? (begin + sLocal) : -1;

    if (pPick >= 0) {
        WaveformWidget::PickMarker p;
        p.sampleIndex = pPick;
        p.channel = 0;
        p.phase = "P";
        p.suggested = true;
        upsertFileMarker(pickCurrentFilePath, p);
    }
    if (sPick >= 0) {
        WaveformWidget::PickMarker s;
        s.sampleIndex = sPick;
        s.channel = 0;
        s.phase = "S";
        s.suggested = true;
        upsertFileMarker(pickCurrentFilePath, s);
    }

    waveWidget->setAssistCurve(aicCurve, "AIC Cost");
    waveWidget->setPickMarkers(pickMarkersByFile.value(pickCurrentFilePath));
    refreshPickTable();
    refreshPickInfo();
    statusBar()->showMessage(QString("AIC done in range [%1, %2], P=%3, S=%4")
                                 .arg(begin)
                                 .arg(rangeEnd)
                                 .arg(pPick)
                                 .arg(sPick),
                             4500);
}

void MainWindow::runBatchAutoPick() {
    if (pickCurrentFiles.isEmpty()) {
        QMessageBox::information(this, "Batch Auto Pick", "Please load a pick folder first.");
        return;
    }

    int processed = 0;
    int updated = 0;

    for (const QString &filePath : pickCurrentFiles) {
        WaveData wd;
        QString error;
        if (!readWaveformFileAuto(filePath, wd, error) || wd.samples.isEmpty()) {
            continue;
        }

        QVector<double> signal;
        signal.reserve(wd.samples.size());
        for (const auto &row : wd.samples) {
            signal.push_back(row.isEmpty() ? 0.0 : row[0]);
        }

        const QVector<double> assistSignal = buildAssistSignalFrom(signal);
        const int n = assistSignal.size();
        if (n < 80) {
            continue;
        }

        int pPick = -1;
        int sPick = -1;

        const int sta = staWindowSpin->value();
        const int lta = ltaWindowSpin->value();
        const double pThreshold = staPThresholdSpin->value();
        const double sThreshold = staSThresholdSpin->value();
        const int minGap = staMinGapSpin->value();

        if (lta > sta && n > lta + 2) {
            QVector<double> absSig(n, 0.0);
            QVector<double> prefix(n + 1, 0.0);
            for (int i = 0; i < n; ++i) {
                absSig[i] = std::abs(assistSignal[i]);
                prefix[i + 1] = prefix[i] + absSig[i];
            }

            auto meanRange = [&](int start, int end) {
                start = std::clamp(start, 0, n);
                end = std::clamp(end, 0, n);
                if (end <= start) {
                    return 0.0;
                }
                return (prefix[end] - prefix[start]) / (end - start);
            };

            for (int i = lta; i < n; ++i) {
                const double staMean = meanRange(i - sta, i);
                const double ltaMean = meanRange(i - lta, i);
                const double ratio = (ltaMean < 1e-9) ? 0.0 : (staMean / ltaMean);
                if (pPick < 0 && ratio > pThreshold) {
                    pPick = i;
                }
                if (pPick >= 0 && i > pPick + minGap && ratio > sThreshold) {
                    sPick = i;
                    break;
                }
            }
        }

        auto segmentVariance = [&](int begin, int end) {
            const int len = end - begin;
            if (len <= 1) {
                return 1e-9;
            }
            double sum = 0.0;
            for (int i = begin; i < end; ++i) {
                sum += assistSignal[i];
            }
            const double mean = sum / len;
            double var = 0.0;
            for (int i = begin; i < end; ++i) {
                const double d = assistSignal[i] - mean;
                var += d * d;
            }
            var /= (len - 1);
            return std::max(var, 1e-9);
        };

        auto bestAicIndex = [&](int start, int stop) {
            int bestIdx = -1;
            double bestVal = std::numeric_limits<double>::max();
            for (int k = start; k < stop; ++k) {
                const int leftLen = k;
                const int rightLen = n - k;
                if (leftLen <= 2 || rightLen <= 2) {
                    continue;
                }
                const double v1 = segmentVariance(0, k);
                const double v2 = segmentVariance(k, n);
                const double aic = leftLen * std::log(v1) + rightLen * std::log(v2);
                if (aic < bestVal) {
                    bestVal = aic;
                    bestIdx = k;
                }
            }
            return bestIdx;
        };

        const int pStop = std::clamp(static_cast<int>(std::round(n * aicPSplitRatioSpin->value())), 30, n - 30);
        const int sStart = std::clamp(static_cast<int>(std::round(n * aicSSplitRatioSpin->value())), 30, n - 30);
        const int pAic = bestAicIndex(20, pStop);
        const int sAic = bestAicIndex(std::max(20, sStart), n - 20);

        if (pAic >= 0) {
            pPick = pAic;
        }
        if (sAic >= 0) {
            sPick = sAic;
        }

        bool changed = false;
        if (pPick >= 0) {
            WaveformWidget::PickMarker p;
            p.sampleIndex = pPick;
            p.channel = 0;
            p.phase = "P";
            p.suggested = true;
            upsertFileMarker(filePath, p);
            changed = true;
        }
        if (sPick >= 0) {
            WaveformWidget::PickMarker s;
            s.sampleIndex = sPick;
            s.channel = 0;
            s.phase = "S";
            s.suggested = true;
            upsertFileMarker(filePath, s);
            changed = true;
        }

        ++processed;
        if (changed) {
            ++updated;
        }
    }

    if (!pickCurrentFilePath.isEmpty()) {
        waveWidget->setPickMarkers(pickMarkersByFile.value(pickCurrentFilePath));
        refreshPickTable();
    }
    refreshPickInfo();

    statusBar()->showMessage(QString("Batch auto-pick finished. processed=%1 updated=%2")
                                 .arg(processed)
                                 .arg(updated),
                             6000);
}

void MainWindow::acceptSuggestedPickMarkers() {
    if (pickCurrentFilePath.isEmpty() || !pickMarkersByFile.contains(pickCurrentFilePath)) {
        return;
    }

    auto &markers = pickMarkersByFile[pickCurrentFilePath];
    for (auto &m : markers) {
        m.suggested = false;
    }
    waveWidget->setPickMarkers(markers);
    refreshPickTable();
    refreshPickInfo();
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
        // Use sensor id (folder name) as trace label when available
        const QString sensorLabel = dataFileSensors.value(filePath, QFileInfo(filePath).fileName());
        traceNames.push_back(sensorLabel);
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

    if (isDataMode()) {
        dataShowSlice = false;
        if (dataSliceButton) {
            dataSliceButton->setText("Show Slice");
        }
    }

    if (loadSingleFile(currentFilePath)) {
        statusBar()->showMessage("Loaded full waveform data", 5000);
    }
}

void MainWindow::toggleDataSliceMode() {
    dataShowSlice = !dataShowSlice;
    if (dataSliceButton) {
        dataSliceButton->setText(dataShowSlice ? "Show Full Waveform" : "Show Slice");
    }

    if (!currentFilePath.isEmpty()) {
        if (!currentRawSamples.isEmpty() && currentChannelNames.size() > 0) {
            QVector<QVector<double>> displaySamples = currentRawSamples;
            int sliceStartIndex = 0;
            if (dataShowSlice && buildDataSliceSamples(currentRawSamples, displaySamples, sliceStartIndex)) {
                currentSignal.clear();
                currentSignal.reserve(displaySamples.size());
                for (const auto &row : displaySamples) {
                    currentSignal.push_back(row.isEmpty() ? 0.0 : row[0]);
                }
                waveWidget->setData(displaySamples, currentChannelNames);
                currentViewStart = waveWidget->visibleStartSample();
                currentViewEnd = waveWidget->visibleEndSample();
                refreshAnalysisWidgets();
                statusBar()->showMessage(QString("Data slice loaded: display=(%1,%2) raw=(%3,%4)")
                    .arg(displaySamples.size())
                    .arg(displaySamples.isEmpty() ? 0 : displaySamples[0].size())
                    .arg(currentRawSamples.size())
                    .arg(currentRawSamples.isEmpty() ? 0 : currentRawSamples[0].size()),
                    4000);
                return;
            }
        }
        loadSingleFile(currentFilePath);
    }
}

void MainWindow::sendToPick() {
    if (currentFiles.isEmpty()) {
        QMessageBox::information(this, "Send to Pick", "Please load data files first.");
        return;
    }

    receivedPickFileList = currentFiles;
    receivedPickDataRoot = currentDataRoot;
    receivedPickDataProject = currentDataProject;
    receivedPickEventTime = currentEventTime;
    receivedPickFileSensors = dataFileSensors;

    statusBar()->showMessage(QString("Sent %1 data files to Pick View").arg(currentFiles.size()), 3000);
    
    modeTabs->setCurrentIndex(1);
}

void MainWindow::receiveFromDataView() {
    if (receivedPickFileList.isEmpty()) {
        QMessageBox::information(this, "Receive from Data View", "No data received yet. Use 'Send to Pick' in Data View first.");
        return;
    }

    pickCurrentFiles = receivedPickFileList;
    pickCurrentFolder = receivedPickDataRoot;
    currentDataRoot = receivedPickDataRoot;
    currentDataProject = receivedPickDataProject;
    currentEventTime = receivedPickEventTime;
    dataFileSensors = receivedPickFileSensors;
    pickShowFullWaveform = false;
    pickToggleFullButton->setText("Show Full Waveform");

    pickFolderList->clear();
    pickFileList->clear();
    pickMarkersByFile.clear();

    QDir rootDir(receivedPickDataRoot);
    QString rootPath = rootDir.absolutePath();
    pickFolderEdit->setText(QDir::toNativeSeparators(rootPath));

    auto *rootItem = new QListWidgetItem(rootDir.dirName().isEmpty() ? rootPath : rootDir.dirName());
    rootItem->setToolTip(QDir::toNativeSeparators(rootPath));
    rootItem->setData(Qt::UserRole, rootPath);
    pickFolderList->addItem(rootItem);

    // 使用 populateFilesForFolder 来正确填充文件列表，并且显示全路径
    for (const QString &filePath : receivedPickFileList) {
        auto *item = new QListWidgetItem(QDir::toNativeSeparators(filePath));
        item->setToolTip(QDir::toNativeSeparators(filePath));
        item->setData(Qt::UserRole, filePath);
        if (dataFileSensors.contains(filePath)) {
            item->setData(Qt::UserRole + 1, dataFileSensors.value(filePath));
        }
        pickFileList->addItem(item);
    }

    if (!receivedPickFileList.isEmpty()) {
        pickFileList->setCurrentRow(0);
        loadPickFile(receivedPickFileList.first());
    }

    statusBar()->showMessage(QString("Loaded %1 data files from Data View").arg(receivedPickFileList.size()), 3000);
}

void MainWindow::togglePickWaveformMode() {
    pickShowFullWaveform = !pickShowFullWaveform;
    if (pickShowFullWaveform) {
        pickToggleFullButton->setText("Show Cropped Waveform");
    } else {
        pickToggleFullButton->setText("Show Full Waveform");
    }

    if (!pickCurrentFilePath.isEmpty()) {
        loadPickFile(pickCurrentFilePath);
    }
}

bool MainWindow::buildPickWindowSamples(const WaveData &curr, const QString &sensorId, QVector<QVector<double>> &outSamples, int &usedStartIndex) const {
    if (!currentEventTime.isValid() || sensorId.isEmpty() || currentDataRoot.isEmpty() || currentDataProject.isEmpty()) {
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

    const int currCount = curr.samples.size();
    WaveData prev;
    WaveData next;
    if (startIndex < 0) {
        const QString prevPath = findMinuteFile(currentEventTime.addSecs(-60));
        if (!prevPath.isEmpty()) {
            QString err;
            readWaveformFileAuto(prevPath, prev, err);
        }
    }
    if (endIndex > currCount) {
        const QString nextPath = findMinuteFile(currentEventTime.addSecs(60));
        if (!nextPath.isEmpty()) {
            QString err;
            readWaveformFileAuto(nextPath, next, err);
        }
    }

    const int channelCount = curr.isThreeComponent ? 3 : (curr.samples.isEmpty() ? 1 : curr.samples[0].size());
    const int outChannels = std::max(1, channelCount);
    outSamples.clear();
    outSamples.reserve(std::max(0, endIndex - startIndex));

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

    if (outSamples.isEmpty()) {
        return false;
    }

    usedStartIndex = startIndex;
    return true;
}

bool MainWindow::loadPickFile(const QString &filePath) {
    WaveData wd;
    QString error;

    if (!readWaveformFileAuto(filePath, wd, error)) {
        QMessageBox::warning(this, "Read error", error);
        return false;
    }

    pickCurrentFilePath = filePath;
    lastPickFilePath = filePath;
    waveWidget->clearAssistCurve();
    waveWidget->clearAssistRange();

    const QFileInfo fi(filePath);
    if (fi.exists()) {
        const bool isReceivedMode = !receivedPickFileList.isEmpty();
        if (!isReceivedMode) {
            const QString folderPath = fi.absolutePath();
            if (pickCurrentFolder != folderPath) {
                pickCurrentFolder = folderPath;
                pickCurrentFiles.clear();
                pickFolderList->clear();
                pickFileList->clear();
            }

            if (!pickCurrentFiles.contains(filePath)) {
                pickCurrentFiles.push_back(filePath);
            }

            pickFolderEdit->setText(QDir::toNativeSeparators(pickCurrentFolder));

            bool foundItem = false;
            for (int i = 0; i < pickFileList->count(); ++i) {
                auto *existing = pickFileList->item(i);
                if (existing && existing->data(Qt::UserRole).toString() == filePath) {
                    pickFileList->setCurrentRow(i);
                    foundItem = true;
                    break;
                }
            }
            if (!foundItem) {
                auto *item = new QListWidgetItem(fi.fileName());
                item->setToolTip(QDir::toNativeSeparators(filePath));
                item->setData(Qt::UserRole, filePath);
                pickFileList->addItem(item);
                pickFileList->setCurrentItem(item);
            }
        } else {
            bool foundItem = false;
            for (int i = 0; i < pickFileList->count(); ++i) {
                auto *existing = pickFileList->item(i);
                if (existing && existing->data(Qt::UserRole).toString() == filePath) {
                    pickFileList->setCurrentRow(i);
                    foundItem = true;
                    break;
                }
            }
            if (!foundItem) {
                auto *item = new QListWidgetItem(QDir::toNativeSeparators(filePath));
                item->setToolTip(QDir::toNativeSeparators(filePath));
                item->setData(Qt::UserRole, filePath);
                pickFileList->addItem(item);
                pickFileList->setCurrentItem(item);
            }
        }
    }

    pickCurrentChannelNames.clear();
    if (wd.isThreeComponent) {
        pickCurrentChannelNames << "X" << "Y" << "Z";
    } else {
        pickCurrentChannelNames << "Amp";
    }

    pickCurrentRawSamples = wd.samples; // 【TEMP】直接用原始数据！

    QVector<QVector<double>> displaySamples = pickCurrentRawSamples;
    int pickOffset = 0;
    if (!pickShowFullWaveform && currentEventTime.isValid() && !currentDataSensorId.isEmpty()) {
        WaveData viewData;
        viewData.samples = pickCurrentRawSamples;
        viewData.isThreeComponent = wd.isThreeComponent;
        QVector<QVector<double>> windowSamples;
        int windowOffset = 0;
        if (buildPickWindowSamples(viewData, currentDataSensorId, windowSamples, windowOffset) && !windowSamples.isEmpty()) {
            displaySamples = windowSamples;
            pickOffset = windowOffset;
        }
    }

    currentSignal.clear();
    currentSignal.reserve(displaySamples.size());
    for (const auto &row : displaySamples) {
        currentSignal.push_back(row.isEmpty() ? 0.0 : row[0]);
    }

    waveWidget->setData(displaySamples, pickCurrentChannelNames);

    QVector<WaveformWidget::PickMarker> adjustedMarkers;
    for (auto m : pickMarkersByFile.value(filePath)) {
        m.sampleIndex -= pickOffset;
        adjustedMarkers.push_back(m);
    }
    waveWidget->setPickMarkers(adjustedMarkers);

    refreshPickTable();
    refreshPickInfo();
    statusBar()->showMessage(QString("Pick loaded: %1  display=(%2,%3)  raw=(%4,%5)")
        .arg(QFileInfo(filePath).fileName())
        .arg(displaySamples.size())
        .arg(displaySamples.isEmpty() ? 0 : displaySamples[0].size())
        .arg(pickCurrentRawSamples.size())
        .arg(pickCurrentRawSamples.isEmpty() ? 0 : pickCurrentRawSamples[0].size()), 4000);
    return true;
}

void MainWindow::refreshPickInfo() {
    const int fileCount = pickCurrentFiles.size();
    int totalMarkers = 0;
    for (auto it = pickMarkersByFile.constBegin(); it != pickMarkersByFile.constEnd(); ++it) {
        totalMarkers += it.value().size();
    }

    QString text = QString("Pick folder: %1\nFiles: %2\nTotal markers: %3")
                       .arg(QDir::toNativeSeparators(pickCurrentFolder))
                       .arg(fileCount)
                       .arg(totalMarkers);
    text += QString("\nFilter: %1 (HP=%2Hz, LP=%3Hz)")
                .arg(pickFilterCombo->currentText())
                .arg(pickHighpassHzSpin->value(), 0, 'f', 1)
                .arg(pickLowpassHzSpin->value(), 0, 'f', 1);
    text += QString("\nSTA/LTA: STA=%1 LTA=%2 Pth=%3 Sth=%4 Gap=%5")
                .arg(staWindowSpin->value())
                .arg(ltaWindowSpin->value())
                .arg(staPThresholdSpin->value(), 0, 'f', 2)
                .arg(staSThresholdSpin->value(), 0, 'f', 2)
                .arg(staMinGapSpin->value());
    int rangeStart = 0;
    int rangeEnd = 0;
    if (waveWidget->assistRange(rangeStart, rangeEnd)) {
        text += QString("\nAssist range: [%1, %2] (%3 samples)")
                    .arg(rangeStart)
                    .arg(rangeEnd)
                    .arg(rangeEnd - rangeStart);
    } else {
        text += "\nAssist range: Full waveform";
    }
    if (!pickCurrentFilePath.isEmpty()) {
        text += QString("\nCurrent file: %1").arg(QFileInfo(pickCurrentFilePath).fileName());
    }
    pickInfoLabel->setText(text);
}

void MainWindow::refreshPickTable() {
    pickTable->setRowCount(0);
    if (pickCurrentFilePath.isEmpty()) {
        return;
    }

    const auto markers = pickMarkersByFile.value(pickCurrentFilePath);
    pickTable->setRowCount(markers.size());
    for (int i = 0; i < markers.size(); ++i) {
        const auto &m = markers[i];
        pickTable->setItem(i, 0, new QTableWidgetItem(m.phase));
        pickTable->setItem(i, 1, new QTableWidgetItem(QString::number(m.channel + 1)));
        pickTable->setItem(i, 2, new QTableWidgetItem(QString::number(m.sampleIndex)));
        pickTable->setItem(i, 3, new QTableWidgetItem(QString::number(static_cast<double>(m.sampleIndex) / kSampleRateHz, 'f', 4)));
        pickTable->setItem(i, 4, new QTableWidgetItem(m.suggested ? "assist" : "manual"));
    }
}

QString MainWindow::pickMarkerStorePath() const {
    if (pickCurrentFolder.isEmpty()) {
        return QString();
    }
    return QDir(pickCurrentFolder).filePath("pick_markers.json");
}

void MainWindow::loadPickMarkersFromDisk() {
    if (pickCurrentFolder.isEmpty()) {
        return;
    }

    const QString storePath = pickMarkerStorePath();
    QFile f(storePath);
    if (!f.exists()) {
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray data = f.readAll();
    f.close();

    const auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }

    const QJsonArray items = doc.object().value("items").toArray();
    for (const auto &v : items) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject obj = v.toObject();
        const QString rel = obj.value("file").toString();
        if (rel.isEmpty()) {
            continue;
        }

        const QString absPath = QDir(pickCurrentFolder).absoluteFilePath(rel);
        WaveformWidget::PickMarker m;
        m.phase = obj.value("phase").toString();
        m.channel = obj.value("channel").toInt();
        m.sampleIndex = obj.value("sample").toInt();
        m.suggested = (obj.value("source").toString() == "assist");
        if (m.phase.isEmpty()) {
            continue;
        }
        pickMarkersByFile[absPath].push_back(m);
    }
}

QVector<double> MainWindow::applyLowPassFilter(const QVector<double> &signal, double cutoffHz) const {
    if (signal.isEmpty() || cutoffHz <= 0.0) {
        return signal;
    }

    const int n = static_cast<int>(signal.size());
    int window = static_cast<int>(std::round(static_cast<double>(kSampleRateHz) / cutoffHz));
    window = std::clamp(window, 1, 2001);
    if ((window % 2) == 0) {
        ++window;
    }
    const int half = window / 2;

    QVector<double> out(n, 0.0);
    for (int i = 0; i < n; ++i) {
        const int b = std::max(0, i - half);
        const int e = std::min(n - 1, i + half);
        double sum = 0.0;
        for (int j = b; j <= e; ++j) {
            sum += signal[j];
        }
        out[i] = sum / (e - b + 1);
    }
    return out;
}

QVector<QVector<double>> MainWindow::applyFilterToSamples(const QVector<QVector<double>> &samples) const {
    if (samples.isEmpty()) {
        return samples;
    }

    const int n = samples.size();
    const int c = samples[0].size();
    QVector<QVector<double>> out(n, QVector<double>(c, 0.0));

    for (int ch = 0; ch < c; ++ch) {
        QVector<double> channelSig;
        channelSig.reserve(n);
        for (const auto &row : samples) {
            channelSig.push_back((ch < row.size()) ? row[ch] : 0.0);
        }

        const QVector<double> filtered = buildAssistSignalFrom(channelSig);
        for (int i = 0; i < n; ++i) {
            out[i][ch] = filtered[i];
        }
    }

    return out;
}

QVector<double> MainWindow::buildAssistSignal() const {
    return buildAssistSignalFrom(currentSignal);
}

QVector<double> MainWindow::buildAssistSignalFrom(const QVector<double> &signal) const {
    if (signal.isEmpty()) {
        return {};
    }

    const QString filterName = pickFilterCombo->currentText();
    const double hp = pickHighpassHzSpin->value();
    const double lp = pickLowpassHzSpin->value();

    if (filterName == "No Filter") {
        return signal;
    }

    if (filterName == "Low-pass") {
        return applyLowPassFilter(signal, lp);
    }

    if (filterName == "High-pass") {
        const QVector<double> low = applyLowPassFilter(signal, hp);
        QVector<double> out(signal.size(), 0.0);
        for (int i = 0; i < signal.size(); ++i) {
            out[i] = signal[i] - low[i];
        }
        return out;
    }

    if (filterName == "Band-pass") {
        if (lp <= hp) {
            return signal;
        }
        const QVector<double> lowHp = applyLowPassFilter(signal, hp);
        QVector<double> high(signal.size(), 0.0);
        for (int i = 0; i < signal.size(); ++i) {
            high[i] = signal[i] - lowHp[i];
        }
        return applyLowPassFilter(high, lp);
    }

    return signal;
}

bool MainWindow::readWaveformFileAuto(const QString &filePath, WaveData &wd, QString &error) const {
    QFileInfo fi(filePath);
    const QString ext = fi.suffix().toLower();
    if (ext == "csv" || ext == "txt") {
        return WaveformReader::readTextWaveFile(filePath, wd, error);
    }

    if (looksLikeTextWaveFile(filePath)) {
        if (WaveformReader::readTextWaveFile(filePath, wd, error)) {
            return true;
        }
        error.clear();
    }

    if (WaveformReader::readWaveFile(filePath, wd, error)) {
        return true;
    }

    QString err2;
    if (WaveformReader::readTextWaveFile(filePath, wd, err2)) {
        return true;
    }

    return false;
}

void MainWindow::upsertFileMarker(const QString &filePath, const WaveformWidget::PickMarker &marker) {
    auto &markers = pickMarkersByFile[filePath];
    for (auto &m : markers) {
        if (m.channel == marker.channel && m.phase == marker.phase && m.suggested == marker.suggested) {
            m = marker;
            return;
        }
    }
    markers.push_back(marker);
}

void MainWindow::reloadPickWaveformDisplay() {
    if (pickCurrentRawSamples.isEmpty()) {
        return;
    }

    QVector<QVector<double>> dataToSet = pickCurrentRawSamples;
    int pickOffset = 0;
    if (!pickShowFullWaveform && currentEventTime.isValid() && !currentDataSensorId.isEmpty()) {
        WaveData viewData;
        viewData.samples = pickCurrentRawSamples;
        viewData.isThreeComponent = (pickCurrentChannelNames.size() == 3);
        QVector<QVector<double>> windowSamples;
        int windowOffset = 0;
        if (buildPickWindowSamples(viewData, currentDataSensorId, windowSamples, windowOffset) && !windowSamples.isEmpty()) {
            dataToSet = windowSamples;
            pickOffset = windowOffset;
        }
    }

    if (pickDisplayCombo->currentIndex() != 0) {
        dataToSet = applyFilterToSamples(dataToSet);
    }

    waveWidget->setData(dataToSet, pickCurrentChannelNames);

    QVector<WaveformWidget::PickMarker> adjustedMarkers;
    for (const auto &marker : pickMarkersByFile.value(pickCurrentFilePath)) {
        WaveformWidget::PickMarker shifted = marker;
        shifted.sampleIndex -= pickOffset;
        adjustedMarkers.push_back(shifted);
    }
    waveWidget->setPickMarkers(adjustedMarkers);

    currentSignal.clear();
    currentSignal.reserve(dataToSet.size());
    for (const auto &row : dataToSet) {
        currentSignal.push_back(row.isEmpty() ? 0.0 : row[0]);
    }
}

bool MainWindow::isDataMode() const {
    return modeTabs->currentIndex() == 0 && waveTabs->currentIndex() == 1;
}
