[CmdletBinding()]
param(
    [string]$GameRoot = 'E:/Program Files (x86)/Steam/steamapps/common/Palworld',
    [string]$OutputDirectory,
    [string]$NativeBuildDirectory,
    [switch]$AllowUninstalledBuild # Preview packaging; does not claim game validation.
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$OutputDirectory) { $OutputDirectory = Join-Path $root ('dist/public-v0.3.17-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
if (!$NativeBuildDirectory) { $NativeBuildDirectory = Join-Path $root 'build-processor' }
$out = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $out) { throw 'Use a new empty output path; existing release packages are not overwritten.' }
$installed = Join-Path $GameRoot 'Mods/NativeMods/UE4SS/Mods'
& (Join-Path $PSScriptRoot 'generate_localizations.ps1') -Check
$pakNames = @{
    PalResourceFactoryProcessor = @('PalResourceFactoryProcessor_P.pak','PalResourceFactoryProcessorIcons_P.pak')
    PalResourceFactoryAncientBreeder = @('PalResourceFactoryBreederVisual_P.pak','PalResourceFactoryAncientBreederIcons_P.pak')
}
$dllPaths = @{
    PalResourceFactoryProcessor = 'processor/main.dll'
    PalResourceFactoryAncientBreeder = 'ancient-breeder/main.dll'
}
$files = [Collections.Generic.List[object]]::new()
foreach ($name in @('PalResourceFactoryProcessor','PalResourceFactoryAncientBreeder')) {
    $dll = Join-Path $NativeBuildDirectory $dllPaths[$name]
    $liveDll = Join-Path $installed "$name/dlls/main.dll"
    if (!$AllowUninstalledBuild -and (Get-FileHash $dll).Hash -ne (Get-FileHash $liveDll).Hash) { throw "Built and installed DLL differ: $name" }
    $files.Add(@{Source=$dll; Relative="Mods/$name/dlls/main.dll"})
    $files.Add(@{Source=(Join-Path $root 'native/enabled.txt'); Relative="Mods/$name/enabled.txt"})
    $schema = Join-Path $root "src/palschema/$name"
    foreach ($part in @('buildings','translations')) {
        foreach ($file in Get-ChildItem (Join-Path $schema $part) -Recurse -File) {
            $relative = [IO.Path]::GetRelativePath($schema,$file.FullName).Replace('\','/')
            if ((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $installed "PalSchema/mods/$name/$relative")).Hash) {
                throw "Source and installed data differ: $name/$relative"
            }
            $files.Add(@{Source=$file.FullName; Relative="Mods/PalSchema/mods/$name/$relative"})
        }
    }
    foreach ($pak in $pakNames[$name]) {
        $files.Add(@{Source=(Join-Path $installed "PalSchema/mods/$name/paks/$pak"); Relative="Mods/PalSchema/mods/$name/paks/$pak"})
    }
}
$files.Add(@{Source=(Join-Path $root 'workshop/media/flow-cover.jpg'); Relative='thumbnail.jpg'})
foreach ($name in @('LICENSE','THIRD_PARTY_NOTICES.md','README.md')) {
    $files.Add(@{Source=(Join-Path $root $name); Relative=$name})
}
$files.Add(@{Source=(Join-Path $root 'workshop/DESCRIPTION.txt'); Relative='DESCRIPTION.txt'})
$hashes = foreach ($file in $files) {
    $target = Join-Path $out $file.Relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    Copy-Item -LiteralPath $file.Source -Destination $target
    $hash = (Get-FileHash -LiteralPath $target).Hash
    if ($hash -ne (Get-FileHash -LiteralPath $file.Source).Hash) { throw "Copy verification failed: $target" }
    '{0}  {1}' -f $hash,$file.Relative
}
$info = [ordered]@{
    ModName='Pal Resource Factory / 帕魯資源工廠'
    PackageName='PalResourceFactoryMod'
    Thumbnail='thumbnail.jpg'
    Version='0.3.17-public-preview'
    DebugMode=$false
    MinRevision=102642
    Author='paul800901'
    Dependencies=@('UE4SSExperimentalPW','PalSchema')
    Tags=@('UE4SS','PalSchema','Gameplay')
    # UE4SS installs under Mods/NativeMods/UE4SS; only our scoped Mods subtree
    # is targeted. No UE4SS.dll, mods.txt, global config, or PalSchema DLL.
    InstallRule=@(@{Type='UE4SS';Targets=@('./Mods')})
}
[IO.File]::WriteAllText((Join-Path $out 'Info.json'),($info | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
$hashes += '{0}  Info.json' -f (Get-FileHash (Join-Path $out 'Info.json')).Hash
[IO.File]::WriteAllLines((Join-Path $out 'SHA256SUMS.txt'),$hashes,[Text.UTF8Encoding]::new($false))
Compress-Archive -Path (Join-Path $out '*') -DestinationPath ($out+'.zip') -CompressionLevel Optimal
Write-Output "Package: $out"
Write-Output "Archive: $out.zip"
Write-Output "Verified $($files.Count) copied files against their sources."
