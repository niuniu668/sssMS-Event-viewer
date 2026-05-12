import sys
import re

file_path = 'mainwindow.cpp'
with open(file_path, 'r', encoding='utf-8') as f:
    text = f.read()

# Add includes
includes = """#include <QShortcut>
#include <QMenu>
#include <QAction>
#include <QKeySequence>"""
if "#include <QShortcut>" not in text:
    text = text.replace('#include <QSet>', '#include <QSet>\n' + includes)

# Insert Shortcuts and Context Menu code at the end of MainWindow constructor
# Look for: connection block
injection_point = "    connect(modeTabs, &QTabWidget::currentChanged, this, [left, splitter, pickSettingsDock](int idx) {"

injection_code = """
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
        menu.setStyleSheet("QMenu { background-color: #333; color: white; border: 1px solid #555; } QMenu::item:selected { background-color: #2a82da; }");
        
        QAction *actB = menu.addAction("Mode: Browse (B)");
        QAction *actP = menu.addAction("Mode: Pick P (P)");
        QAction *actS = menu.addAction("Mode: Pick S (S)");
        QAction *actE = menu.addAction("Mode: Erase (E)");
        menu.addSeparator();
        QAction *actAutoStaLta = menu.addAction("Run STA/LTA Assist");
        QAction *actAutoAic = menu.addAction("Run AIC Assist");
        menu.addSeparator();
        QAction *actClear = menu.addAction("Clear Current Configs/Picks");

        connect(actB, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(0); });
        connect(actP, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(1); });
        connect(actS, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(2); });
        connect(actE, &QAction::triggered, this, [this](){ pickModeCombo->setCurrentIndex(3); });
        connect(actAutoStaLta, &QAction::triggered, this, &MainWindow::runStaLtaAssist);
        connect(actAutoAic, &QAction::triggered, this, &MainWindow::runAicAssist);
        connect(actClear, &QAction::triggered, this, &MainWindow::clearCurrentPickMarkers);

        menu.exec(waveWidget->mapToGlobal(pos));
    });

"""

if "new QShortcut" not in text:
    text = text.replace(injection_point, injection_code + injection_point)

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(text)
