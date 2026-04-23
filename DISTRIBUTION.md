# 发布打包说明

本仓库已提供固定输出目录的打包脚本：

- 脚本：scripts/package_dist.ps1
- 包装入口：scripts/package_dist.bat
- 输出目录：dist/QtWaveformViewer
- 输出压缩包：dist/QtWaveformViewer-win64.zip

## 使用步骤

1. 先用 Release 模式编译项目，得到 QtWaveformViewer.exe。
2. 打开 Qt 命令行终端（确保可用 windeployqt）。
3. 在仓库根目录运行：

```powershell
./scripts/package_dist.ps1
```

如果自动查找 exe 失败，手动指定：

```powershell
./scripts/package_dist.ps1 -ExePath "D:\path\to\QtWaveformViewer.exe"
```

如果系统里有多个 Qt（例如 Anaconda Qt5 和本机 Qt6），建议显式指定 windeployqt：

```powershell
./scripts/package_dist.ps1 \
	-ExePath "D:\path\to\QtWaveformViewer.exe" \
	-WindeployqtPath "C:\Qt\6.6.3\mingw_64\bin\windeployqt.exe"
```

## 分发建议

- 直接把 dist/QtWaveformViewer-win64.zip 发给他人。
- 或把 dist/QtWaveformViewer 整个目录打包分发。
- 接收方无需安装 Qt，即可运行目录中的 QtWaveformViewer.exe。

## 注意

- 必须在与编译器匹配的 Qt 环境下执行 windeployqt。
- windeployqt 的 Qt 主版本必须和你的 exe 一致（Qt6 程序要用 Qt6 的 windeployqt）。
- 若目标机器缺少 VC 运行库，脚本使用 --compiler-runtime 会一并拷贝常见运行时依赖。
