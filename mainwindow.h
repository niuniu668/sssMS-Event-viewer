#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QDateTime>
#include <QMap>

class QLineEdit;
class QPushButton;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QTabWidget;
class WaveformWidget;
class SpectrumWidget;
class SpectrogramWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void browseEventFolder();
    void browseDataEventFolder();
    void browseDataFolder();
    void loadEventFolder();
    void loadDataFolder();
    void onEventFolderItemClicked(QListWidgetItem *item);
    void onDataFolderItemClicked(QListWidgetItem *item);
    void onEventFileItemClicked(QListWidgetItem *item);
    void onDataFileItemClicked(QListWidgetItem *item);
    void onWaveViewChanged(int startSample, int endSample);
    void onShowSpectrumChanged(bool checked);
    void onShowSpectrogramChanged(bool checked);
    void exportCurrentPng();
    void exportOverviewPng();
    void previewStacked();
    void exportStackedPng();
    void showFullWaveform();

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

    QTabWidget *modeTabs;

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

    WaveformWidget *waveWidget;
    SpectrumWidget *spectrumWidget;
    SpectrogramWidget *spectrogramWidget;

    QPushButton *saveCurrentButton;
    QPushButton *saveOverviewButton;
    QPushButton *previewStackedButton;
    QPushButton *saveStackedButton;
    QPushButton *showFullButton;

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
    QVector<double> currentSignal;
    int currentViewStart = 0;
    int currentViewEnd = 0;
};

#endif
