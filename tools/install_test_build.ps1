[CmdletBinding()]
param(
    [string]$GameRoot = 'E:\Program Files (x86)\Steam\steamapps\common\Palworld',
    [switch]$EnableNative,
    [switch]$AllowQuarantinedBuild
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not $AllowQuarantinedBuild) {
    throw '0.3.5 is quarantined: the original incubator Blueprints spin on custom building IDs and can disturb world simulation. Do not reinstall it; wait for the next test build.'
}
$resolvedGameRoot = [System.IO.Path]::GetFullPath($GameRoot)
$ue4ssRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedGameRoot 'Mods\NativeMods\UE4SS'))
$modsRoot = [System.IO.Path]::GetFullPath((Join-Path $ue4ssRoot 'Mods'))
$schemaModsRoot = [System.IO.Path]::GetFullPath((Join-Path $modsRoot 'PalSchema\mods'))
$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'dist\PalResourceFactory'))
$nativeSource = Join-Path $releaseRoot 'Mods\PalResourceFactoryNative'
$schemaSource = Join-Path $releaseRoot 'Mods\PalSchema\mods\PalResourceFactory'
$nativeTarget = [System.IO.Path]::GetFullPath((Join-Path $modsRoot 'PalResourceFactoryNative'))
$schemaTarget = [System.IO.Path]::GetFullPath((Join-Path $schemaModsRoot 'PalResourceFactory'))

if (-not (Test-Path -LiteralPath $ue4ssRoot -PathType Container)) {
    throw "UE4SS root not found: $ue4ssRoot"
}
foreach ($processName in @('Palworld-Win64-Shipping', 'PalServer-Win64-Shipping', 'PalServer')) {
    if (Get-Process -Name $processName -ErrorAction SilentlyContinue) {
        throw "Close $processName before installing the test build."
    }
}

& (Join-Path $projectRoot 'native\build_native.ps1')
& (Join-Path $PSScriptRoot 'stage_release.ps1') -EnableNative:$EnableNative

foreach ($source in @($nativeSource, $schemaSource)) {
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        throw "Staged source is missing: $source"
    }
}
if (-not $nativeTarget.StartsWith($modsRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Resolved native target escaped the UE4SS Mods directory'
}
if (-not $schemaTarget.StartsWith($schemaModsRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Resolved PalSchema target escaped the PalSchema mods directory'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupRoot = Join-Path $projectRoot "dist\backups\$stamp"
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

function Install-IsolatedDirectory {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Target,
        [Parameter(Mandatory)][string]$BackupName
    )
    if (Test-Path -LiteralPath $Target) {
        Copy-Item -LiteralPath $Target -Destination (Join-Path $backupRoot $BackupName) -Recurse -Force
        Remove-Item -LiteralPath $Target -Recurse -Force
    }
    Copy-Item -LiteralPath $Source -Destination $Target -Recurse -Force
}

Install-IsolatedDirectory -Source $nativeSource -Target $nativeTarget -BackupName 'PalResourceFactoryNative'
Install-IsolatedDirectory -Source $schemaSource -Target $schemaTarget -BackupName 'PalResourceFactory-PalSchema'

$dll = Join-Path $nativeTarget 'dlls\main.dll'
$buildingData = Join-Path $schemaTarget 'buildings\facilities.json'
foreach ($installedFile in @($dll, $buildingData)) {
    if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
        throw "Installed test file is missing: $installedFile"
    }
}
$nativeMarker = Join-Path $nativeTarget 'enabled.txt'
$customPak = Join-Path $schemaTarget 'paks\PalResourceFactory_P.pak'
if ((Test-Path -LiteralPath $nativeMarker) -ne [bool]$EnableNative) {
    throw 'Installed native enabled marker does not match the requested isolation mode'
}
if (Test-Path -LiteralPath $customPak) {
    throw 'Custom Blueprint PAK must not be present in the login-isolation build'
}

$receipt = @{
    InstalledAt = (Get-Date).ToString('o')
    Version = '0.3.5-login-isolation-data-only'
    NativeEnabled = [bool]$EnableNative
    CustomPakInstalled = $false
    GameRoot = $resolvedGameRoot
    NativeDll = $dll
    NativeDllSha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    BuildingData = $buildingData
    BuildingDataSha256 = (Get-FileHash -LiteralPath $buildingData -Algorithm SHA256).Hash
    BackupRoot = $backupRoot
}
$receiptPath = Join-Path $projectRoot 'dist\test-install-receipt.json'
$receipt | ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8

Write-Host "Installed login-isolation preview: $receiptPath"
Write-Warning 'This data-only isolation build intentionally disables PalResourceFactoryNative and omits the custom Blueprint PAK. It can test login, technology placement, interaction, parent assignment, one-Wood recipes, and no-power behavior only.'
