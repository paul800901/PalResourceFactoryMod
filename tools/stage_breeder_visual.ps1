[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$log=Get-Content -LiteralPath "$project/build/breeder-visual-cook.log" -Raw
if ($log -notmatch 'Success - 0 error\(s\)' -or $log -match 'Failed to compile Material|valid ShaderMap') { throw 'Breeder visual cook did not pass' }
$cooked="$project/unreal/Saved/Cooked/Windows/PalResourceFactory/Content/PalResourceFactory/BreederVisual"
$stage="$project/build-processor/breeder-visual-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $stage | Out-Null
$files=@(Get-ChildItem -LiteralPath $cooked -File -Recurse | Sort-Object FullName)
if (!$files.Count -or @($files | Where-Object Extension -NotIn '.uasset','.uexp','.ubulk').Count) { throw 'Unexpected visual package inputs' }
$lines=foreach($f in $files) {
    $relative=$f.FullName.Substring($cooked.Length+1).Replace('\','/')
    '"{0}" "../../../Pal/Content/PalResourceFactory/BreederVisual/{1}"' -f $f.FullName,$relative
}
$response="$stage/files.txt"
[IO.File]::WriteAllLines($response,$lines,[Text.UTF8Encoding]::new($false))
$pak="$stage/PalResourceFactoryBreederVisual_P.pak"
$tool='D:/PalSkillDPSAnalyzer/.toolchain/UnrealEngine-5.1/Engine/Binaries/Win64/UnrealPak.exe'
& $tool $pak "-Create=$response" -compress
if ($LASTEXITCODE) { throw 'Visual PAK failed' }
$list=& $tool $pak -List
if ($LASTEXITCODE -or ($list -join "`n") -notmatch 'Mount point ../../../Pal/Content/PalResourceFactory/BreederVisual/') { throw 'Visual PAK readback failed' }
if (@($list | Where-Object { $_ -match '^LogPakFile: Display: "' }).Count -ne $files.Count) { throw 'Visual PAK file count differs' }
Copy-Item -LiteralPath "$project/build-processor/ancient-breeder/main.dll" -Destination "$stage/main.dll"
Get-FileHash -LiteralPath $pak,"$stage/main.dll" | Format-List Path,Hash
