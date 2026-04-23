#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QCheckBox;
class QListWidget;
class QLabel;
class QModelIndex;
class WaveformWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void browseFolder();
    void loadFolder();
    void onFileClicked(const QModelIndex &index);
    void exportCurrentPng();
    void exportOverviewPng();
    void previewStacked();
    void exportStackedPng();

private:
    QStringList collectFiles(const QString &folder, bool recursive) const;
    bool loadSingleFile(const QString &filePath);
    bool buildStackedTraces(QVector<QVector<double>> &traces, QStringList &traceNames) const;

    QLineEdit *folderEdit;
    QPushButton *browseButton;
    QPushButton *loadButton;
    QCheckBox *recursiveCheck;
    QCheckBox *normalizeCheck;
    QListWidget *fileList;
    QLabel *infoLabel;
    WaveformWidget *waveWidget;
    QPushButton *saveCurrentButton;
    QPushButton *saveOverviewButton;
    QPushButton *previewStackedButton;
    QPushButton *saveStackedButton;

    QStringList currentFiles;
};

#endif
