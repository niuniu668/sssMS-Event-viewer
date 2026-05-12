import sys

file_path = 'waveformwidget.h'
with open(file_path, 'r', encoding='utf-8') as f:
    text = f.read()

# add signal for hover to header
signal_to_add = "    void mouseHovered(int channelIndex, double timeSec, double amplitude);\n"
if "void mouseHovered" not in text:
    text = text.replace('    void pickMarkerRemoved(int sampleIndex, int channel, const QString &phase, bool suggested);', '    void pickMarkerRemoved(int sampleIndex, int channel, const QString &phase, bool suggested);\n' + signal_to_add)

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(text)

file_path_cpp = 'waveformwidget.cpp'
with open(file_path_cpp, 'r', encoding='utf-8') as f:
    text_cpp = f.read()

# Add hover tracking method
event_override = """void WaveformWidget::mouseMoveEvent(QMouseEvent *event) {
    if (traces.isEmpty() || numSamples == 0) {
        emit mouseHovered(-1, 0, 0); // Clear
        return QWidget::mouseMoveEvent(event);
    }
    
    double h = height() / (double)traces.size();
    int ch = event->pos().y() / h;
    if (ch >= 0 && ch < traces.size()) {
        int samp = (event->pos().x() / (double)width()) * currentViewSamples + viewStartSample;
        samp = std::clamp(samp, 0, numSamples - 1);
        double t = static_cast<double>(samp) / sampleRateHz;
        double a = traces[ch][samp];
        emit mouseHovered(ch, t, a);
    } else {
        emit mouseHovered(-1, 0, 0);
    }
    
"""
if "void WaveformWidget::mouseMoveEvent(QMouseEvent *event) {" not in text_cpp:
    # Let's replace the first line of mouseReleaseEvent temporarily, or just append
    # Wait, usually mouseReleaseEvent or mousePressEvent exists. Let's find mousePressEvent.
    pass

# We will just inject mouseMoveEvent right before mousePressEvent
if "void WaveformWidget::mouseMoveEvent" not in text_cpp:
    text_cpp = text_cpp.replace("void WaveformWidget::mousePressEvent(QMouseEvent *event) {", event_override + "    // Fallthrough for drawing if needed in future\n}\n\nvoid WaveformWidget::mousePressEvent(QMouseEvent *event) {")

with open(file_path_cpp, 'w', encoding='utf-8') as f:
    f.write(text_cpp)
