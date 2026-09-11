[CmdletBinding()]
param(
    [ValidateSet('probe', 'read', 'publish', 'describe', 'media')][string]$Mode = 'probe',
    [string]$PackageOrId,
    [string]$Receipt
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sdk = Join-Path $root 'external/workshop-uploader'
# These three files come from Pocketpair's PalworldModUploader repository.
# Keep its steam_appid.txt (uploader app 4212470); do not impersonate another mod.
foreach ($name in @('Steamworks.NET.dll','steam_api64.dll','steam_appid.txt')) {
    if (!(Test-Path (Join-Path $sdk $name))) { throw "Missing official uploader library: $name" }
}
[System.Runtime.InteropServices.NativeLibrary]::Load((Join-Path $sdk 'steam_api64.dll')) | Out-Null
Add-Type -Path (Join-Path $sdk 'Steamworks.NET.dll')
$refs = @((Join-Path $sdk 'Steamworks.NET.dll')) + @(Get-ChildItem (Join-Path $PSHOME 'ref') -Filter '*.dll' | ForEach-Object FullName)
Add-Type -Path (Join-Path $PSScriptRoot 'Program.cs') -ReferencedAssemblies $refs
$previousDirectory = [Environment]::CurrentDirectory
try {
    [Environment]::CurrentDirectory = $sdk
    $code = [Program]::Main([string[]]@($Mode,$PackageOrId,$Receipt))
    if ($code -ne 0) { throw "Workshop operation failed (exit $code)" }
} finally { [Environment]::CurrentDirectory = $previousDirectory }
