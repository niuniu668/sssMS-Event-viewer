# Qt Waveform Viewer v3.0.0

发布日期：2026-04-25

## 主要更新
- 将界面整理为两个选项卡：Event View 和 Data View。
- 补全 Event 到 Data 的查找逻辑，按事件时间和传感器目录定位对应 data 文件。
- Data 页支持显示找到的 data 文件完整路径，并优先展示 Event 文件夹列表。
- 事件波形与 data 波形支持按事件时间截取 4000 点窗口，用于堆叠展示与后续分析。
- 完成 Release 打包，生成可分发的 dist 产物。

## 发布产物
- [dist/QtWaveformViewer-win64.zip](dist/QtWaveformViewer-win64.zip)
- [dist/QtWaveformViewer/QtWaveformViewer.exe](dist/QtWaveformViewer/QtWaveformViewer.exe)

## 备注
- 本次建议使用标签名 v3.0.0 作为 GitHub Release tag。