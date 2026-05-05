#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QDateTime>
#include <QMap>

class QCloseEvent;
#include "waveformwidget.h"

class QLineEdit;
class QPushButton;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QTabWidget;
class QComboBox;
class QTableWidget;
class QSpinBox;
class QDoubleSpinBox;
class SpectrumWidget;
class SpectrogramWidget;
struct WaveData;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void browseEventFolder();
    void browseDataEventFolder();
    void browseDataFolder();
    void browsePickFolder();
    void loadEventFolder();
    void loadDataFolder();
    void loadPickFolder();
    void onEventFolderItemClicked(QListWidgetItem *item);
    void onDataFolderItemClicked(QListWidgetItem *item);
    void onEventFileItemClicked(QListWidgetItem *item);
    void onDataFileItemClicked(QListWidgetItem *item);
    void onPickFolderItemClicked(QListWidgetItem *item);
    void onPickFileItemClicked(QListWidgetItem *item);
    void onWaveViewChanged(int startSample, int endSample);
    void onShowSpectrumChanged(bool checked);
    void onShowSpectrogramChanged(bool checked);
    void onPickModeChanged(int index);
    void onPickMarkerAdded(int sampleIndex, int channel, const QString &phase, bool suggested);
    void onPickMarkerRemoved(int sampleIndex, int channel, const QString &phase, bool suggested);
    void clearCurrentPickMarkers();
    void savePickMarkers();
    void exportPickCsv();
    void runStaLtaAssist();
    void runAicAssist();
    void runBatchAutoPick();
    void acceptSuggestedPickMarkers();
    void exportCurrentPng();
    void exportOverviewPng();
    void previewStacked();
    void exportStackedPng();
    void showFullWaveform();
    void toggleDataSliceMode();
    void sendToPick();
    void receiveFromDataView();
    void togglePickWaveformMode();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QStringList collectFiles(const QString &folder, bool recursive) const;
    QStringList resolveDataFiles(const QString &eventFolder,
                                 const QString &dataRoot,
                                 QMap<QString, QString> &fileSensors,
                                 QString &projectName,
                                 QDateTime &eventTime) const;
    void populateFolderAndFileColumns(QListWidget *folderListWidget,
                                      QListWidget *fileListWidget,
                                      const QString &folder,
                                      const QStringList &files);
    void populateFilesForFolder(QListWidget *fileListWidget, const QStringList &files);
    void refreshAnalysisWidgets();
    bool loadSingleFile(const QString &filePath);
    bool loadDataEventWindow(const QString &filePath, const QString &sensorId);
    bool buildStackedTraces(QVector<QVector<double>> &traces, QStringList &traceNames) const;
    bool isDataMode() const;
    bool loadPickFile(const QString &filePath);
    bool buildPickWindowSamples(const WaveData &curr, const QString &sensorId, QVector<QVector<double>> &outSamples, int &usedStartIndex) const;
    void refreshPickInfo();
    void refreshPickTable();
    QString pickMarkerStorePath() const;
    void loadPickMarkersFromDisk();
    QVector<double> applyLowPassFilter(const QVector<double> &signal, double cutoffHz) const;
    QVector<QVector<double>> applyFilterToSamples(const QVector<QVector<double>> &samples) const;
    QVector<double> buildAssistSignal() const;
    QVector<double> buildAssistSignalFrom(const QVector<double> &signal) const;
    bool readWaveformFileAuto(const QString &filePath, WaveData &wd, QString &error) const;
    void upsertFileMarker(const QString &filePath, const WaveformWidget::PickMarker &marker);
    void reloadPickWaveformDisplay();
    bool buildDataSliceSamples(const QVector<QVector<double>> &source, QVector<QVector<double>> &outSamples, int &usedStartIndex) const;
    void reloadDataWaveformDisplay();
    void saveSessionState() const;
    void restoreSessionState();

    QTabWidget *modeTabs;
    QTabWidget *waveTabs;

    QLineEdit *eventFolderEdit;
    QPushButton *browseEventButton;
    QPushButton *loadEventButton;

    QLineEdit *dataEventFolderEdit;
    QPushButton *browseDataEventButton;
    QPushButton *loadDataEventButton;
    QLineEdit *dataFolderEdit;
    QPushButton *browseDataButton;

    QCheckBox *recursiveCheck;
    QCheckBox *normalizeCheck;
    QCheckBox *folderStackedCheck;
    QCheckBox *findDataCheck;
    QCheckBox *showSpectrumCheck;
    QCheckBox *showSpectrogramCheck;

    QListWidget *eventFolderList;
    QListWidget *eventFileList;
    QListWidget *dataFolderList;
    QListWidget *dataFileList;

    QLabel *eventInfoLabel;
    QLabel *dataInfoLabel;
    QLabel *pickInfoLabel;

    QLineEdit *pickFolderEdit;
    QPushButton *browsePickFolderButton;
    QPushButton *loadPickFolderButton;
    QListWidget *pickFolderList;
    QListWidget *pickFileList;

    QComboBox *pickModeCombo;
    QComboBox *pickDisplayCombo;
    QComboBox *pickFilterCombo;
    QDoubleSpinBox *pickHighpassHzSpin;
    QDoubleSpinBox *pickLowpassHzSpin;
    QSpinBox *staWindowSpin;
    QSpinBox *ltaWindowSpin;
    QDoubleSpinBox *staPThresholdSpin;
    QDoubleSpinBox *staSThresholdSpin;
    QSpinBox *staMinGapSpin;
    QDoubleSpinBox *aicPSplitRatioSpin;
    QDoubleSpinBox *aicSSplitRatioSpin;
    QPushButton *clearPickButton;
    QPushButton *savePickButton;
    QPushButton *exportPickCsvButton;
    QPushButton *runStaLtaButton;
    QPushButton *runAicButton;
    QPushButton *runBatchAutoPickButton;
    QPushButton *acceptSuggestedButton;
    QTableWidget *pickTable;

    WaveformWidget *waveWidget;
    SpectrumWidget *spectrumWidget;
    SpectrogramWidget *spectrogramWidget;

    QPushButton *saveCurrentButton;
    QPushButton *saveOverviewButton;
    QPushButton *previewStackedButton;
    QPushButton *saveStackedButton;
    QPushButton *showFullButton;
    QPushButton *sendToPickButton;
    QPushButton *receiveFromDataViewButton;
    QPushButton *pickToggleFullButton;
    QPushButton *dataSliceButton;
    
    QStringList receivedPickFileList;
    QString receivedPickDataRoot;
    QString receivedPickDataProject;
    QDateTime receivedPickEventTime;
    QMap<QString, QString> receivedPickFileSensors;
    bool pickShowFullWaveform = false;

    QString lastEventFolderPath;
    QString lastEventFilePath;
    QString lastDataEventFolderPath;
    QString lastDataFolderPath;
    QString lastDataFilePath;
    QString lastPickFolderPath;
    QString lastPickFilePath;
    int lastMainTabIndex = 0;
    int lastWaveTabIndex = 0;

    QStringList currentFiles;
    QString currentFolder;
    QString currentFolderPath;
    QMap<QString, QStringList> folderFiles;
    QMap<QString, QString> dataFileSensors;
    QString currentFilePath;
    QString currentDataSensorId;
    QString currentDataRoot;
    QString currentDataProject;
    QDateTime currentEventTime;
    QVector<QVector<double>> currentRawSamples;
    QStringList currentChannelNames;
    bool dataShowSlice = false;
    QVector<double> currentSignal;
    int currentViewStart = 0;
    int currentViewEnd = 0;

    QString pickCurrentFolder;
    QStringList pickCurrentFiles;
    QString pickCurrentFilePath;
    QVector<QVector<double>> pickCurrentRawSamples;
    QStringList pickCurrentChannelNames;
    QMap<QString, QVector<WaveformWidget::PickMarker>> pickMarkersByFile;
};

#endif
