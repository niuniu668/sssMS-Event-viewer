import sys

file_path = 'mainwindow.cpp'
with open(file_path, 'r', encoding='utf-8') as f:
    text = f.read()

# Instead of passing the event directly from waveWidget, we can listen for mouse double click or shift click
# Let's add status-bar hovered coordinate trace

# Find the connect block near the bottom of MainWindow constructor
injection_point_status_bar = "    connect(showSpectrumCheck, &QCheckBox::toggled, this, &MainWindow::onShowSpectrumChanged);"

code_to_inject = """
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
"""

if "=== UX Enhancements: Status Bar coordinates ===" not in text:
    text = text.replace(injection_point_status_bar, code_to_inject + injection_point_status_bar)

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(text)

