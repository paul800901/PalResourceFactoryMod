[CmdletBinding()]
param(
    [string]$SharedKitRoot = 'D:\PalSkillDPSAnalyzer\.toolchain\PalworldModdingKit'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$unrealRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'unreal'))
$palSourceTarget = [System.IO.Path]::GetFullPath((Join-Path $SharedKitRoot 'Source\Pal'))
$loaderSourceTarget = [System.IO.Path]::GetFullPath((Join-Path $SharedKitRoot 'Source\PalModLoader'))
$pluginsTarget = [System.IO.Path]::GetFullPath((Join-Path $SharedKitRoot 'Plugins'))
$sourceRoot = Join-Path $unrealRoot 'Source'
$palSourceLink = Join-Path $sourceRoot 'Pal'
$loaderSourceLink = Join-Path $sourceRoot 'PalModLoader'
$pluginsLink = Join-Path $unrealRoot 'SharedPlugins'

if (-not $unrealRoot.StartsWith([System.IO.Path]::GetFullPath($projectRoot), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Resolved Unreal workspace escaped the project root'
}
foreach ($target in @($palSourceTarget, $loaderSourceTarget, $pluginsTarget)) {
    if (-not (Test-Path -LiteralPath $target -PathType Container)) {
        throw "Shared kit dependency is missing: $target"
    }
}

if (Test-Path -LiteralPath $sourceRoot) {
    $sourceItem = Get-Item -LiteralPath $sourceRoot -Force
    if ($sourceItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        Remove-Item -LiteralPath $sourceRoot -Force
    }
}
New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null

function Ensure-Junction {
    param(
        [Parameter(Mandatory)][string]$Link,
        [Parameter(Mandatory)][string]$Target
    )
    if (Test-Path -LiteralPath $Link) {
        $existing = Get-Item -LiteralPath $Link -Force
        if (-not ($existing.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Expected a junction but found a normal path: $Link"
        }
        return
    }
    New-Item -ItemType Junction -Path $Link -Target $Target | Out-Null
}

Ensure-Junction -Link $palSourceLink -Target $palSourceTarget
Ensure-Junction -Link $loaderSourceLink -Target $loaderSourceTarget
Ensure-Junction -Link $pluginsLink -Target $pluginsTarget

Write-Host "Unreal workspace prepared: $unrealRoot"
