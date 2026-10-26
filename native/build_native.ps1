[CmdletBinding()]
param(
    [string]$SharedToolchainRoot = 'D:\PalSkillDPSAnalyzer',
    [string]$UE4SSDll = 'E:\Program Files (x86)\Steam\steamapps\common\Palworld\Mods\NativeMods\UE4SS\UE4SS.dll',
    [ValidateSet('PalResourceFactoryNative', 'PalResourceFactoryProbe', 'PalResourceFactorySettlementTest', 'PalResourceFactoryProcessor')]
    [string]$Target = 'PalResourceFactoryNative'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build-native'
if ($Target -eq 'PalResourceFactoryProbe') { $buildDirectory = Join-Path $projectRoot 'build-probe' }
if ($Target -eq 'PalResourceFactorySettlementTest') { $buildDirectory = Join-Path $projectRoot 'build-settlement-test' }
if ($Target -eq 'PalResourceFactoryProcessor') { $buildDirectory = Join-Path $projectRoot 'build-processor' }
$ue4ssSource = Join-Path $SharedToolchainRoot 'external\RE-UE4SS'
$externalRoot = Join-Path $SharedToolchainRoot 'external'
$activateToolchain = Join-Path $SharedToolchainRoot 'tools\enter_native_toolchain.ps1'

foreach ($requiredPath in @($UE4SSDll, $ue4ssSource, $externalRoot, $activateToolchain)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required local build dependency is missing: $requiredPath"
    }
}

& $activateToolchain
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null

$dumpbin = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\dumpbin.exe'
$libTool = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\lib.exe'
$definitionFile = Join-Path $buildDirectory 'UE4SS.def'
$importLibrary = Join-Path $buildDirectory 'UE4SS.lib'
$exportsFile = Join-Path $buildDirectory 'UE4SS.exports.txt'

& $dumpbin /nologo /exports $UE4SSDll | Set-Content -LiteralPath $exportsFile -Encoding ASCII
if ($LASTEXITCODE -ne 0) { throw "dumpbin failed with exit code $LASTEXITCODE" }

$exports = foreach ($line in Get-Content -LiteralPath $exportsFile) {
    if ($line -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)\s*$') { $Matches[1] }
}
if ($exports.Count -lt 100) { throw "Unexpected UE4SS export count: $($exports.Count)" }

$definition = @('LIBRARY UE4SS', 'EXPORTS') + ($exports | ForEach-Object { "    $_" })
[System.IO.File]::WriteAllLines($definitionFile, $definition, [System.Text.UTF8Encoding]::new($false))
& $libTool /nologo "/def:$definitionFile" /machine:x64 "/out:$importLibrary"
if ($LASTEXITCODE -ne 0) { throw "lib.exe failed with exit code $LASTEXITCODE" }

cmake -S $PSScriptRoot -B $buildDirectory -G Ninja `
    '-DCMAKE_BUILD_TYPE=Release' `
    "-DUE4SS_SOURCE_DIR=$ue4ssSource" `
    "-DUE4SS_IMPORT_LIBRARY=$importLibrary" `
    "-DEXTERNAL_ROOT=$externalRoot"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

cmake --build $buildDirectory --config Release --target $Target
if ($LASTEXITCODE -ne 0) { throw "Native build failed with exit code $LASTEXITCODE" }

if ($Target -eq 'PalResourceFactoryProcessor') {
    Write-Host "Processor built (not installed or game verified): $buildDirectory\processor\main.dll"
} elseif ($Target -eq 'PalResourceFactorySettlementTest') {
    Write-Host "Manual settlement test built (not installed): $buildDirectory\settlement-test\main.dll"
} elseif ($Target -eq 'PalResourceFactoryProbe') {
    Write-Host "Manual preview built (not installed): $buildDirectory\probe\main.dll"
} else {
    Write-Host "Native module built: $buildDirectory\main.dll"
}
