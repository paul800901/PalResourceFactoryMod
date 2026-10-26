[CmdletBinding()]
param([switch]$PakOnly)

$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$cooked = Join-Path $project 'unreal\Saved\Cooked\Windows\PalResourceFactory\Content\PalResourceFactory\Processor'
$binary = Join-Path $project 'build-processor\processor\main.dll'
$data = Join-Path $project 'src\palschema\PalResourceFactoryProcessor\buildings\processor.json'
$pakTool = 'D:\PalSkillDPSAnalyzer\.toolchain\UnrealEngine-5.1\Engine\Binaries\Win64\UnrealPak.exe'
$cookLog = Get-Content -LiteralPath (Join-Path $project 'build\processor-cook.log') -Raw
if ($cookLog -notmatch 'Success - 0 error\(s\)' -or $cookLog -match 'Failed to compile Material|Missing Sine input|valid ShaderMap') {
    throw 'Processor cook did not pass, or a material would fall back in game.'
}
foreach ($required in (@($cooked, $data, $pakTool) + $(if (-not $PakOnly) { @($binary) } else { @() }))) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing processor input: $required" }
}
$rows = Get-Content -LiteralPath $data -Raw | ConvertFrom-Json
if (@($rows.PSObject.Properties).Count -ne 1 -or -not $rows.PRF_EggResourceProcessor) {
    throw 'The package must contain only the first processor building.'
}
$stage = Join-Path $project ('build-processor\staged-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$native = Join-Path $stage 'UE4SS\Mods\PalResourceFactoryProcessor'
$schema = Join-Path $stage 'UE4SS\Mods\PalSchema\mods\PalResourceFactoryProcessor'
New-Item -ItemType Directory -Path (Join-Path $native 'dlls'), (Join-Path $schema 'buildings'), (Join-Path $schema 'paks') | Out-Null
if (-not $PakOnly) { Copy-Item -LiteralPath $binary -Destination (Join-Path $native 'dlls\main.dll') }
[IO.File]::WriteAllText((Join-Path $native 'enabled.txt'), '')
Copy-Item -LiteralPath $data -Destination (Join-Path $schema 'buildings\processor.json')
Copy-Item -LiteralPath (Join-Path $project 'docs\PROCESSOR_TEST_20260906.md') -Destination (Join-Path $stage 'README.md')

$files = @(Get-ChildItem -LiteralPath $cooked -File -Recurse | Sort-Object FullName)
if ($files.Count -eq 0 -or @($files | Where-Object Extension -NotIn '.uasset', '.uexp', '.ubulk').Count) {
    throw 'Unexpected cooked processor files.'
}
$response = Join-Path $stage 'processor-pak-files.txt'
$lines = foreach ($file in $files) {
    $relative = $file.FullName.Substring($cooked.Length + 1).Replace('\', '/')
    '"{0}" "../../../Pal/Content/PalResourceFactory/Processor/{1}"' -f $file.FullName, $relative
}
[IO.File]::WriteAllLines($response, $lines, [Text.UTF8Encoding]::new($false))
$pak = Join-Path $schema 'paks\PalResourceFactoryProcessor_P.pak'
& $pakTool $pak "-Create=$response" -compress
if ($LASTEXITCODE -ne 0) { throw 'Processor PAK creation failed.' }
$listing = & $pakTool $pak -List
if ($LASTEXITCODE -ne 0) { throw 'Processor PAK readback failed.' }
$text = $listing -join "`n"
if ($text -notmatch [regex]::Escape('Mount point ../../../Pal/Content/PalResourceFactory/Processor/')) {
    throw 'Processor PAK has an unexpected mount point.'
}
$entries = @($listing | Where-Object { $_ -match '^LogPakFile: Display: "' })
if ($entries.Count -ne $files.Count -or $text -notmatch 'BP_PRF_EggProcessor.uasset') {
    throw 'Processor PAK entry readback differs.'
}
$receipt = [ordered]@{
    status = 'local-build-only-not-installed-not-game-verified'
    native_sha256 = $(if (-not $PakOnly) { (Get-FileHash -LiteralPath (Join-Path $native 'dlls\main.dll') -Algorithm SHA256).Hash } else { $null })
    pak_sha256 = (Get-FileHash -LiteralPath $pak -Algorithm SHA256).Hash
    asset_files = $files.Count
    building_ids = @('PRF_EggResourceProcessor')
    game_files_changed = $false
    executable_sha256 = (Get-FileHash -LiteralPath 'E:\Program Files (x86)\Steam\steamapps\common\Palworld\Pal\Binaries\Win64\Palworld-Win64-Shipping.exe' -Algorithm SHA256).Hash
}
[IO.File]::WriteAllText((Join-Path $stage 'build-receipt.json'), ($receipt | ConvertTo-Json -Depth 4), [Text.UTF8Encoding]::new($false))
Write-Output "Staged processor only; game files unchanged: $stage"
