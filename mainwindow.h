#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QMap>

class QLineEdit;
class QPushButton;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class WaveformWidget;
class SpectrumWidget;
class SpectrogramWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void browseFolder();
    void loadFolder();
    void onFolderItemClicked(QListWidgetItem *item);
    void onFileItemClicked(QListWidgetItem *item);
    void onWaveViewChanged(int startSample, int endSample);
    void onShowSpectrumChanged(bool checked);
    void onShowSpectrogramChanged(bool checked);
    void exportCurrentPng();
    void exportOverviewPng();
    void previewStacked();
    void exportStackedPng();

private:
    QStringList collectFiles(const QString &folder, bool recursive) const;
    void populateFolderAndFileColumns(const QString &folder, const QStringList &files);
    void populateFilesForFolder(const QString &folderPath);
    void refreshAnalysisWidgets();
    bool loadSingleFile(const QString &filePath);
    bool buildStackedTraces(QVector<QVector<double>> &traces, QStringList &traceNames) const;

    QLineEdit *folderEdit;
    QPushButton *browseButton;
    QPushButton *loadButton;
    QCheckBox *recursiveCheck;
    QCheckBox *normalizeCheck;
    QCheckBox *folderStackedCheck;
    QCheckBox *showSpectrumCheck;
    QCheckBox *showSpectrogramCheck;
    QListWidget *folderList;
    QListWidget *fileList;
    QLabel *infoLabel;
    WaveformWidget *waveWidget;
    SpectrumWidget *spectrumWidget;
    SpectrogramWidget *spectrogramWidget;
    QPushButton *saveCurrentButton;
    QPushButton *saveOverviewButton;
    QPushButton *previewStackedButton;
    QPushButton *saveStackedButton;

    QStringList currentFiles;
    QString currentFolder;
    QString currentFolderPath;
    QMap<QString, QStringList> folderFiles;
    QString currentFilePath;
    QVector<double> currentSignal;
    int currentViewStart = 0;
    int currentViewEnd = 0;
};

#endif
