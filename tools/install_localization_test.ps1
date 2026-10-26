$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$mods='E:/Program Files (x86)/Steam/steamapps/common/Palworld/Mods/NativeMods/UE4SS/Mods'
& "$PSScriptRoot/generate_localizations.ps1" -Check
if(Get-Process Palworld* -ErrorAction SilentlyContinue) { throw 'Close Palworld before installing' }
$backup=Join-Path $root ('build-processor/localization-backup-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($backup) | Out-Null
$pairs=@()
$pairs+=,@("$root/build-processor/processor/main.dll","PalResourceFactoryProcessor/dlls/main.dll")
foreach($mod in @('PalResourceFactoryProcessor','PalResourceFactoryAncientBreeder')) {
    foreach($dir in @('buildings','translations')) {
        $base="$root/src/palschema/$mod"
        foreach($file in Get-ChildItem -LiteralPath "$base/$dir" -File -Recurse) {
            $relative=$file.FullName.Substring($base.Length+1).Replace('\','/')
            $pairs+=,@($file.FullName,"PalSchema/mods/$mod/$relative")
        }
    }
}
foreach($pair in $pairs) {
    $source,$relative=$pair
    $target="$mods/$relative"
    if(Test-Path -LiteralPath $target) {
        $old="$backup/$relative"
        [IO.Directory]::CreateDirectory((Split-Path -Parent $old)) | Out-Null
        Copy-Item -LiteralPath $target -Destination $old
        if((Get-FileHash $old).Hash -ne (Get-FileHash $target).Hash) { throw 'Backup mismatch' }
    }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
    if((Get-FileHash $source).Hash -ne (Get-FileHash $target).Hash) { throw "Install mismatch $relative" }
}
"Installed $($pairs.Count) files. Backup=$backup. Game UI verification pending."
