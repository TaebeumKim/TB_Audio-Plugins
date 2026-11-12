param([string]$SnapshotPrefix = 'editor')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'
$library = Join-Path $root 'Builds\VisualStudio2022\x64\Release\Shared Code\TB_XYZPanner.lib'
if (-not (Test-Path -LiteralPath $library)) { throw 'Build Release SharedCode first.' }
$newerSources = Get-ChildItem -LiteralPath (Join-Path $root 'Source') -File |
    Where-Object { $_.Extension -in '.cpp', '.h' -and $_.LastWriteTimeUtc -gt (Get-Item -LiteralPath $library).LastWriteTimeUtc }
if ($newerSources) { throw 'Rebuild production SharedCode before running the editor harness.' }

& $msbuild (Join-Path $PSScriptRoot 'EditorRegression.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:ConfigurationType=Application /p:TargetName=EditorRegression /p:TargetExt=.exe "/p:OutDir=$PSScriptRoot\out\\" "/p:IntDir=$PSScriptRoot\out\editor-obj\\" /m /nologo /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw 'Editor regression harness build failed.' }
& (Join-Path $PSScriptRoot 'out\EditorRegression.exe') $SnapshotPrefix | Tee-Object -FilePath (Join-Path $PSScriptRoot "out\$SnapshotPrefix-tests.log")
if ($LASTEXITCODE -ne 0) { throw 'Editor regression checks failed.' }
