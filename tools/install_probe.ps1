[CmdletBinding()]
param([switch]$SettlementTest, [string]$ReplaceDllSha256 = '')

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$gameRoot = 'E:\Program Files (x86)\Steam\steamapps\common\Palworld'
$gameExe = Join-Path $gameRoot 'Pal\Binaries\Win64\Palworld-Win64-Shipping.exe'
$modsRoot = Join-Path $gameRoot 'Mods\NativeMods\UE4SS\Mods'
$moduleName = 'PalResourceFactoryProbe'
$version = '0.1.0-manual-preview'
$buildRoot = Join-Path $projectRoot 'build-probe'
$dllSource = Join-Path $buildRoot 'probe\main.dll'
$markerSource = Join-Path $projectRoot 'native\probe\enabled.txt'
$instructions = Join-Path $projectRoot 'docs\MANUAL_PROBE.md'
if ($SettlementTest) {
    $moduleName = 'PalResourceFactorySettlementTest'
    $version = '0.2.0-manual-ground-test'
    $buildRoot = Join-Path $projectRoot 'build-settlement-test'
    $dllSource = Join-Path $buildRoot 'settlement-test\main.dll'
    $markerSource = Join-Path $projectRoot 'native\settlement-test\enabled.txt'
    $instructions = Join-Path $projectRoot 'docs\MANUAL_SETTLEMENT_TEST.md'
}
$destination = Join-Path $modsRoot $moduleName
$expectedGameHash = 'FE3C15064524BAE1947852467C4F92BC22469ACC033A3D3C8031EAB4324E41E8'

function Assert-GameClosed {
    $running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match '^(Palworld|PalServer)(-|$)'
    })
    if ($running.Count) { throw 'Close Palworld and PalServer before installing the manual experiment.' }
}

Assert-GameClosed
foreach ($required in @($gameExe, $modsRoot, $dllSource, $markerSource, $instructions)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing required path: $required" }
}
if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $expectedGameHash) {
    throw 'The game executable differs from the inspected version. Nothing was installed.'
}
$resolvedDestination = [IO.Path]::GetFullPath($destination)
$expectedDestination = [IO.Path]::GetFullPath($modsRoot).TrimEnd('\') + '\' + $moduleName
if ($resolvedDestination -ne $expectedDestination) { throw 'Unexpected destination.' }
$modsList = Join-Path $modsRoot 'mods.txt'
$modsListHash = (Get-FileHash -LiteralPath $modsList -Algorithm SHA256).Hash
$installedDll = Join-Path $destination 'dlls\main.dll'
$upgrading = Test-Path -LiteralPath $destination
$backup = $null
if ($upgrading) {
    # Require the inspected exact previous DLL. Preserve all per-egg receipts.
    if ($ReplaceDllSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        (Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $ReplaceDllSha256) {
        throw 'Existing DLL was not explicitly identified by its current SHA256. Nothing was installed.'
    }
    $backup = Join-Path $buildRoot ('install-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path (Join-Path $backup 'dlls') | Out-Null
    foreach ($relative in @('dlls\main.dll', 'README.md', 'enabled.txt')) {
        $previous = Join-Path $destination $relative
        if (Test-Path -LiteralPath $previous) {
            $copy = Join-Path $backup $relative
            Copy-Item -LiteralPath $previous -Destination $copy
            if ((Get-FileHash -LiteralPath $previous).Hash -ne (Get-FileHash -LiteralPath $copy).Hash) {
                throw 'Previous installation backup readback differs. Nothing was installed.'
            }
        }
    }
    $previousReceipt = Join-Path $buildRoot 'install-receipt.json'
    if (Test-Path -LiteralPath $previousReceipt) {
        Copy-Item -LiteralPath $previousReceipt -Destination (Join-Path $backup 'install-receipt.json')
    }
} elseif ($ReplaceDllSha256) {
    throw 'Replacement requested but the previous installation is absent.'
}
Assert-GameClosed
if ($upgrading) {
    if ((Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $ReplaceDllSha256) {
        throw 'Installed DLL changed after inspection. Nothing was installed.'
    }
} else {
    New-Item -ItemType Directory -Path (Join-Path $destination 'dlls') | Out-Null
}
Copy-Item -LiteralPath $dllSource -Destination $installedDll
Copy-Item -LiteralPath $instructions -Destination (Join-Path $destination 'README.md')
$sourceHash = (Get-FileHash -LiteralPath $dllSource -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $sourceHash) {
    throw 'Installed DLL readback differs; marker was not enabled.'
}
# UE4SS supports enabled.txt without editing mods.txt. Activate only after DLL readback.
Copy-Item -LiteralPath $markerSource -Destination (Join-Path $destination 'enabled.txt')
if ((Get-FileHash -LiteralPath $modsList -Algorithm SHA256).Hash -ne $modsListHash) {
    throw 'mods.txt changed during installation; inspect external activity.'
}
$receipt = [ordered]@{
    installedAt = (Get-Date).ToString('o')
    status = 'installed_not_game_verified'
    module = $moduleName
    version = $version
    destination = $destination
    dllSha256 = $sourceHash
    gameExeSha256 = $expectedGameHash
    modsTxtSha256BeforeAndAfter = $modsListHash
    filesWritten = @('dlls\main.dll', 'README.md', 'enabled.txt')
    upgradedExistingModule = $upgrading
    previousDllSha256 = $ReplaceDllSha256
    backupDirectory = $backup
    settlementsPreserved = $true
    materialOutput = $(if ($SettlementTest) { 'vanilla_ground_drop_request' } else { 'none' })
    eggRemovalOnHotkey = [bool]$SettlementTest
    itemAwardOnHotkey = [bool]$SettlementTest
    settlementExecuted = $false
    gameLaunchPerformed = $false
    fullModReady = $false
}
$receiptPath = Join-Path $buildRoot 'install-receipt.json'
$receipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $receiptPath -Encoding utf8
Write-Output "Installed manual experiment (not game verified): $destination"
Write-Output "Readback receipt: $receiptPath"
