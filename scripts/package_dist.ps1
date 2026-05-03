param(
    [string]$ExePath,
    [string]$PackageName = "QtWaveformViewer",
    [string]$DistRoot = "dist",
    [switch]$SkipZip,
    [string]$WindeployqtPath,
    [string]$QtDir = "C:\Qt\6.7.3\mingw_64",
    [switch]$KeepTemp
)

$ErrorActionPreference = "Stop"

function Resolve-AbsolutePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Resolve-Path -LiteralPath $Path).Path
}

function Find-Windeployqt {
    param(
        [string]$UserProvidedPath,
        [string]$FixedQtDir
    )

    if ($UserProvidedPath) {
        $resolved = Resolve-Path -LiteralPath $UserProvidedPath -ErrorAction Stop
        return $resolved.Path
    }

    if ($FixedQtDir) {
        $candidate = Join-Path $FixedQtDir "bin\windeployqt.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $cmd = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $qmakeCmd = Get-Command qmake -ErrorAction SilentlyContinue
    if ($qmakeCmd) {
        $qmakeDir = Split-Path -Path $qmakeCmd.Source -Parent
        $candidate = Join-Path $qmakeDir "windeployqt.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Find-ReleaseExe {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$TargetName
    )

    $all = Get-ChildItem -Path $RepoRoot -Filter "$TargetName.exe" -Recurse -File |
        Where-Object {
            $_.FullName -notmatch "\\dist\\" -and
            $_.FullName -notmatch "\\debug\\"
        } |
        Sort-Object LastWriteTime -Descending

    if (-not $all) {
        return $null
    }

    return $all[0].FullName
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-AbsolutePath -Path (Join-Path $scriptDir "..")

if (-not $ExePath) {
    $ExePath = Find-ReleaseExe -RepoRoot $repoRoot -TargetName $PackageName
    if (-not $ExePath) {
        throw "Cannot find $PackageName.exe. Build in Release mode first, or pass -ExePath."
    }
}

$resolvedExePath = Resolve-AbsolutePath -Path $ExePath
if (-not (Test-Path -LiteralPath $resolvedExePath)) {
    throw "Executable does not exist: $resolvedExePath"
}

$windeployqt = Find-Windeployqt -UserProvidedPath $WindeployqtPath -FixedQtDir $QtDir
if (-not $windeployqt) {
    throw "Cannot find windeployqt under $QtDir. Check that the Qt 6.7.3 MinGW toolchain is installed at that path, or pass -WindeployqtPath explicitly."
}

$distRootPath = Join-Path $repoRoot $DistRoot
$packageDir = Join-Path $distRootPath $PackageName
$distExePath = Join-Path $packageDir "$PackageName.exe"
$tempDeployDir = Join-Path $env:TEMP "$PackageName-deploy"
$tempExePath = Join-Path $tempDeployDir "$PackageName.exe"

if (Test-Path -LiteralPath $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}

if (Test-Path -LiteralPath $tempDeployDir) {
    Remove-Item -LiteralPath $tempDeployDir -Recurse -Force
}
New-Item -ItemType Directory -Path $tempDeployDir -Force | Out-Null
Copy-Item -LiteralPath $resolvedExePath -Destination $tempExePath -Force

Write-Host "[1/4] Copied EXE to temp deploy path: $tempExePath"
Write-Host "[2/4] Running windeployqt: $windeployqt"
$stdoutLog = Join-Path $env:TEMP "$PackageName-windeployqt-stdout.log"
$stderrLog = Join-Path $env:TEMP "$PackageName-windeployqt-stderr.log"

if (Test-Path -LiteralPath $stdoutLog) {
    Remove-Item -LiteralPath $stdoutLog -Force
}
if (Test-Path -LiteralPath $stderrLog) {
    Remove-Item -LiteralPath $stderrLog -Force
}

$proc = Start-Process -FilePath $windeployqt `
    -ArgumentList @("--release", "--compiler-runtime", $tempExePath) `
    -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $stdoutLog `
    -RedirectStandardError $stderrLog

$deployText = ""
if (Test-Path -LiteralPath $stdoutLog) {
    $deployText += (Get-Content -LiteralPath $stdoutLog -Raw)
}
if (Test-Path -LiteralPath $stderrLog) {
    $deployText += (Get-Content -LiteralPath $stderrLog -Raw)
}
if ($deployText) {
    Write-Host $deployText
}

if ($proc.ExitCode -ne 0) {
    if ($deployText -match "does not seem to be a Qt executable") {
        throw "windeployqt failed with exit code: $($proc.ExitCode). The selected windeployqt likely does not match your app Qt version. Use -WindeployqtPath to point to the same Qt major version used to build the EXE (for example, Qt6 windeployqt for a Qt6 app)."
    }
    throw "windeployqt failed with exit code: $($proc.ExitCode)"
}

if (Test-Path -LiteralPath $stdoutLog) {
    Remove-Item -LiteralPath $stdoutLog -Force
}
if (Test-Path -LiteralPath $stderrLog) {
    Remove-Item -LiteralPath $stderrLog -Force
}

New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
Copy-Item -Path (Join-Path $tempDeployDir "*") -Destination $packageDir -Recurse -Force
Write-Host "[3/4] Copied deployed files to dist: $packageDir"

if (-not $SkipZip) {
    $zipPath = Join-Path $distRootPath "$PackageName-win64.zip"
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    Write-Host "[4/4] Creating zip: $zipPath"
    Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Done: $packageDir"
    Write-Host "Done: $zipPath"
} else {
    Write-Host "[4/4] Skip zip. Done: $packageDir"
}

if (-not $KeepTemp -and (Test-Path -LiteralPath $tempDeployDir)) {
    Remove-Item -LiteralPath $tempDeployDir -Recurse -Force
}