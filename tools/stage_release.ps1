[CmdletBinding()]
param(
    [switch]$EnableNative
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$nativeDll = Join-Path $projectRoot 'build-native\main.dll'
$releaseRoot = Join-Path $projectRoot 'dist\PalResourceFactory'
$nativeTarget = Join-Path $releaseRoot 'Mods\PalResourceFactoryNative'
$schemaTarget = Join-Path $releaseRoot 'Mods\PalSchema\mods\PalResourceFactory'
$expectedReleaseRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'dist\PalResourceFactory'))

& (Join-Path $PSScriptRoot 'validate_project.ps1')
if (-not (Test-Path -LiteralPath $nativeDll -PathType Leaf)) {
    throw 'Native DLL is missing. Run native\build_native.ps1 first.'
}

if ([System.IO.Path]::GetFullPath($releaseRoot) -ne $expectedReleaseRoot) {
    throw 'Resolved release target escaped the expected dist directory'
}
if (Test-Path -LiteralPath $releaseRoot) {
    Remove-Item -LiteralPath $releaseRoot -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $nativeTarget 'dlls') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $schemaTarget 'buildings') -Force | Out-Null

Copy-Item -LiteralPath $nativeDll -Destination (Join-Path $nativeTarget 'dlls\main.dll') -Force
if ($EnableNative) {
    Copy-Item -LiteralPath (Join-Path $projectRoot 'native\enabled.txt') -Destination (Join-Path $nativeTarget 'enabled.txt') -Force
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'src\palschema\PalResourceFactory\README.md') -Destination (Join-Path $schemaTarget 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'src\palschema\PalResourceFactory\buildings\facilities.json') -Destination (Join-Path $schemaTarget 'buildings\facilities.json') -Force

@{
    Name = 'PalResourceFactory'
    Version = '0.3.5-login-isolation-data-only'
    NativeEnabled = [bool]$EnableNative
    CustomPakIncluded = $false
    Purpose = 'Login isolation: no power, one Wood, both original interactive Blueprints, no custom PAK, native disabled by default'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $releaseRoot 'test-build.json') -Encoding UTF8

Write-Host "Staged login-isolation preview: $releaseRoot (native enabled: $([bool]$EnableNative))"
