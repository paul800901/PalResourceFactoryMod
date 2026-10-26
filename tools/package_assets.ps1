[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$cookedRoot = Join-Path $projectRoot 'unreal\Saved\Cooked\Windows\PalResourceFactory\Content\PalResourceFactory\Buildings'
$pakRoot = Join-Path $projectRoot 'src\palschema\PalResourceFactory\paks'
$pakPath = Join-Path $pakRoot 'PalResourceFactory_P.pak'
$responsePath = Join-Path $projectRoot 'build\PalResourceFactory-assets.txt'
$unrealPak = 'D:\PalSkillDPSAnalyzer\.toolchain\UnrealEngine-5.1\Engine\Binaries\Win64\UnrealPak.exe'
$assetNames = @(
    'BP_PRF_EggResourceProcessor.uasset',
    'BP_PRF_EggResourceProcessor.uexp',
    'BP_PRF_ResourceBreedingFacility.uasset',
    'BP_PRF_ResourceBreedingFacility.uexp'
)

if (-not (Test-Path -LiteralPath $unrealPak -PathType Leaf)) {
    throw "UnrealPak was not found: $unrealPak"
}

New-Item -ItemType Directory -Path $pakRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $responsePath) -Force | Out-Null

$responseLines = foreach ($assetName in $assetNames) {
    $source = Join-Path $cookedRoot $assetName
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Cooked building asset is missing: $source"
    }
    $mount = "../../../Pal/Content/PalResourceFactory/Buildings/$assetName"
    '"{0}" "{1}"' -f $source, $mount
}
[System.IO.File]::WriteAllLines($responsePath, $responseLines, [System.Text.UTF8Encoding]::new($false))

& $unrealPak $pakPath "-Create=$responsePath" '-compress'
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $pakPath -PathType Leaf)) {
    throw 'UnrealPak did not produce the PalResourceFactory asset package'
}

$listing = & $unrealPak $pakPath '-List'
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to inspect the generated PalResourceFactory asset package'
}
$listingText = $listing -join "`n"
if ($listingText -notmatch [regex]::Escape('Mount point ../../../Pal/Content/PalResourceFactory/Buildings/')) {
    throw 'Generated package has the wrong Unreal mount point'
}
foreach ($assetName in $assetNames) {
    if ($listingText -notmatch [regex]::Escape($assetName)) {
        throw "Generated package is missing $assetName"
    }
}
if (($listing | Where-Object { $_ -match '^LogPakFile: Display: "BP_PRF_' }).Count -ne 4) {
    throw 'Generated package does not contain exactly four building asset files'
}

Write-Host "Packaged four independent building files: $pakPath"
