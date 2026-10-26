[CmdletBinding()]
param(
    [string]$GameRoot = 'E:\Program Files (x86)\Steam\steamapps\common\Palworld'
)

$ErrorActionPreference = 'Stop'
$resolvedGameRoot = [System.IO.Path]::GetFullPath($GameRoot)
$ue4ssRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedGameRoot 'Mods\NativeMods\UE4SS'))
$modsRoot = [System.IO.Path]::GetFullPath((Join-Path $ue4ssRoot 'Mods'))
$schemaModsRoot = [System.IO.Path]::GetFullPath((Join-Path $modsRoot 'PalSchema\mods'))
$targets = @(
    [System.IO.Path]::GetFullPath((Join-Path $modsRoot 'PalResourceFactoryNative')),
    [System.IO.Path]::GetFullPath((Join-Path $schemaModsRoot 'PalResourceFactory'))
)

foreach ($processName in @('Palworld-Win64-Shipping', 'PalServer-Win64-Shipping', 'PalServer')) {
    if (Get-Process -Name $processName -ErrorAction SilentlyContinue) {
        throw "Close $processName before uninstalling the test build."
    }
}
foreach ($target in $targets) {
    $insideNative = $target.StartsWith($modsRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
    $insideSchema = $target.StartsWith($schemaModsRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
    if (-not ($insideNative -or $insideSchema)) {
        throw "Resolved target escaped the intended UE4SS directories: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
        Write-Host "Removed: $target"
    }
}
