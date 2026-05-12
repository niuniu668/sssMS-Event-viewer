import sys

file_path = 'mainwindow.cpp'
with open(file_path, 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('    topControlLayout->addWidget(pickTopWidget);', '')

dock_replacement = """
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
"""

target_str = """    auto *pickTopWidget = new QWidget(central);
    auto *pickTopLayout = new QVBoxLayout(pickTopWidget);
    pickTopLayout->setContentsMargins(0, 0, 0, 0);

    auto *pickTopRow1 = new QHBoxLayout();
    auto *pickTopRow2 = new QHBoxLayout();
    auto *pickTopRow3 = new QHBoxLayout();"""
text = text.replace(target_str, '')

text_to_delete = """    pickTopRow1->addWidget(new QLabel("Mode:"));
    pickTopRow1->addWidget(pickModeCombo);
    pickTopRow1->addWidget(pickDisplayCombo);
    pickTopRow1->addWidget(runStaLtaButton);
    pickTopRow1->addWidget(runAicButton);
    pickTopRow1->addWidget(runBatchAutoPickButton);
    pickTopRow1->addWidget(acceptSuggestedButton);
    pickTopRow1->addWidget(clearPickButton);
    pickTopRow1->addWidget(savePickButton);
    pickTopRow1->addWidget(exportPickCsvButton);
    pickTopRow1->addStretch();

    auto *pickFilterRow = new QHBoxLayout();
    pickTopRow2->addWidget(new QLabel("Filter:"));
    pickTopRow2->addWidget(pickFilterCombo);
    pickTopRow2->addWidget(new QLabel("HP(Hz):"));
    pickTopRow2->addWidget(pickHighpassHzSpin);
    pickTopRow2->addWidget(new QLabel("LP(Hz):"));
    pickTopRow2->addWidget(pickLowpassHzSpin);
    pickTopRow2->addStretch();

    auto *pickStaRow = new QHBoxLayout();
    pickTopRow3->addWidget(new QLabel("STA/LTA:"));
    pickTopRow3->addWidget(new QLabel("STA"));
    pickTopRow3->addWidget(staWindowSpin);
    pickTopRow3->addWidget(new QLabel("LTA"));
    pickTopRow3->addWidget(ltaWindowSpin);
    pickTopRow3->addWidget(new QLabel("P th"));
    pickTopRow3->addWidget(staPThresholdSpin);
    pickTopRow3->addWidget(new QLabel("S th"));
    pickTopRow3->addWidget(staSThresholdSpin);
    pickTopRow3->addWidget(new QLabel("Min gap"));
    pickTopRow3->addWidget(staMinGapSpin);
    pickTopRow3->addWidget(new QLabel("AIC P<=ratio"));
    pickTopRow3->addWidget(aicPSplitRatioSpin);
    pickTopRow3->addWidget(new QLabel("AIC S>=ratio"));
    pickTopRow3->addWidget(aicSSplitRatioSpin);
    pickTopRow3->addStretch();

    auto *pickAicRow = new QHBoxLayout();

    pickTopLayout->addLayout(pickTopRow1);
    pickTopLayout->addLayout(pickTopRow2);
    pickTopLayout->addLayout(pickTopRow3);"""
text = text.replace(text_to_delete, '')

text = text.replace('    auto *pickSplitRow = new QHBoxLayout();', dock_replacement + '\n    auto *pickSplitRow = new QHBoxLayout();')

connection_str = """    connect(modeTabs, &QTabWidget::currentChanged, this, [left, splitter, pickTopWidget](int idx) {
        const bool isPick = (idx == 1);
        pickTopWidget->setVisible(isPick);"""

new_connection_str = """    connect(modeTabs, &QTabWidget::currentChanged, this, [left, splitter, pickSettingsDock](int idx) {
        const bool isPick = (idx == 1);
        pickSettingsDock->setVisible(isPick);"""

text = text.replace(connection_str, new_connection_str)

text = text.replace('    pickTopWidget->setVisible(modeTabs->currentIndex() == 1);', '    pickSettingsDock->setVisible(modeTabs->currentIndex() == 1);\n')

text = text.replace('Q_UNUSED(pickModeRow);\n    Q_UNUSED(pickFilterRow);\n    Q_UNUSED(pickStaRow);\n    Q_UNUSED(pickAicRow);\n', '')

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(text)
