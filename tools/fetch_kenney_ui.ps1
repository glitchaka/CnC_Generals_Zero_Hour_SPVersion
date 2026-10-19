$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'ExternalAssets\KenneyUI'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$base = 'https://raw.githubusercontent.com/tatisgordon/Godot-Lobby/main/themes/kenney_ui-pack/PNG'
$files = @{
  'panel_blue.png'   = "$base/Blue/Default/button_rectangle_depth_flat.png"
  'bar_green.png'    = "$base/Green/Default/button_rectangle_depth_flat.png"
  'slot_yellow.png'  = "$base/Yellow/Default/button_rectangle_depth_flat.png"
}
foreach ($name in $files.Keys) {
  $dst = Join-Path $out $name
  if (-not (Test-Path $dst)) {
    Write-Host "Descargando Kenney UI: $name"
    Invoke-WebRequest -UseBasicParsing -Uri $files[$name] -OutFile $dst
  }
}
@'
Kenney UI Pack
Source: https://kenney-assets.itch.io/ui-pack
License: CC0 1.0 Universal
'@ | Set-Content -Encoding UTF8 (Join-Path $out 'SOURCE.txt')
Write-Host 'Kenney UI listo.'
