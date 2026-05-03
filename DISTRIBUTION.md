# 发布打包说明

本仓库已提供固定输出目录的打包脚本：

- 脚本：scripts/package_dist.ps1
- 包装入口：scripts/package_dist.bat
- 输出目录：dist/QtWaveformViewer
- 输出压缩包：dist/QtWaveformViewer-win64.zip

## 使用步骤

1. 先用 Release 模式编译项目，得到 QtWaveformViewer.exe。
2. 使用固定的 Qt 6.7.3 MinGW 目录执行打包脚本，不要依赖系统 PATH。
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


## 统一构建环境

仓库新增了统一环境脚本 `scripts/build_env.bat`，用于集中管理构建使用的 `QTDIR` 和 `MINGW_DIR`：

- 默认值指向 `C:\Qt\6.7.3\mingw_64` 和 `C:\Qt\Tools\mingw1120_64\bin`。
- 可通过设置环境变量 `REPO_QTDIR` 和 `REPO_MINGW_DIR` 来覆盖默认路径，或直接编辑 `scripts/build_env.bat`。

构建时和打包时的脚本会自动调用该文件来确保使用一致的 Qt + MinGW 组合，避免工具链混用导致的运行时崩溃。

## 分发建议


## 注意

