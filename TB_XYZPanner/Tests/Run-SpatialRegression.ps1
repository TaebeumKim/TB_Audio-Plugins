param([switch]$BuildSharedCode)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'Builds\VisualStudio2022'
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'

if ($BuildSharedCode) {
    & $msbuild (Join-Path $buildRoot 'TB_XYZPanner_SharedCode.vcxproj') /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw 'Production SharedCode build failed.' }
}

$library = Join-Path $buildRoot 'x64\Release\Shared Code\TB_XYZPanner.lib'
if (-not (Test-Path -LiteralPath $library)) { throw 'Build Release SharedCode before running the harness, or pass -BuildSharedCode.' }
$libraryTime = (Get-Item -LiteralPath $library).LastWriteTimeUtc
$newerSources = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -File |
    Where-Object { $_.Extension -in '.cpp', '.h' -and $_.LastWriteTimeUtc -gt $libraryTime }
if ($newerSources) { throw 'Production headers/sources are newer than SharedCode. Rebuild first to avoid mixing incompatible code.' }

# Global overrides take effect before the imported production C++ targets evaluate.
& $msbuild (Join-Path $PSScriptRoot 'SpatialRegression.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:ConfigurationType=Application /p:TargetName=SpatialRegression /p:TargetExt=.exe "/p:OutDir=$PSScriptRoot\out\\" "/p:IntDir=$PSScriptRoot\out\obj\\" /m /nologo /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw 'Spatial regression harness build failed.' }
& (Join-Path $PSScriptRoot 'out\SpatialRegression.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'out\last-run.log')
if ($LASTEXITCODE -ne 0) { throw 'Spatial regression checks failed.' }
