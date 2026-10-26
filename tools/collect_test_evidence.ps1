[CmdletBinding()]
param(
    [string]$GameRoot = 'E:\Program Files (x86)\Steam\steamapps\common\Palworld'
)

$ErrorActionPreference = 'Stop'
$resolvedGameRoot = [System.IO.Path]::GetFullPath($GameRoot)
$candidateLogs = @(
    (Join-Path $resolvedGameRoot 'Pal\Binaries\Win64\UE4SS.log'),
    (Join-Path $resolvedGameRoot 'Mods\NativeMods\UE4SS\UE4SS.log')
)
$log = $candidateLogs | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ($null -eq $log) {
    throw "UE4SS.log was not found under: $resolvedGameRoot"
}

$matches = Select-String -LiteralPath $log -Pattern '\[PalResourceFactory\]' | Select-Object -Last 100
if (-not $matches) {
    throw "No PalResourceFactory entries found in: $log"
}
$matches | ForEach-Object { $_.Line }
