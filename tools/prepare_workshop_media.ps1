$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'workshop/media'
[IO.Directory]::CreateDirectory($out) | Out-Null
# Format conversion only: retain the complete image and original dimensions.
Add-Type -AssemblyName System.Drawing
$codec = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
$parameters = [Drawing.Imaging.EncoderParameters]::new(1)
$parameters.Param[0] = [Drawing.Imaging.EncoderParameter]::new([Drawing.Imaging.Encoder]::Quality, [long]90)
foreach ($pair in @(
    @('workshop/media/flow-source.png','flow-cover.jpg'),
    @('art/blender/previews/processor_hero.png','processor.jpg'),
    @('art/blender/previews/breeder_hero.png','breeder.jpg')
)) {
    $source = [Drawing.Image]::FromFile((Join-Path $root $pair[0]))
    try {
        $target = Join-Path $out $pair[1]
        $source.Save($target,$codec,$parameters)
        if ((Get-Item $target).Length -ge 1000000) { throw "Preview exceeds Steam limit: $target" }
    } finally { $source.Dispose() }
}
$parameters.Dispose()
