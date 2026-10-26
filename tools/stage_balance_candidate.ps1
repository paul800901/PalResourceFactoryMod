param([switch]$Install)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$mods='E:/Program Files (x86)/Steam/steamapps/common/Palworld/Mods/NativeMods/UE4SS/Mods'
$stage=Join-Path $project ('build-processor/balance-'+(Get-Date -Format yyyyMMdd-HHmmss))
$cook=Join-Path $project 'unreal/Saved/Cooked/Windows/PalResourceFactory/Content/PalResourceFactory/Icons'
$paktool='D:/PalSkillDPSAnalyzer/.toolchain/UnrealEngine-5.1/Engine/Binaries/Win64/UnrealPak.exe'
if ((Get-Content "$project/build/icons-cook.log" -Raw) -notmatch 'Success - 0 error\(s\)') { throw 'Icon cook did not pass' }
New-Item -ItemType Directory -Path $stage | Out-Null
$targets=@()
foreach($item in @(
    @('PalResourceFactoryProcessor','processor','PRF_EggResourceProcessor'),
    @('PalResourceFactoryAncientBreeder','breeder','PRF_ResourceBreedingFacility')
)) {
    $mod,$file,$id=$item
    $schema="$project/src/palschema/$mod/buildings/$file.json"
    $row=(Get-Content $schema -Raw | ConvertFrom-Json).$id
    if($row.Name -match '測試' -or $row.Description -match '測試|免電' -or $row.BuildingData.TypeUIDisplay -ne 'PalTraining') { throw 'Presentation readback failed' }
    if($row.Technology.IconName -ne $id -or $row.BuildingData.RequiredEnergyType -ne 'Electric') { throw 'Icon or power readback failed' }
    $files=@(Get-ChildItem -LiteralPath $cook -File | Where-Object BaseName -eq $id)
    if($files.Count -lt 2) { throw "Missing cooked icon $id" }
    $response="$stage/$file-files.txt"
    $lines=$files | ForEach-Object { '"{0}" "../../../Pal/Content/PalResourceFactory/Icons/{1}"' -f $_.FullName,$_.Name }
    [IO.File]::WriteAllLines($response,$lines,[Text.UTF8Encoding]::new($false))
    $pak="$stage/${mod}Icons_P.pak"
    & $paktool $pak "-Create=$response" -compress | Out-Null
    if($LASTEXITCODE) { throw 'Icon pack failed' }
    $listing=& $paktool $pak -List
    if($LASTEXITCODE -or ($listing -join "`n") -notmatch "$id.uasset") { throw 'Icon pack readback failed' }
    $binary=if($file -eq 'processor') { 'processor' } else { 'ancient-breeder' }
    Copy-Item "$project/build-processor/$binary/main.dll" "$stage/$file.dll"
    Copy-Item $schema "$stage/$file.json"
    $targets+=,@("$stage/$file.dll","$mods/$mod/dlls/main.dll")
    $targets+=,@("$stage/$file.json","$mods/PalSchema/mods/$mod/buildings/$file.json")
    $targets+=,@($pak,"$mods/PalSchema/mods/$mod/paks/${mod}Icons_P.pak")
}
if($Install) {
    if(Get-Process -Name 'Palworld-Win64-Shipping','Palworld' -ErrorAction SilentlyContinue) { throw 'Game is running; installation blocked' }
    $backup="$stage/backup"
    New-Item -ItemType Directory -Path $backup | Out-Null
    foreach($pair in $targets) {
        $relative=$pair[1].Substring($mods.Length+1)
        if(Test-Path -LiteralPath $pair[1]) {
            $old=Join-Path $backup $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $old) -Force | Out-Null
            Copy-Item -LiteralPath $pair[1] -Destination $old
        }
        Copy-Item -LiteralPath $pair[0] -Destination $pair[1]
        if((Get-FileHash $pair[0]).Hash -ne (Get-FileHash $pair[1]).Hash) { throw 'Installed file differs' }
    }
}
$targets | ForEach-Object { Get-FileHash -LiteralPath $_[0] } | Format-Table Path,Hash
Write-Output "Stage=$stage Installed=$Install GameVerified=False"
